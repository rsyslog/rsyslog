$DropTrailingLFOnReception
--------------------------

**Type:** global configuration parameter

**Default:** on

**Description:**

Syslog messages frequently have the line feed character (LF) as the last
character of the message. In almost all cases, this LF should not really
become part of the message. However, recent IETF syslog standardization
recommends against modifying syslog messages (e.g. to keep digital
signatures valid). This option allows to specify if trailing LFs should
be dropped or not. The default is to drop them, which is consistent with
what sysklogd does.

**Sample:**

``$DropTrailingLFOnReception on``

