.. _param-omhttp-batch-maxsize:
.. _omhttp.parameter.input.batch-maxsize:

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
:Scope: input
:Type: Size
:Default: input=100
:Required?: no
:Introduced: Not specified

Description
-----------
This parameter specifies the maximum number of messages that will be sent in each batch.

Input usage
-----------
.. _omhttp.parameter.input.batch-maxsize-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       batch="on"
       batchMaxSize="200"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
