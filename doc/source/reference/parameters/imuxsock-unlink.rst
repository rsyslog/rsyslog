.. _param-imuxsock-unlink:
.. _imuxsock.parameter.input.unlink:

Unlink
======

.. index::
   single: imuxsock; Unlink
   single: Unlink

.. summary-start

Controls whether the socket is unlinked on open and close.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: Unlink
:Scope: input
:Type: boolean
:Default: input=on
:Required?: no
:Introduced: 7.3.9

Description
-----------
If turned on (default), the socket is unlinked and re-created when opened
and also unlinked when finally closed. Set it to off if you handle socket
creation yourself.

.. note::

   Note that handling socket creation oneself has the
   advantage that a limited amount of messages may be queued by the OS
   if rsyslog is not running.

.. versionadded:: 7.3.9

Input usage
-----------
.. _param-imuxsock-input-unlink:
.. _imuxsock.parameter.input.unlink-usage:

.. code-block:: rsyslog

   input(type="imuxsock" unlink="off")

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
