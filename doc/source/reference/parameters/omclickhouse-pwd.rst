.. _param-omclickhouse-pwd:
.. _omclickhouse.parameter.module.pwd:

pwd
===

.. index::
   single: omclickhouse; pwd
   single: pwd

.. summary-start

Provides the password for HTTP basic authentication when required by ClickHouse.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omclickhouse`.

:Name: pwd
:Scope: module
:Type: word
:Default: module=""
:Required?: no
:Introduced: not specified

Description
-----------
Password for basic authentication.

Module usage
------------
.. _param-omclickhouse-module-pwd:
.. _omclickhouse.parameter.module.pwd-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" user="ingest" pwd="s3cr3t")

See also
--------
See also :doc:`../../configuration/modules/omclickhouse`.
