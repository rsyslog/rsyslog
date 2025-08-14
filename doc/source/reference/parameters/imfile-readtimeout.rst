.. _param-imfile-readtimeout:
.. _imfile.parameter.module.readtimeout:

readTimeout
===========

.. index::
   single: imfile; readTimeout
   single: readTimeout

.. summary-start

This sets the default value for input *timeout* parameters.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: readTimeout
:Scope: module | input
:Type: integer
:Default: module=0; input=inherits module
:Required?: no
:Introduced: 8.23.0

Description
-----------
*Default: 0 (no timeout)*

.. versionadded:: 8.23.0

This sets the default value for input *timeout* parameters. See there
for exact meaning. Parameter value is the number of seconds.

.. versionadded:: 8.23.0

This can be used with *startmsg.regex* (but not *readMode*). If specified,
partial multi-line reads are timed out after the specified timeout interval.
That means the current message fragment is being processed and the next
message fragment arriving is treated as a completely new message. The
typical use case for this parameter is a file that is infrequently being
written. In such cases, the next message arrives relatively late, maybe hours
later. Specifying a readTimeout will ensure that those "last messages" are
emitted in a timely manner. In this use case, the "partial" messages being
processed are actually full messages, so everything is fully correct.

To guard against accidental too-early emission of a (partial) message, the
timeout should be sufficiently large (5 to 10 seconds or more recommended).
Specifying a value of zero turns off timeout processing. Also note the
relationship to the *timeoutGranularity* global parameter, which sets the
lower bound of *readTimeout*.

Setting timeout values slightly increases processing time requirements; the
effect should only be visible of a very large number of files is being
monitored.

Module usage
------------
.. _param-imfile-module-readtimeout:
.. _imfile.parameter.module.readtimeout-usage:
.. code-block:: rsyslog

   module(load="imfile" readTimeout="...")

Input usage
-----------
.. _param-imfile-input-readtimeout:
.. _imfile.parameter.input.readtimeout:
.. code-block:: rsyslog

   input(type="imfile" readTimeout="...")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
