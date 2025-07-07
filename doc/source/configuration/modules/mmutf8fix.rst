Fix invalid UTF-8 Sequences (mmutf8fix)
=======================================

**Module Name:** mmutf8fix

**Author:** Rainer Gerhards <rgerhards@adiscon.com>

**Available since**: 7.5.4

**Description**:

The mmutf8fix module permits to fix invalid UTF-8 sequences. Most often,
such invalid sequences result from syslog sources sending in non-UTF
character sets, e.g. ISO 8859. As syslog does not have a way to convey
the character set information, these sequences are not properly handled.
While they are typically uncritical with plain text files, they can
cause big headache with database sources as well as systems like
ElasticSearch.

The module supports different "fixing" modes and fixes. The current
implementation will always replace invalid bytes with a single US ASCII
character. Additional replacement modes will probably be added in the
future, depending on user demand. In the longer term it could also be
evolved into an any-charset-to-UTF8 converter. But first let's see if it
really gets into widespread enough use.

**Proper Usage**:

Some notes are due for proper use of this module. This is a message
modification module utilizing the action interface, which means you call
it like an action. This gives great flexibility on the question on when
and how to call this module. Note that once it has been called, it
actually modifies the message. The original message is then no longer
available. However, this does **not** change any properties set, used or
extracted before the modification is done.

One potential use case is to normalize all messages. This is done by
simply calling mmutf8fix right in front of all other actions.

If only a specific source (or set of sources) is known to cause
problems, mmutf8fix can be conditionally called only on messages from
them. This also offers performance benefits. If such multiple sources
exists, it probably is a good idea to define different listeners for
their incoming traffic, bind them to specific
`ruleset <multi_ruleset.html>`_ and call mmutf8fix as first action in
this ruleset.

**Module Configuration Parameters**:

Note: parameter names are case-insensitive.

Currently none.

 

**Action Configuration Parameters**:

Note: parameter names are case-insensitive.

-  **mode** - **utf-8**/controlcharacters

   This sets the basic detection mode.
   In **utf-8** mode (the default), proper UTF-8 encoding is checked and
   bytes which are not proper UTF-8 sequences are acted on. If a proper
   multi-byte start sequence byte is detected but any of the following
   bytes is invalid, the whole sequence is replaced by the replacement
   method. This mode is most useful with non-US-ASCII character sets,
   which validly includes multibyte sequences. Note that in this mode
   control characters are NOT being replaced, because they are valid
   UTF-8.
   In **controlcharacters** mode, all bytes which do not represent a
   printable US-ASCII character (codes 32 to 126) are replaced. Note
   that this also mangles valid UTF-8 multi-byte sequences, as these are
   (deliberately) outside of that character range. This mode is most
   useful if it is known that no characters outside of the US-ASCII
   alphabet need to be processed.
-  **replacementChar** - default " " (space), a single character

   This is the character that invalid sequences are replaced by.
   Currently, it MUST be a **printable** US-ASCII character.

**Caveats/Known Bugs:**

-  overlong UTF-8 encodings are currently not detected in utf-8 mode.

**Samples:**

In this snippet, we write one file without fixing UTF-8 and another one
with the message fixed. Note that once mmutf8fix has run, access to the
original message is no longer possible.

::

  module(load="mmutf8fix") action(type="omfile"
  file="/path/to/non-fixed.log") action(type="mmutf8fix")
  action(type="omfile" file="/path/to/fixed.log")

In this sample, we fix only message originating from host 10.0.0.1.

::

  module(load="mmutf8fix") if $fromhost-ip == "10.0.0.1" then
  action(type="mmutf8fix") # all other actions here...

This is mostly the same as the previous sample, but uses
"controlcharacters" processing mode.

::

  module(load="mmutf8fix") if $fromhost-ip == "10.0.0.1" then
  action(type="mmutf8fix" mode="controlcharacters") # all other actions here...

