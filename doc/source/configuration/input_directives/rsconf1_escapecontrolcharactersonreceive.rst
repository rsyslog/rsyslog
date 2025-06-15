$EscapeControlCharactersOnReceive
---------------------------------

**Type:** global configuration parameter

**Default:** on

**Description:**

This parameter instructs rsyslogd to replace control characters during
reception of the message. The intent is to provide a way to stop
non-printable messages from entering the syslog system as whole. If this
option is turned on, all control-characters are converted to a 3-digit
octal number and be prefixed with the $ControlCharacterEscapePrefix
character (being '\\' by default). For example, if the BEL character
(ctrl-g) is included in the message, it would be converted to "\\007".
To be compatible to sysklogd, this option must be turned on.

**Warning:**

-  turning on this option most probably destroys non-western character
   sets (like Japanese, Chinese and Korean)
-  turning on this option destroys digital signatures if such exists
   inside the message
-  if turned on, the drop-cc, space-cc and escape-cc `property
   replacer <property_replacer.html>`_ options do not work as expected
   because control characters are already removed upon message
   reception. If you intend to use these property replacer options, you
   must turn off $EscapeControlCharactersOnReceive.

**Sample:**

``$EscapeControlCharactersOnReceive on``

