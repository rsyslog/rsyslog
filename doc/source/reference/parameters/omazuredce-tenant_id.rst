.. _param-omazuredce-tenant_id:
.. _omazuredce.parameter.action.tenant_id:

.. meta::
   :description: Reference for the omazuredce tenant_id parameter.
   :keywords: rsyslog, omazuredce, tenant_id, azure, entra

tenant_id
=========

.. index::
   single: omazuredce; tenant_id
   single: tenant_id

.. summary-start

Sets the Microsoft Entra tenant used when requesting OAuth access tokens.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazuredce`.

:Name: tenant_id
:Scope: action
:Type: string
:Default: none
:Required?: yes
:Introduced: Not specified

Description
-----------
``tenant_id`` identifies the Microsoft Entra tenant that issues the token for
the configured application credentials.

Action usage
------------
.. _omazuredce.parameter.action.tenant_id-usage:

.. code-block:: rsyslog

   action(type="omazuredce" tenant_id="<tenant-id>" ...)

See also
--------
See also :doc:`../../configuration/modules/omazuredce`.
