# MOVED (2026-09-22)

The L3/L4/L5 integration harness no longer lives here. New home (WSL):

`~/projects/mmorpg-prototype/mmorpg-prototype-integration-tests/`

- `Tests/Contract/` → `contracts/` + `proto/`
- `Tools/Bots/` → `bots/`, `Tools/Replay/` → `replay/`, load probes → `load/`
- runners → `bin/*.sh` (`run_l3/run_fast/run_slow`, `preflight`, `watch_logs`, `soak`, `reset_bots`)

This directory is kept as-is for now (deletion is a separate explicit step).
Do NOT extend these copies — extend the integration-tests repo instead.
Runbook: integration-tests `README.md`. Verified green on DEV 2026-09-22
(fast 21+5skip, slow 12+5skip, watch_logs exit 0).
