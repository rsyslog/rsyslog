.. _param-omclickhouse-pwd:
.. _omclickhouse.parameter.input.pwd:

pwd
===

.. index::
   single: omclickhouse; pwd
   single: pwd

.. summary-start

Provides the password for HTTP basic authentication when required by ClickHouse.

.. summary-end

This parameter applies to :doc:`/configuration/modules/omclickhouse`.

:Name: pwd
:Scope: input
:Type: word
:Default: ""
:Required?: no
:Introduced: not specified

Description
-----------
Password for basic authentication.

Input usage
-----------
.. _omclickhouse.parameter.input.pwd-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" user="ingest" pwd="s3cr3t")

See also
--------
See also :doc:`/configuration/modules/omclickhouse`.
