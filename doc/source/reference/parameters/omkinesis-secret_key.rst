.. _param-omkinesis-secret_key:

.. meta::
   :description: Reference for the omkinesis secret_key action parameter.
   :keywords: rsyslog, omkinesis, kinesis, secret_key

secret_key
==========

.. index::
   single: omkinesis; secret_key

.. summary-start

AWS secret access key. If omitted, the module reads AWS_SECRET_ACCESS_KEY at configuration time.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkinesis`.

:Name: secret_key
:Scope: action
:Type: string
:Default: AWS_SECRET_ACCESS_KEY
:Required?: yes

Description
-----------

AWS secret access key. If omitted, the module reads AWS_SECRET_ACCESS_KEY at configuration time.
