imgssapi: GSSAPI Syslog Input Module
====================================


Provides the ability to receive syslog messages from the network
protected via Kerberos 5 encryption and authentication. This module also
accept plain tcp syslog messages on the same port if configured to do
so. If you need just plain tcp, use :doc:`imtcp <imtcp>` instead.

**Author:**\ varmojfekoj

.. toctree::
   :maxdepth: 1

   gssapi

Configuration Directives
------------------------

.. function:: InputGSSServerRun <port>

   Starts a GSSAPI server on selected port - note that this runs
   independently from the TCP server.

.. function:: InputGSSServerServiceName <name>

   The service name to use for the GSS server.

.. function:: $InputGSSServerPermitPlainTCP on\|off

   Permits the server to receive plain tcp syslog (without GSS) on the
   same port

.. function:: $InputGSSServerMaxSessions <number>

   Sets the maximum number of sessions supported

Caveats/Known Bugs
------------------

-  module always binds to all interfaces
-  only a single listener can be bound

Example
-------

This sets up a GSS server on port 1514 that also permits to receive
plain tcp syslog messages (on the same port):

::

  $ModLoad imgssapi # needs to be done just once $InputGSSServerRun 1514
  $InputGSSServerPermitPlainTCP on

