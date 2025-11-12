.. _param-omclickhouse-usehttps:
.. _omclickhouse.parameter.input.usehttps:

useHttps
========

.. index::
   single: omclickhouse; useHttps
   single: useHttps
   single: omclickhouse; usehttps
   single: usehttps

.. summary-start

Controls whether HTTPS is used by default when no scheme is specified for the
ClickHouse server.

.. summary-end

This parameter applies to :doc:`/configuration/modules/omclickhouse`.

:Name: useHttps
:Scope: input
:Type: boolean
:Default: on
:Required?: no
:Introduced: not specified

Description
-----------
Default scheme to use when sending events to ClickHouse if none is specified
on a server.

Input usage
-----------
.. _omclickhouse.parameter.input.usehttps-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" useHttps="off")

See also
--------
See also :doc:`/configuration/modules/omclickhouse`.
