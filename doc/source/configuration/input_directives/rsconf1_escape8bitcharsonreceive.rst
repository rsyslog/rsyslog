$Escape8BitCharactersOnReceive
------------------------------

**Type:** global configuration parameter

**Default:** off

**Available Since:** 5.5.2

**Description:**

This parameter instructs rsyslogd to replace non US-ASCII characters
(those that have the 8th bit set) during reception of the message. This
may be useful for some systems. Please note that this escaping breaks
Unicode and many other encodings. Most importantly, it can be assumed
that Asian and European characters will be rendered hardly readable by
this settings. However, it may still be useful when the logs themselves
are primarily in English and only occasionally contain local script. If
this option is turned on, all control-characters are converted to a
3-digit octal number and be prefixed with the
$ControlCharacterEscapePrefix character (being '#' by default).

**Warning:**

-  turning on this option most probably destroys non-western character
   sets (like Japanese, Chinese and Korean) as well as European
   character sets.
-  turning on this option destroys digital signatures if such exists
   inside the message
-  if turned on, the drop-cc, space-cc and escape-cc `property
   replacer <property_replacer.html>`_ options do not work as expected
   because control characters are already removed upon message
   reception. If you intend to use these property replacer options, you
   must turn off $Escape8BitCharactersOnReceive.

**Sample:**

``$Escape8BitCharactersOnReceive on``

