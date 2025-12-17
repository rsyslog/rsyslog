.. _param-mmdarwin-container:
.. _mmdarwin.parameter.module.container:

container
=========

.. index::
   single: mmdarwin; container
   single: container

.. summary-start

Defines the JSON container path that holds mmdarwin's generated data.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmdarwin`.

:Name: container
:Scope: module
:Type: word
:Default: module="!mmdarwin"
:Required?: no
:Introduced: 8.2008.0

Description
-----------
Set the JSON container path that mmdarwin uses when it writes Darwin metadata
and response values back to the message. The container string must begin with
``"!"`` or ``"."`` so the path resolves to either the structured data
in the message or a local variable tree.

If no value is configured, mmdarwin stores data under ``"!mmdarwin"``.
The module prefixes this container to the target specified by
:ref:`param-mmdarwin-key` and to the generated ``"darwin_id"`` field that
tracks the request UUID. As a result, selecting a custom container lets you
control where the Darwin score and identifier are recorded.

Module usage
------------
.. _param-mmdarwin-module-container-usage:
.. _mmdarwin.parameter.module.container-usage:

.. code-block:: rsyslog

   module(load="mmdarwin" container="!darwin")
   action(type="mmdarwin" key="certitude")

See also
--------
See also :doc:`../../configuration/modules/mmdarwin`.
