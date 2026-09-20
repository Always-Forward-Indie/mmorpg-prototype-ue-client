# OUTBOX: reliable chunk→game fact delivery

## Problem
~22 fact families (`savePositions`, `saveReputation`, `saveLearnedSkill`, …)
are sent once and forgotten. Loss is visible only at next-join reconcile.
Precedents: silent `setLearnedSkill` (SERVER_BUGS #11), double charges,
reputation LWW races. Goal: exactly-once effect with at-least-once
delivery; loss self-evident within bounded time.

## Design
- **Fact key**: `{characterId}:{factType}:{seq}`; seq = per-character
  monotonic counter (memory + persisted high-watermark, survives restarts
  without mass duplicates).
- **Send (chunk)**: all `ChunkServer.cpp` seam lambdas enqueue into Outbox
  instead of direct `sendDataToGameServer`. Flush on 5s timer (existing
  scheduler) + when N accumulate. Unacked facts retry with exponential
  backoff 5s→60s cap. Per-character FIFO.
- **Receive (game)**: `fact_keys_applied(key, applied_at)` (migration 083).
  Every save handler first `INSERT key … ON CONFLICT DO NOTHING`: inserted
  → apply business logic; conflict → duplicate, re-ACK only.
- **ACK (game→chunk)**: single generic `factAck{key, status}` (applied /
  duplicate), batched per tick. Chunk drops acked facts from Outbox.
- **Counters**: `pending/sent/acked/expired` in the 10s chunk summary +
  error log when `pending` older than 60s (= Watch SEAM pattern).
- **Reconcile (second line)**: high-watermark handshake at join (chunk
  sends, game answers diffs) + catch-up send. Existing snapshots untouched.
- **DLQ**: after retries exhaust → quarantine table + alert (never spin).
- **Deliberate tradeoffs**: 5s polling (not CDC — scale allows); no broker
  (two processes, direct TCP is the right size); sync-tier for valuables
  (vendor buy) is a separate concern, not this plan.

## Status 2026-09-20 (Phase A shipped, pilot-3 game side)

- A1 DONE: migration `083_fact_keys_applied` applied on dev + committed.
- A2 DONE: header-only `FactOutbox` (chunk) + worker integration (key
  stamping, immediate send, 5s strand-bound flush, ACK branch, DLQ expiry
  log) + `Outbox game:` line in the 10s summary (live in prod log).
  6 unit pins, chunk 440/440.
- A4 DONE (pilot): `claim_fact_key` prepare + file-static helpers
  (`outboxFactKeyOf` / `outboxClaimFactKey` 1/0/-1 / `outboxSendFactAck`)
  + wired into saveLearnedSkill, saveReputation, saveInventoryChange
  (legacy keyless senders unaffected). Game 40/40. Remaining ~24 save
  handlers use the same 6-line pattern (follow-up).
- Verified: claim SQL probe (1 then 0, cleaned), live binaries contain the
  code, Watch clean. Commits: chunk `6c1c9c03`, game `6231fa9f`, login
  `17f77051`.
- Remaining (Phases B–D): per-character seq keys are assigned but
  high-watermark persist not yet done; handshake reconcile not built;
  live e2e (bots) deferred.

## Follow-up fix 2026-09-20 (correctness hole in shipped v1)
- Keys were `{char}:{type}:{seq}` with seq restarting at boot → post-restart
  facts collided with pre-restart keys in `fact_keys_applied` and got
  falsely deduped (LOSS). Fixed: `bootId` (random per process) in every
  key + `BootIdsDifferAcrossInstances` pin (chunk `49645910`).
- Allowlist narrowed to the 3 wired handlers (learn/rep/inventory):
  untracked facts would otherwise retry forever into DLQ noise since no
  ACK ever comes for them. Widen strictly together with wiring the game
  handler (comment in code). Chunk 441/441.
- **A. Infra**: migration 083; header-only chunk `Outbox` (enqueue/flush/
  retry/backoff/counters); splice into ~22 seam lambdas (behavior 1-1);
  game `factAck` + applied-keys helper in save handlers. Accept: builds,
  units green, stand boots, Watch clean.
- **B. Keys & seqs**: key generation (per-character seq + high-watermark
  persist); `key` through all facts (chunk) + parse (game). SQL probes:
  double apply of one key → single effect. Accept: duplicate = acked as
  duplicate, zero double effects.
- **C. Reconcile + monitoring**: high-watermark handshake + catch-up;
  counters in 10s summary + Watch pattern on `pending>60s`; docs
  (AGENTS.md/runbook key format + retry semantics); SERVER_BUGS update;
  tracker.
- **D. Live proof (bots, on approval)**: kill/learn/vendor runs + forced
  retry (chunk restart mid-flow → unacked arrives after reconnect).
  Criteria: zero loss, zero dupes, counters converge. Then L3 + Watch.

## Estimates & risks
~4–5h + live phase. Risks: (1) breadth of game-handler edits — mitigated
by unit + SQL probes + live run; (2) perf — negligible (facts ~units/sec,
batched ACKs); (3) reconnect retry storm — mitigated by per-key
coalescing (latest absolute SET wins, deltas queue).
