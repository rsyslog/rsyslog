.. _param-imptcp-framing-delimiter-regex:
.. _imptcp.parameter.input.framing-delimiter-regex:

framing.delimiter.regex
=======================

.. index::
   single: imptcp; framing.delimiter.regex
   single: framing.delimiter.regex

.. summary-start

Uses a regular expression to identify the start of the next message.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: framing.delimiter.regex
:Scope: input
:Type: string
:Default: input=off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Experimental parameter. It is similar to "MultiLine", but provides greater
control of when a log message ends. You can specify a regular expression that
characterizes the header to expect at the start of the next message. As such,
it indicates the end of the current message. For example, one can use this
setting to use an RFC3164 header as frame delimiter::

    framing.delimiter.regex="^<[0-9]{1,3}>(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)"

Note that when oversize messages arrive this mode may have problems finding
the proper frame terminator. There are some provisions inside imptcp to make
these kinds of problems unlikely, but if the messages are very much over the
configured MaxMessageSize, imptcp emits an error messages. Chances are great
it will properly recover from such a situation.

Input usage
-----------
.. _param-imptcp-input-framing-delimiter-regex:
.. _imptcp.parameter.input.framing-delimiter-regex-usage:

.. code-block:: rsyslog

   input(type="imptcp" framing.delimiter.regex="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
