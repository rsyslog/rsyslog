$MarkMessagePeriod
------------------

**Type:** specific to immark input module

**Default:** 1200 (20 minutes)

**Description:**

This specifies when mark messages are to be written to output modules.
The time specified is in seconds. Specifying 0 is possible and disables
mark messages. In that case, however, it is more efficient to NOT load
the immark input module.

So far, there is only one mark message process and any subsequent
$MarkMessagePeriod overwrites the previous.

**This parameter is only available after the immark input module has
been loaded.**

**Sample:**

``$MarkMessagePeriod  600 # mark messages appear every 10 Minutes``

**Available since:** rsyslog 3.0.0

