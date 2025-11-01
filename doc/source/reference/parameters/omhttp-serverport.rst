.. _param-omhttp-serverport:
.. _omhttp.parameter.module.serverport:

serverport
==========

.. index::
   single: omhttp; serverport
   single: serverport

.. summary-start

Specifies the TCP port that omhttp uses when connecting to the configured HTTP server.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: serverport
:Scope: module
:Type: integer
:Default: module=443
:Required?: no
:Introduced: Not specified

Description
-----------
The port you want to connect to.

Module usage
------------
.. _param-omhttp-module-serverport:
.. _omhttp.parameter.module.serverport-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       serverPort="8443"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
