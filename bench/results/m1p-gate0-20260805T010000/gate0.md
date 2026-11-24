# M1P gate 0 evidence (`e1b0414`)

## Source identity

- HEAD and origin/main both pointed to `e1b0414ada81ef783dc51a8fc2cac598638dc823`. The working tree was clean.
- Runtime source matched `f58545d`. Commit `e1b0414` changed documentation only.

## Fresh build (`build-e1b0414`)

- GCC 13.3.0, `-O3 -DNDEBUG`, OpenSSL at `/opt/openssl35`.
- Host SHA-256 prefix: `286684c031528b2fd447`.
- Worker SHA-256 prefix: `c5a1e885bfd111f27112`. The worker at `3f7e8b2` had prefix `0c8bf8f7`.
- Load generator SHA-256 prefix: `c76e386692d1f4395e21`.

## Frozen tests on the fresh build

- queue-saturation passed 14/14.
- outbound-buffer passed 5/5.
- p0-boundaries passed.

## A/A comparison

The fixed-1k fixture ran at 64 connections for one warm-up round and three measured rounds.

| Metric | Three rounds | Median | Observed spread | Limit |
| --- | --- | ---: | ---: | ---: |
| QPS | 1,552 / 1,543 / 1,562 | 1,552 | 1.2% | ≤5% |
| p99 (ms) | 65.4 / 61.0 / 55.0 | 61 | 9.8% | ≤10% |

## Environmental noise

- The cold first round reached 1,373 QPS against a steady-state 1,552. A dedicated warm-up round removed this effect.
- With 64 concurrent connections, p99 varied from 55 to 65 ms. The baseline uses the median.
- The host VM disk was 95% full (224G/234G). Each build required about 0.5G. Free space was checked each round.
- The container mounted `perf_event_paranoid=4` read-only, so `perf` was unavailable. The worker's built-in `CAPSID_PERF_DIAG` probes supplied diagnostics in place of call-level profiling.

## Limits of earlier evidence

In `dual-ab-r2-20260804T143227`, the current side lacked an exact latest-commit identity and call-stack profiles. That run serves as a historical diagnostic only. It did not supply the baseline for this gate.
