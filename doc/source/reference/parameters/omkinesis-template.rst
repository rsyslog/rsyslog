.. SPDX-License-Identifier: Apache-2.0

.. _param-omkinesis-template:

.. meta::
   :description: Reference for the omkinesis template action parameter.
   :keywords: rsyslog, omkinesis, kinesis, template

template
========

.. index::
   single: omkinesis; template

.. summary-start

Template rendering the record data. The rendered bytes are base64 encoded for the API.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkinesis`.

:Name: template
:Scope: action
:Type: word
:Default: RSYSLOG_TraditionalFileFormat
:Required?: no

Description
-----------

Template rendering the record data. The rendered bytes are base64 encoded for the API.
