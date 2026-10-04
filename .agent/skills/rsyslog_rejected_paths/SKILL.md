---
name: rsyslog_rejected_paths
description: Consult and update the dated catalog of tried-and-abandoned rsyslog engineering paths so agents do not repeat rejected queue, diskqueue, or similar designs. Use before proposing performance or locking experiments, when parking a candidate, or when checking whether old benchmark advice is stale.
---

# Rejected engineering paths

Canonical log:

[doc/ai/rejected_engineering_paths.md](../../../doc/ai/rejected_engineering_paths.md)

Read that file before inventing a "clever" follow-up in an area it already
covers. Do not treat subsystem READMEs or chat memory as the substitute.

## When to use

- Planning a performance, locking, wakeup, allocator, or disk-queue change
- A user asks to retry waiter targeting, LinkedList enqueue prealloc, or
  another idea that sounds familiar
- A campaign or PR is **not** being pursued and the reason should survive
  the session
- An entry's dates look old and you need to know if it is still binding

## Agent rules

1. **Search first.** Match on `area`, `id`, and a short description of the
   idea. If it is the same design, stop and cite the entry instead of
   reimplementing it.
2. **Honor status.** `rejected` means do not merge that design without new
   evidence. `parked` means do not treat an open PR as approved hygiene.
   `measurement-rejected` means fix the harness before claiming a product
   result. `superseded` means follow the later retained work.
3. **Check dates before citing numbers.** If today is after `stale_after`,
   or `last_reviewed` / `catalog_updated` is older than
   `default_review_horizon_days` (180), say the evidence is historical.
   Do not quote old ratios as current. Re-measure only when `reopen_if` is
   met and the user wants the attempt.
4. **Do not revive on principle.** "Generally better practice" is not enough
   when the catalog records complexity, leak surface, or a missed target.
5. **Record new dead ends in the same change** when you abandon a candidate
   after measurement or review. Fill every template field, set `recorded`
   and `last_reviewed` to today, set `stale_after` to today plus
   `default_binding_horizon_days` (540) unless a shorter horizon is clearly
   justified, and bump `catalog_updated`.
6. **Keep evidence elsewhere.** Point to tracked benchmark JSON, README
   sections, or PR URLs. Do not paste host paths, raw logs, or large tables
   into the catalog.
7. **Update, do not fork.** If an entry is wrong or superseded, edit it and
   refresh `last_reviewed`. Do not add a second overlapping id.

## Related skills

- `rsyslog-performance`: how to measure; this skill is what not to retry
- `rsyslog_module`: concurrency patterns; still consult the catalog for
  queue/wakeup experiments
- `rsyslog_commit`: catalog edits are agent/internal docs, not ChangeLog
