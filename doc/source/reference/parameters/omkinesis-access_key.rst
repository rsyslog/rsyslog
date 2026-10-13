.. _param-omkinesis-access_key:

.. meta::
   :description: Reference for the omkinesis access_key action parameter.
   :keywords: rsyslog, omkinesis, kinesis, access_key

access_key
==========

.. index::
   single: omkinesis; access_key

.. summary-start

AWS access key ID. If omitted, the module reads AWS_ACCESS_KEY_ID at configuration time.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkinesis`.

:Name: access_key
:Scope: action
:Type: string
:Default: AWS_ACCESS_KEY_ID
:Required?: yes

Description
-----------

AWS access key ID. If omitted, the module reads AWS_ACCESS_KEY_ID at configuration time.
