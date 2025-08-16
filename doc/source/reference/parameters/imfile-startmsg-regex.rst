.. _param-imfile-startmsg-regex:
.. _imfile.parameter.input.startmsg-regex:
.. _imfile.parameter.startmsg-regex:

startmsg.regex
==============

.. index::
   single: imfile; startmsg.regex
   single: startmsg.regex

.. summary-start

Regex that marks the beginning of a new message for multiline processing.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: startmsg.regex
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: none
:Required?: no
:Introduced: 8.10.0

Description
-----------
Permits the processing of multi-line messages. When set, a message is
terminated when the next one begins and ``startmsg.regex`` matches the
line that identifies the start of a message. It is more flexible than
``readMode`` but has lower performance. ``startmsg.regex``,
``endmsg.regex`` and ``readMode`` cannot all be defined for the same
input.

Input usage
-----------
.. _param-imfile-input-startmsg-regex:
.. _imfile.parameter.input.startmsg-regex-usage:

.. code-block:: rsyslog

   input(type="imfile" startmsg.regex="^start")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
