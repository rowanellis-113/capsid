#!/usr/bin/env python3
"""End-to-end verifier for examples/binding-rpc-v2 against a running capsid-host."""

import argparse
import concurrent.futures
import json
import sys
import time
import urllib.request


def get(base_url, path, timeout=15):
    with urllib.request.urlopen(f"{base_url}{path}", timeout=timeout) as resp:
        return resp.status, dict(resp.headers.items()), resp.read()


def get_json(base_url, path, timeout=15):
    status, headers, body = get(base_url, path, timeout=timeout)
    assert status == 200, f"{path}: expected 200, got {status}: {body!r}"
    data = json.loads(body.decode("utf-8"))
    return status, headers, data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--url",
        default="http://127.0.0.1:8080",
        help="Base URL of the running capsid-host (default: http://127.0.0.1:8080)",
    )
    args = parser.parse_args()
    base = args.url.rstrip("/")

    # 1. Index
    _, _, index = get_json(base, "/")
    assert "routes" in index and len(index["routes"]) >= 14, f"unexpected index: {index}"
    print("[PASS] GET / (route index)")

    # 2. Single-request M4A / M4B / M4C / M4D JSON verification routes
    json_routes = [
        "/m4a/fair-scheduling",
        "/m4a/errors",
        "/m4a/options",
        "/m4a/quotas",
        "/m4b/readable-stream",
        "/m4b/writable-stream",
        "/m4c/http-proxies",
        "/m4d/rpc-target",
    ]
    for route in json_routes:
        _, _, payload = get_json(base, route)
        assert payload.get("ok") is True, f"{route} failed: {json.dumps(payload, indent=2)}"
        print(f"[PASS] GET {route}")

    # 3. M4B direct Binding -> User -> Host NDJSON stream passthrough
    status, headers, body = get(base, "/m4b/stream-passthrough")
    assert status == 200, f"/m4b/stream-passthrough status={status}"
    lines = [json.loads(line) for line in body.decode("utf-8").strip().splitlines()]
    assert len(lines) == 6 and [row["seq"] for row in lines] == [1, 2, 3, 4, 5, 6], lines
    print("[PASS] GET /m4b/stream-passthrough (6 streamed NDJSON chunks)")

    # 4. M4C direct Binding streaming Response return from fetch() (>64 KiB)
    status, headers, body = get(base, "/m4c/proxy-response")
    assert status == 200, f"/m4c/proxy-response status={status}"
    header_lc = {k.lower(): v for k, v in headers.items()}
    assert header_lc.get("x-binding-stream") == "vault-report", header_lc
    assert len(body) == 5 * 16384, f"expected 81920 bytes, got {len(body)}"
    for i in range(5):
        expected = ord("A") + i
        assert body[i * 16384] == expected and body[i * 16384 + 16383] == expected
    print("[PASS] GET /m4c/proxy-response (80 KiB streaming Response passthrough)")

    # 5. M4B + M4D concurrent cross-request stream & handle fencing
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        hold_future = pool.submit(get_json, base, "/m4d/fencing-hold", 15)
        time.sleep(0.15)
        steal_future = pool.submit(get_json, base, "/m4d/fencing-steal", 15)
        _, _, steal_payload = steal_future.result()
        _, _, hold_payload = hold_future.result()
    assert steal_payload.get("ok") is True, f"fencing-steal failed: {steal_payload}"
    assert hold_payload.get("ok") is True, f"fencing-hold failed: {hold_payload}"
    print("[PASS] GET /m4d/fencing-hold + /m4d/fencing-steal (cross-request isolation)")

    # 6. Deterministic request-end cleanup for undisposed RpcTarget handles
    _, _, leak_payload = get_json(base, "/m4d/request-end-leak")
    assert leak_payload.get("ok") is True, leak_payload
    _, _, verify_payload = get_json(base, "/m4d/request-end-verify")
    assert verify_payload.get("ok") is True, f"request-end-verify failed: {verify_payload}"
    print("[PASS] GET /m4d/request-end-leak -> /m4d/request-end-verify (request-end disposal)")

    print("ALL BINDING RPC v2 SHOWCASE CHECKS PASSED (12/12)")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
