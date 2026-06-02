.. _param-mmpstrucdata-maxstructureddatasize:
.. _mmpstrucdata.parameter.action.maxstructureddatasize:

maxStructuredDataSize
=====================

.. index::
   single: mmpstrucdata; maxStructuredDataSize
   single: maxStructuredDataSize

.. summary-start

Sets the largest RFC5424 structured-data field, in bytes, that
``mmpstrucdata`` parses.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmpstrucdata`.

:Name: maxStructuredDataSize
:Scope: action
:Type: size
:Default: action=global(maxMessageSize)
:Required?: no
:Introduced: 8.2606.0

Description
-----------

``mmpstrucdata`` skips parsing when a message's RFC5424 structured-data field is
larger than this value and logs an error. The default follows
``global(maxMessageSize)`` so normal input-side message-size policy also bounds
structured-data parsing unless this action parameter is set explicitly.

The value must be greater than zero.

Action usage
------------
.. code-block:: rsyslog

   action(type="mmpstrucdata" maxStructuredDataSize="64k")

YAML usage
----------
.. code-block:: yaml

   actions:
     - type: mmpstrucdata
       maxStructuredDataSize: 64k

See also
--------
See also :doc:`../../configuration/modules/mmpstrucdata`.
