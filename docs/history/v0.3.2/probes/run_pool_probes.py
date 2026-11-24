"""Build and run the independent Linux probes against an existing Capsid Ninja build."""

import argparse
import json
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("build", type=Path, help="Linux build containing compile_commands.json")
group = parser.add_mutually_exclusive_group()
group.add_argument("--recheck", action="store_true", help="run the follow-up review cases")
group.add_argument("--design", action="store_true", help="check config v2 and tenant fairness")
args = parser.parse_args()
build = args.build.resolve()
cases = ("health", "floor", "selection", "surge")
fixture = Path(__file__).with_name("pool_probes.cc")
target = "test-host-elastic-pool"
source_name = "test_host_elastic_pool.cc"
if args.recheck:
    cases = ("admission", "success", "late", "quarantine", "mixed-ledger")
    fixture = Path(__file__).with_name("recheck_pool.cc")
elif args.design:
    cases = ("within-share", "protected-overshare", "memory-inheritance")
    fixture = Path(__file__).with_name("design_probes.cc")
    target = "test-host-eviction-coordinator"
    source_name = "test_host_eviction_coordinator.cc"
fixture = fixture.resolve()
subprocess.run(["cmake", "--build", str(build), "--target", target, "capsid-worker"], check=True)
entries = json.loads((build / "compile_commands.json").read_text())
entry = next(e for e in entries if Path(e["file"]).name == source_name)
source = Path(entry["file"])
flags = shlex.split(entry["command"])
commands = subprocess.check_output(["ninja", "-t", "commands", target], cwd=build, text=True)
link_flags = shlex.split(commands.splitlines()[-1])
if link_flags[:2] == [":", "&&"] and link_flags[-2:] == ["&&", ":"]:
    link_flags = link_flags[2:-2]
with tempfile.TemporaryDirectory(prefix="capsid-independent-review-") as directory:
    obj = str(Path(directory) / "probes.o")
    exe = str(Path(directory) / "probes")
    flags[flags.index("-o") + 1] = obj
    flags[flags.index("-c") + 1] = str(fixture)
    subprocess.run([*flags, "-iquote", str(source.parent)], cwd=entry["directory"], check=True)
    link_flags[link_flags.index("-o") + 1] = exe
    index = next(i for i, arg in enumerate(link_flags) if arg.endswith("/" + source.name + ".o"))
    link_flags[index] = obj
    subprocess.run(link_flags, cwd=build, check=True)
    failed = []
    for case in cases:
        print(f"== {case} ==", flush=True)
        if subprocess.run([exe, str(build / "capsid-worker"), case], timeout=45).returncode:
            failed.append(case)
    print(
        f"{len(cases) - len(failed)}/{len(cases)} passed. Failures: {', '.join(failed)}", flush=True
    )
    sys.exit(bool(failed))
