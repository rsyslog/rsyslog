.. _param-imfile-endmsg-regex:
.. _imfile.parameter.input.endmsg-regex:
.. _imfile.parameter.endmsg-regex:

endmsg.regex
============

.. index::
   single: imfile; endmsg.regex
   single: endmsg.regex

.. summary-start

Regex that identifies the end of a multi-line message.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: endmsg.regex
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: none
:Required?: no
:Introduced: 8.38.0

Description
-----------
Allows processing of multi-line messages. A message is terminated when
``endmsg.regex`` matches the line that marks the end of the message. It is
more flexible than ``readMode`` but has lower performance. ``endmsg.regex``,
``startmsg.regex`` and ``readMode`` cannot all be defined for the same
input.

Examples:

.. code-block:: none

   date stdout P start of message
   date stdout P  middle of message
   date stdout F  end of message

Here ``endmsg.regex="^[^ ]+ stdout F "`` matches the line with ``F`` and
assembles the message ``start of message middle of message end of message``.

Input usage
-----------
.. _param-imfile-input-endmsg-regex:
.. _imfile.parameter.input.endmsg-regex-usage:

.. code-block:: rsyslog

   input(type="imfile"
         File="/var/log/example.log"
         Tag="example"
         endmsg.regex="pattern")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
