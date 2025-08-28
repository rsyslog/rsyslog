$DropMsgsWithMaliciousDnsPTRRecords
-----------------------------------

**Type:** global configuration parameter

**Default:** off

**Description:**

Rsyslog contains code to detect malicious DNS PTR records (reverse name
resolution). An attacker might use specially-crafted DNS entries to make
you think that a message might have originated on another IP address.
Rsyslog can detect those cases. It will log an error message in any
case. If this option here is set to "on", the malicious message will be
completely dropped from your logs. If the option is set to "off", the
message will be logged, but the original IP will be used instead of the
DNS name. Reverse lookup results are cached; see :ref:`reverse_dns_cache`
for controlling cache timeout and refresh.

**Sample:**

``$DropMsgsWithMaliciousDnsPTRRecords on``

