.. _param-omkinesis-partition_key:

.. meta::
   :description: Reference for the omkinesis partition_key action parameter.
   :keywords: rsyslog, omkinesis, kinesis, partition_key

partition_key
=============

.. index::
   single: omkinesis; partition_key

.. summary-start

Fixed partition key for all records from this action. It must be 1 to 256 bytes.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkinesis`.

:Name: partition_key
:Scope: action
:Type: string
:Default: none
:Required?: Required unless partition_key_template is set

Description
-----------

Fixed partition key for all records from this action. It must be 1 to 256 bytes.
Set exactly one of ``partition_key`` and ``partition_key_template``.
