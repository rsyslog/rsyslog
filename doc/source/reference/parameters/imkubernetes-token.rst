.. _param-imkubernetes-token:
.. _imkubernetes.parameter.module.token:

Token
=====

.. index::
   single: imkubernetes; Token
   single: Token

.. summary-start

Literal bearer token used for Kubernetes API authentication.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkubernetes`.

:Name: Token
:Scope: module
:Type: string
:Default: none
:Required?: no
:Introduced: 8.2604.0

Description
-----------
If set, this token is used directly in the ``Authorization: Bearer`` header.
It takes precedence over :ref:`TokenFile <param-imkubernetes-tokenfile>`.

Module usage
------------

.. code-block:: rsyslog

   module(load="imkubernetes" Token="eyJhbGciOi...")

See also
--------
See also :doc:`../../configuration/modules/imkubernetes`.
