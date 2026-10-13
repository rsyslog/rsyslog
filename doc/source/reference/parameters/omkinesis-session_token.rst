.. _param-omkinesis-session_token:

.. meta::
   :description: Reference for the omkinesis session_token action parameter.
   :keywords: rsyslog, omkinesis, kinesis, session_token

session_token
=============

.. index::
   single: omkinesis; session_token

.. summary-start

Session token for temporary credentials. If omitted, the module reads AWS_SESSION_TOKEN at configuration time.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omkinesis`.

:Name: session_token
:Scope: action
:Type: string
:Default: AWS_SESSION_TOKEN
:Required?: no

Description
-----------

Session token for temporary credentials. If omitted, the module reads AWS_SESSION_TOKEN at configuration time.
