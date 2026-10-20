.. SPDX-License-Identifier: Apache-2.0

.. _param-omkinesis-region:

.. meta::
   :description: Reference for the omkinesis region action parameter.
   :keywords: rsyslog, omkinesis, kinesis, region

region
======

.. index::
   single: omkinesis; region

.. summary-start

AWS region used for the endpoint and SigV4 signing scope.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkinesis`.

:Name: region
:Scope: action
:Type: string
:Default: AWS_REGION
:Required?: yes

Description
-----------

AWS region used for the endpoint and SigV4 signing scope.
