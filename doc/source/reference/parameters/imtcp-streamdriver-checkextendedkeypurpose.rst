.. _param-imtcp-streamdriver-checkextendedkeypurpose:
.. _imtcp.parameter.module.streamdriver-checkextendedkeypurpose:
.. _imtcp.parameter.input.streamdriver-checkextendedkeypurpose:

streamDriver.checkExtendedKeyPurpose
====================================

.. index::
   single: imtcp; StreamDriver.checkExtendedKeyPurpose
   single: StreamDriver.checkExtendedKeyPurpose

.. summary-start

Checks certificate extended key purpose for compatibility with rsyslog operation.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: StreamDriver.checkExtendedKeyPurpose
:Scope: module, input
:Type: boolean
:Default: module=off, input=module parameter
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Whether to check also purpose value in extended fields part of certificate
for compatibility with rsyslog operation. (driver-specific)

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-streamdriver-checkextendedkeypurpose:
.. _imtcp.parameter.module.streamdriver-checkextendedkeypurpose-usage:

.. code-block:: rsyslog

   module(load="imtcp" streamDriver.checkExtendedKeyPurpose="on")

Input usage
-----------
.. _param-imtcp-input-streamdriver-checkextendedkeypurpose:
.. _imtcp.parameter.input.streamdriver-checkextendedkeypurpose-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" streamDriver.checkExtendedKeyPurpose="on")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

