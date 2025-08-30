$DropTrailingCROnReception
---------------------------

**Type:** global configuration parameter

**Default:** off

**Description:**

When enabled together with ``$DropTrailingLFOnReception``, this parameter
controls whether carriage return characters (CR) that precede trailing
line feed characters (LF) should also be dropped from received messages.

This is particularly useful when processing logs from Windows systems or
other sources that use Windows-style line endings (CR+LF sequences). Without
this parameter, when ``$DropTrailingLFOnReception`` is enabled, only the LF
is removed from messages ending with CR+LF, leaving the CR character which
appears as ``#015`` in processed logs.

**Important Notes:**

- This parameter only takes effect when ``$DropTrailingLFOnReception`` is also enabled
- The default is ``off`` to maintain backward compatibility with existing installations
- When enabled, only trailing CR characters that immediately precede a trailing LF are removed
- Standalone CR characters (not followed by LF) are never affected

**Backward Compatibility:**

This parameter defaults to ``off`` to preserve existing behavior. Existing
rsyslog installations will continue to work exactly as before unless this
parameter is explicitly enabled.

**Sample:**

Enable both LF and CR removal for Windows-style line endings:

.. code-block:: none

   $DropTrailingLFOnReception on
   $DropTrailingCROnReception on

**New-Style Configuration:**

.. code-block:: none

   global(
       parser.droptrailinglfonreception="on"
       parser.droptrailingcronreception="on"
   )

**See Also:**

- :doc:`rsconf1_droptrailinglfonreception`
- `GitHub Issue #241 <https://github.com/rsyslog/rsyslog/issues/241>`_