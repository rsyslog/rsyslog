.. _param-omhttp-server:
.. _omhttp.parameter.module.server:

server
======

.. index::
   single: omhttp; server
   single: server

.. summary-start

Defines the list of HTTP server hostnames or IP addresses that omhttp connects to.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: server
:Scope: module
:Type: array
:Default: module=localhost
:Required?: no
:Introduced: Not specified

Description
-----------
The server address you want to connect to. Specify one or more entries to enable client-side load-balancing behavior provided by libcurl.

Module usage
------------
.. _omhttp.parameter.module.server-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       server=["api1.example.net", "api2.example.net"]
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
