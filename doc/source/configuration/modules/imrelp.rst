*************************
imrelp: RELP Input Module
*************************

===========================  ===========================================================================
**Module Name:**             **imrelp**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

Provides the ability to receive syslog messages via the reliable RELP
protocol. This module requires `librelp <http://www.librelp.com>`__ to
be present on the system. From the user's point of view, imrelp works
much like imtcp or imgssapi, except that no message loss can occur.
Please note that with the currently supported RELP protocol version, a
minor message duplication may occur if a network connection between the
relp client and relp server breaks after the client could successfully
send some messages but the server could not acknowledge them. The window
of opportunity is very slim, but in theory this is possible. Future
versions of RELP will prevent this. Please also note that rsyslogd may
lose a few messages if rsyslog is shutdown while a network connection to
the server is broken and could not yet be recovered. Future versions of
RELP support in rsyslog will prevent that issue. Please note that both
scenarios also exist with plain TCP syslog. RELP, even with the small
nits outlined above, is a much more reliable solution than plain TCP
syslog and so it is highly suggested to use RELP instead of plain TCP.
Clients send messages to the RELP server via omrelp.


Notable Features
================

- :ref:`imrelp-statistic-counter`


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.


Module Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imrelp-ruleset`
     - .. include:: ../../reference/parameters/imrelp-ruleset.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-tls-tlslib`
     - .. include:: ../../reference/parameters/imrelp-tls-tlslib.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


Input Parameters
----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imrelp-port`
     - .. include:: ../../reference/parameters/imrelp-port.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-address`
     - .. include:: ../../reference/parameters/imrelp-address.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-name`
     - .. include:: ../../reference/parameters/imrelp-name.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-ruleset-input`
     - .. include:: ../../reference/parameters/imrelp-ruleset-input.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-maxdatasize`
     - .. include:: ../../reference/parameters/imrelp-maxdatasize.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-tls`
     - .. include:: ../../reference/parameters/imrelp-tls.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-tls-compression`
     - .. include:: ../../reference/parameters/imrelp-tls-compression.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-tls-dhbits`
     - .. include:: ../../reference/parameters/imrelp-tls-dhbits.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-tls-permittedpeer`
     - .. include:: ../../reference/parameters/imrelp-tls-permittedpeer.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-tls-authmode`
     - .. include:: ../../reference/parameters/imrelp-tls-authmode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-tls-cacert`
     - .. include:: ../../reference/parameters/imrelp-tls-cacert.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-tls-mycert`
     - .. include:: ../../reference/parameters/imrelp-tls-mycert.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-tls-myprivkey`
     - .. include:: ../../reference/parameters/imrelp-tls-myprivkey.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-tls-prioritystring`
     - .. include:: ../../reference/parameters/imrelp-tls-prioritystring.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-tls-tlscfgcmd`
     - .. include:: ../../reference/parameters/imrelp-tls-tlscfgcmd.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-keepalive`
     - .. include:: ../../reference/parameters/imrelp-keepalive.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-keepalive-probes`
     - .. include:: ../../reference/parameters/imrelp-keepalive-probes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-keepalive-interval`
     - .. include:: ../../reference/parameters/imrelp-keepalive-interval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-keepalive-time`
     - .. include:: ../../reference/parameters/imrelp-keepalive-time.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-oversizemode`
     - .. include:: ../../reference/parameters/imrelp-oversizemode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imrelp-flowcontrol`
     - .. include:: ../../reference/parameters/imrelp-flowcontrol.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


About Chained Certificates
--------------------------

.. versionadded:: 8.2008.0

With librelp 1.7.0, you can use chained certificates.
If using "openssl" as tls.tlslib, we recommend at least OpenSSL Version 1.1
or higher. Chained certificates will also work with OpenSSL Version 1.0.2, but
they will be loaded into the main OpenSSL context object making them available
to all librelp instances (omrelp/imrelp) within the same process.

If this is not desired, you will require to run rsyslog in multiple instances
with different omrelp configurations and certificates.

.. _imrelp-statistic-counter:

Statistic Counter
=================

This plugin maintains :doc:`statistics <../rsyslog_statistic_counter>` for each listener.
The statistic by default is named "imrelp" , followed by the listener port in
parenthesis. For example, the counter for a listener on port 514 is called "imprelp(514)".
If the input is given a name, that input name is used instead of "imrelp". This counter is
available starting rsyslog 7.5.1

The following properties are maintained for each listener:

-  **submitted** - total number of messages submitted for processing since startup


Caveats/Known Bugs
==================

-  see description
-  To obtain the remote system's IP address, you need to have at least
   librelp 1.0.0 installed. Versions below it return the hostname
   instead of the IP address.


Examples
========

Example 1
---------

This sets up a RELP server on port 2514 with a max message size of 10,000 bytes.

.. code-block:: none

   module(load="imrelp") # needs to be done just once
   input(type="imrelp" port="2514" maxDataSize="10k")



Receive RELP traffic via TLS
----------------------------

This receives RELP traffic via TLS using the recommended "openssl" library.
Except for encryption support the scenario is the same as in Example 1.

Certificate files must exist at configured locations. Note that authmode
"certvalid" is not very strong - you may want to use a different one for
actual deployments. For details, see parameter descriptions.

.. code-block:: none

   module(load="imrelp" tls.tlslib="openssl")
   input(type="imrelp" port="2514" maxDataSize="10k"
                tls="on"
                tls.cacert="/tls-certs/ca.pem"
                tls.mycert="/tls-certs/cert.pem"
                tls.myprivkey="/tls-certs/key.pem"
                tls.authmode="certvalid"
                tls.permittedpeer="rsyslog")

.. toctree::
   :hidden:

   ../../reference/parameters/imrelp-ruleset
   ../../reference/parameters/imrelp-tls-tlslib
   ../../reference/parameters/imrelp-port
   ../../reference/parameters/imrelp-address
   ../../reference/parameters/imrelp-name
   ../../reference/parameters/imrelp-ruleset-input
   ../../reference/parameters/imrelp-maxdatasize
   ../../reference/parameters/imrelp-tls
   ../../reference/parameters/imrelp-tls-compression
   ../../reference/parameters/imrelp-tls-dhbits
   ../../reference/parameters/imrelp-tls-permittedpeer
   ../../reference/parameters/imrelp-tls-authmode
   ../../reference/parameters/imrelp-tls-cacert
   ../../reference/parameters/imrelp-tls-mycert
   ../../reference/parameters/imrelp-tls-myprivkey
   ../../reference/parameters/imrelp-tls-prioritystring
   ../../reference/parameters/imrelp-tls-tlscfgcmd
   ../../reference/parameters/imrelp-keepalive
   ../../reference/parameters/imrelp-keepalive-probes
   ../../reference/parameters/imrelp-keepalive-interval
   ../../reference/parameters/imrelp-keepalive-time
   ../../reference/parameters/imrelp-oversizemode
   ../../reference/parameters/imrelp-flowcontrol

