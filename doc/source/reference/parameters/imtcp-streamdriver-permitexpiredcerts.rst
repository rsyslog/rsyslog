.. _param-imtcp-streamdriver-permitexpiredcerts:
.. _imtcp.parameter.module.streamdriver-permitexpiredcerts:

StreamDriver.PermitExpiredCerts
===============================

.. index::
   single: imtcp; StreamDriver.PermitExpiredCerts
   single: StreamDriver.PermitExpiredCerts

.. summary-start

Controls how expired certificates are handled in TLS mode.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: StreamDriver.PermitExpiredCerts
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=warn
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Controls how expired certificates will be handled when stream driver is in TLS mode.
It can have one of the following values:

-  on = Expired certificates are allowed

-  off = Expired certificates are not allowed  (Default, changed from warn to off since Version 8.2012.0)

-  warn = Expired certificates are allowed but warning will be logged

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-streamdriver-permitexpiredcerts:
.. _imtcp.parameter.module.streamdriver-permitexpiredcerts-usage:

.. code-block:: rsyslog

   module(load="imtcp" streamDriver.permitExpiredCerts="off")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

