$GssMode
--------

**Type:** global configuration parameter

**Default:** encryption

**Provided by:** *omgssapi*

**Description:**

Specifies GSS-API mode to use, which can be "**integrity**\ " - clients
are authenticated and messages are checked for integrity,
"**encryption**\ " - same as "integrity", but messages are also
encrypted if both sides support it.

**Sample:**

``$GssMode Encryption``

