# Fix shutdown loop when queue.maxDiskSpace < queue.maxfilesize

## Summary
This PR fixes issue #2693 where rsyslog could enter an infinite loop during shutdown when `queue.maxDiskSpace` is configured to be smaller than `queue.maxfilesize`.

## Problem
When a disk-assisted queue has `maxDiskSpace` set smaller than `maxFileSize`, the queue cannot properly write to disk during shutdown, causing it to loop indefinitely with error messages:
```
main Q: queue.c: ConsumerDA:qqueueEnqMsg item (343) returned with error state: '-2105'
main Q[DA]: queue.c: doEnqSingleObject: queue FULL - waiting 2000ms to drain.
main Q[DA]: queue.c: doEnqSingleObject: cond timeout, dropping message!
```

## Solution
Added validation in the `qqueueStart` function to check if `maxDiskSpace < maxFileSize` for disk-based and disk-assisted queues. If this invalid configuration is detected:
1. Log a warning message explaining the issue
2. Automatically adjust `maxDiskSpace` to match `maxFileSize` to prevent the shutdown loop

## Changes
- Added configuration validation in `runtime/queue.c`
- Validation occurs during queue initialization before the DA system is set up
- The fix ensures backwards compatibility by adjusting invalid configurations rather than failing

## Testing
The fix can be tested with a configuration like:
```
$ActionQueueType Disk
$ActionQueueMaxDiskSpace 10k
$ActionQueueMaxFileSize 100k
```

With this fix, rsyslog will log a warning and automatically adjust the configuration instead of entering a shutdown loop.

Closes: https://github.com/rsyslog/rsyslog/issues/2693