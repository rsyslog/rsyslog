.. _param-imfile-startmsg-regex:
.. _imfile.parameter.module.startmsg-regex:
.. _imfile.parameter.module.startmsg.regex:

startmsg.regex
==============

.. index::
   single: imfile; startmsg.regex
   single: startmsg.regex

.. summary-start

.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: startmsg.regex
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=none
:Required?: no
:Introduced: 8.10.0

Description
-----------
.. versionadded:: 8.10.0

This permits the processing of multi-line messages. When set, a
messages is terminated when the next one begins, and
``startmsg.regex`` contains the regex that identifies the start
of a message. As this parameter is using regular expressions, it
is more flexible than ``readMode`` but at the cost of lower
performance.
Note that ``readMode`` and ``startmsg.regex`` and ``endmsg.regex`` cannot all be
defined for the same input.

Input usage
-----------
.. _param-imfile-input-startmsg-regex:
.. _imfile.parameter.input.startmsg-regex:
.. code-block:: rsyslog

   input(type="imfile" startmsg.regex="...")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
