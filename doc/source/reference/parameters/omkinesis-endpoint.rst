.. SPDX-License-Identifier: Apache-2.0

.. _param-omkinesis-endpoint:

.. meta::
   :description: Reference for the omkinesis endpoint action parameter.
   :keywords: rsyslog, omkinesis, kinesis, endpoint

endpoint
========

.. index::
   single: omkinesis; endpoint

.. summary-start

Endpoint URL override. HTTPS is required except for HTTP on localhost or 127.0.0.1 for local testing.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkinesis`.

:Name: endpoint
:Scope: action
:Type: string
:Default: https://kinesis.<region>.amazonaws.com/
:Required?: no

Description
-----------

Endpoint URL override. HTTPS is required except for HTTP on localhost or 127.0.0.1 for local testing.
