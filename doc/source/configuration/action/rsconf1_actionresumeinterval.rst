$ActionResumeInterval
---------------------

**Type:** action configuration parameter

**Default:** 30

**Description:**

Sets the ActionResumeInterval for all following actions. The interval
provided is always in seconds. Thus, multiply by 60 if you need minutes
and 3,600 if you need hours (not recommended).

When an action is suspended (e.g. destination can not be connected), the
action is resumed for the configured interval. Thereafter, it is
retried. If multiple retries fail, the interval is automatically
extended. This is to prevent excessive resource use for retries. After
each 10 retries, the interval is extended by itself. To be precise, the
actual interval is (numRetries / 10 + 1) \* $ActionResumeInterval. so
after the 10th try, it by default is 60 and after the 100th try it is
330.

**Sample:**

$ActionResumeInterval 30

