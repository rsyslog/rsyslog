.. _param-omkinesis-partition_key_template:

.. meta::
   :description: Reference for the omkinesis partition_key_template action parameter.
   :keywords: rsyslog, omkinesis, kinesis, partition key, template

partition_key_template
======================

.. index::
   single: omkinesis; partition_key_template

.. summary-start

Names a template that renders the partition key separately for each record.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkinesis`.

:Name: partition_key_template
:Scope: action
:Type: word
:Default: none
:Required?: Required unless partition_key is set

Description
-----------

Use a template that renders 1 to 256 bytes for every message. An empty or
oversized rendered key fails that record. Set exactly one of
``partition_key`` and ``partition_key_template``.
