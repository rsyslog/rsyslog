.. _param-imfile-readtimeout:
.. _imfile.parameter.input.readtimeout:
.. _imfile.parameter.readtimeout:

readTimeout
===========

.. index::
   single: imfile; readTimeout
   single: readTimeout

.. summary-start

Sets the multi-line read timeout in seconds; module value provides the default.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: readTimeout
:Scope: module, input
:Type: integer
:Default: module=0; input=inherits module
:Required?: no
:Introduced: 8.23.0

Description
-----------
The module parameter defines the default value for input ``readTimeout``
settings. The value is the number of seconds before partially read messages
are timed out.

At input scope, this can be used with ``startmsg.regex`` (but not
``readMode``). If specified, partial multi-line reads are timed out after the
specified interval. The current message fragment is processed and the next
fragment arriving is treated as a completely new message. This is useful when a
file is infrequently written and a late next message would otherwise delay
processing for a long time.

Module usage
------------
.. _param-imfile-module-readtimeout:
.. _imfile.parameter.module.readtimeout-usage:

.. code-block:: rsyslog

   module(load="imfile" readTimeout="0")

Input usage
-----------
.. _param-imfile-input-readtimeout:
.. _imfile.parameter.input.readtimeout-usage:

.. code-block:: rsyslog

   input(type="imfile"
         File="/var/log/example.log"
         Tag="example"
         readTimeout="0")

Notes
-----
- Use only with ``startmsg.regex``; ignored with ``readMode``.

See also
--------
See also :doc:`../../configuration/modules/imfile`.
