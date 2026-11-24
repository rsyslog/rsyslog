.. SPDX-License-Identifier: Apache-2.0

.. _param-omkinesis-max_record_size:

.. meta::
   :description: Reference for the omkinesis max_record_size action parameter.
   :keywords: rsyslog, omkinesis, kinesis, record size

max_record_size
---------------

.. index::
   single: omkinesis; max_record_size

.. summary-start

Maximum combined size of the rendered data and partition key, in bytes.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkinesis`.

:Name: max_record_size
:Scope: action
:Type: integer
:Default: 1048576 (1 MiB)
:Required?: no

Description
-----------

Set the maximum combined size of the data before base64 encoding and the
partition key. Values from 1048576 to 10485760 bytes (1 to 10 MiB) are
accepted. Increase this limit only when the Kinesis stream is configured for
larger records. Records above the action limit are rejected locally.
