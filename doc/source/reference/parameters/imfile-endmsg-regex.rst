.. _param-imfile-endmsg-regex:
.. _imfile.parameter.module.endmsg-regex:
.. _imfile.parameter.module.endmsg.regex:

endmsg.regex
============

.. index::
   single: imfile; endmsg.regex
   single: endmsg.regex

.. summary-start

Specifies a regular expression to detect the end of a multi-line message.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: endmsg.regex
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=none
:Required?: no
:Introduced: 8.38.0

Description
-----------
.. versionadded:: 8.38.0

This permits the processing of multi-line messages. When set, a message is
terminated when ``endmsg.regex`` matches the line that
identifies the end of a message. As this parameter is using regular
expressions, it is more flexible than ``readMode`` but at the cost of lower
performance.
Note that ``readMode`` and ``startmsg.regex`` and ``endmsg.regex`` cannot all be
defined for the same input.
The primary use case for this is multiline container log files which look like
this:

.. code-block:: none

    date stdout P start of message
    date stdout P  middle of message
    date stdout F  end of message

The `F` means this is the line which contains the final part of the message.
The fully assembled message should be `start of message middle of message end of
message`.  `endmsg.regex="^[^ ]+ stdout F "` will match.

Input usage
-----------
.. _param-imfile-input-endmsg-regex:
.. _imfile.parameter.input.endmsg-regex:
.. code-block:: rsyslog

   input(type="imfile" endmsg.regex="...")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
