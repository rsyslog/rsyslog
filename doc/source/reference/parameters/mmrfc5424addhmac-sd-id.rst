.. _param-mmrfc5424addhmac-sd-id:
.. _mmrfc5424addhmac.parameter.action.sd-id:

sdId
====

.. index::
   single: mmrfc5424addhmac; sdId
   single: sdId

.. summary-start

Sets the RFC5424 structured data ID added to the message.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmrfc5424addhmac`.

:Name: sdId
:Scope: action
:Type: word
:Default: none
:Required?: yes
:Introduced: 7.5.6

Description
-----------
The RFC5424 structured data ID to be used by this module. This is the SD-ID that
will be added. Note that nothing is added if this SD-ID is already present.

Action usage
------------
.. _param-mmrfc5424addhmac-action-sd-id:
.. _mmrfc5424addhmac.parameter.action.sd-id-usage:

.. code-block:: rsyslog

   action(type="mmrfc5424addhmac" sdId="exampleSDID@32473")

See also
--------
See also :doc:`../../configuration/modules/mmrfc5424addhmac`.

