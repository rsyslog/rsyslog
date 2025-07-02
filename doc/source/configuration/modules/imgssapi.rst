************************************
imgssapi: GSSAPI Syslog Input Module
************************************

===========================  ===========================================================================
**Module Name:**             **imgssapi**
**Author:**                  varmojfekoj
===========================  ===========================================================================


Purpose
=======

Provides the ability to receive syslog messages from the network
protected via Kerberos 5 encryption and authentication. This module also
accept plain tcp syslog messages on the same port if configured to do
so. If you need just plain tcp, use :doc:`imtcp <imtcp>` instead.

Note: This is a contributed module, which is not supported by the
rsyslog team. We recommend to use RFC5425 TLS-protected syslog
instead.

.. toctree::
   :maxdepth: 1

   gssapi


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Input Parameter
---------------

.. note::

   Parameter are only available in Legacy Format.


InputGSSServerRun
^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "``$InputGSSServerRun``"

Starts a GSSAPI server on selected port - note that this runs
independently from the TCP server.


InputGSSServerServiceName
^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "``$InputGSSServerServiceName``"

The service name to use for the GSS server.


InputGSSServerPermitPlainTCP
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "0", "no", "``$InputGSSServerPermitPlainTCP``"

Permits the server to receive plain tcp syslog (without GSS) on the
same port.


InputGSSServerMaxSessions
^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "200", "no", "``$InputGSSServerMaxSessions``"

Sets the maximum number of sessions supported.


InputGSSServerKeepAlive
^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "0", "no", "``$InputGSSServerKeepAlive``"

.. versionadded:: 8.5.0

Enables or disable keep-alive handling.


InputGSSListenPortFileName
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "``$InputGSSListenPortFileName``"

.. versionadded:: 8.38.0

With this parameter you can specify the name for a file. In this file the
port, imtcp is connected to, will be written.
This parameter was introduced because the testbench works with dynamic ports.

.. note::

   If this parameter is set, 0 will be accepted as the port. Otherwise it
   is automatically changed to port 514


Caveats/Known Bugs
==================

-  module always binds to all interfaces
-  only a single listener can be bound

Example
=======

This sets up a GSS server on port 1514 that also permits to receive
plain tcp syslog messages (on the same port):

.. code-block:: none

   $ModLoad imgssapi # needs to be done just once
   $InputGSSServerRun 1514
   $InputGSSServerPermitPlainTCP on


