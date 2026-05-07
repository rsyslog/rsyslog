.. _param-imkubernetes-defaultfacility:
.. _imkubernetes.parameter.module.defaultfacility:

DefaultFacility
===============

.. index::
   single: imkubernetes; DefaultFacility
   single: DefaultFacility

.. summary-start

Default syslog facility assigned to submitted records; default ``user``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkubernetes`.

:Name: DefaultFacility
:Scope: module
:Type: facility
:Default: ``user``
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Sets the facility used for messages created by ``imkubernetes`` before they are
placed on the main queue.

Module usage
------------

.. code-block:: rsyslog

   module(load="imkubernetes" DefaultFacility="local0")

See also
--------
See also :doc:`../../configuration/modules/imkubernetes`.
