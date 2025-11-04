.. _param-omclickhouse-user:
.. _omclickhouse.parameter.module.user:

user
====

.. index::
   single: omclickhouse; user
   single: user

.. summary-start

Sets the username for HTTP basic authentication against ClickHouse.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omclickhouse`.

:Name: user
:Scope: module
:Type: word
:Default: module=default
:Required?: no
:Introduced: not specified

Description
-----------
If you have basic HTTP authentication deployed you can specify your user-name here.

Module usage
------------
.. _param-omclickhouse-module-user:
.. _omclickhouse.parameter.module.user-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" user="ingest")

See also
--------
See also :doc:`../../configuration/modules/omclickhouse`.
