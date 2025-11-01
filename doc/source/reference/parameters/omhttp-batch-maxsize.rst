.. _param-omhttp-batch-maxsize:
.. _omhttp.parameter.module.batch-maxsize:

batch.maxsize
=============

.. index::
   single: omhttp; batch.maxsize
   single: batch.maxsize

.. summary-start

Limits how many messages omhttp includes in a batch.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: batch.maxsize
:Scope: module
:Type: Size
:Default: module=100
:Required?: no
:Introduced: Not specified

Description
-----------
This parameter specifies the maximum number of messages that will be sent in each batch.

Module usage
------------
.. _omhttp.parameter.module.batch-maxsize-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       batch="on"
       batch.maxSize="200"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
