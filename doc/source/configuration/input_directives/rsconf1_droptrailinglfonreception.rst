$DropTrailingLFOnReception
--------------------------

**Type:** global configuration parameter

**Default:** on

**Description:**

This is a legacy directive. For new configurations, prefer the modern
``global(parser.dropTrailingLFOnReception="...")`` form. To remove a trailing
carriage return (CR) after LF handling, use
``global(parser.dropTrailingCROnReception="on")`` or the legacy
``$DropTrailingCROnReception on`` directive.

Syslog messages frequently have the line feed character (LF) as the last
character of the message. In almost all cases, this LF should not really
become part of the message. However, recent IETF syslog standardization
recommends against modifying syslog messages (e.g. to keep digital
signatures valid). This option allows to specify if trailing LFs should
be dropped or not. The default is to drop them, which is consistent with
what sysklogd does.

**Sample:**

``$DropTrailingLFOnReception on``

