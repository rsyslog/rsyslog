$ControlCharacterEscapePrefix
-----------------------------

**Type:** global configuration parameter

**Default:** \\

**Description:**

This option specifies the prefix character to be used for control
character escaping (see option $EscapeControlCharactersOnReceive). By
default, it is '\\', which is backwards-compatible with sysklogd. Change
it to '#' in order to be compliant to the value that is somewhat
suggested by Internet-Draft syslog-protocol.

**IMPORTANT**: do not use the ' character. This is reserved and will
most probably be used in the future as a character delimiter. For the
same reason, the syntax of this parameter will probably change in future
releases.

**Sample:**

``$EscapeControlCharactersOnReceive #  # as of syslog-protocol``

