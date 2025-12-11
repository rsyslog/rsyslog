**************************
omrelp: RELP Output Module
**************************

===========================  ===========================================================================
**Module Name:**             **omrelp**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

This module supports sending syslog messages over the reliable RELP
protocol. For RELP's advantages over plain tcp syslog, please see the
documentation for :doc:`imrelp <imrelp>` (the server counterpart). 

Setup

Please note that `librelp <http://www.librelp.com>`__ is required for
imrelp (it provides the core relp protocol implementation).


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
   * - :ref:`param-omrelp-tls-tlslib`
     - .. include:: ../../reference/parameters/omrelp-tls-tlslib.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

Action Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omrelp-target`
     - .. include:: ../../reference/parameters/omrelp-target.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-port`
     - .. include:: ../../reference/parameters/omrelp-port.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-template`
     - .. include:: ../../reference/parameters/omrelp-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-timeout`
     - .. include:: ../../reference/parameters/omrelp-timeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-conn-timeout`
     - .. include:: ../../reference/parameters/omrelp-conn-timeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-rebindinterval`
     - .. include:: ../../reference/parameters/omrelp-rebindinterval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-keepalive`
     - .. include:: ../../reference/parameters/omrelp-keepalive.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-keepalive-probes`
     - .. include:: ../../reference/parameters/omrelp-keepalive-probes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-keepalive-interval`
     - .. include:: ../../reference/parameters/omrelp-keepalive-interval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-keepalive-time`
     - .. include:: ../../reference/parameters/omrelp-keepalive-time.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-windowsize`
     - .. include:: ../../reference/parameters/omrelp-windowsize.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-tls`
     - .. include:: ../../reference/parameters/omrelp-tls.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-tls-compression`
     - .. include:: ../../reference/parameters/omrelp-tls-compression.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-tls-permittedpeer`
     - .. include:: ../../reference/parameters/omrelp-tls-permittedpeer.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-tls-authmode`
     - .. include:: ../../reference/parameters/omrelp-tls-authmode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-tls-cacert`
     - .. include:: ../../reference/parameters/omrelp-tls-cacert.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-tls-mycert`
     - .. include:: ../../reference/parameters/omrelp-tls-mycert.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-tls-myprivkey`
     - .. include:: ../../reference/parameters/omrelp-tls-myprivkey.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-tls-prioritystring`
     - .. include:: ../../reference/parameters/omrelp-tls-prioritystring.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-tls-tlscfgcmd`
     - .. include:: ../../reference/parameters/omrelp-tls-tlscfgcmd.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omrelp-localclientip`
     - .. include:: ../../reference/parameters/omrelp-localclientip.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/omrelp-tls-tlslib
   ../../reference/parameters/omrelp-target
   ../../reference/parameters/omrelp-port
   ../../reference/parameters/omrelp-template
   ../../reference/parameters/omrelp-timeout
   ../../reference/parameters/omrelp-conn-timeout
   ../../reference/parameters/omrelp-rebindinterval
   ../../reference/parameters/omrelp-keepalive
   ../../reference/parameters/omrelp-keepalive-probes
   ../../reference/parameters/omrelp-keepalive-interval
   ../../reference/parameters/omrelp-keepalive-time
   ../../reference/parameters/omrelp-windowsize
   ../../reference/parameters/omrelp-tls
   ../../reference/parameters/omrelp-tls-compression
   ../../reference/parameters/omrelp-tls-permittedpeer
   ../../reference/parameters/omrelp-tls-authmode
   ../../reference/parameters/omrelp-tls-cacert
   ../../reference/parameters/omrelp-tls-mycert
   ../../reference/parameters/omrelp-tls-myprivkey
   ../../reference/parameters/omrelp-tls-prioritystring
   ../../reference/parameters/omrelp-tls-tlscfgcmd
   ../../reference/parameters/omrelp-localclientip


Examples
========

Sending msgs with omrelp
------------------------

The following sample sends all messages to the central server
"centralserv" at port 2514 (note that the server must run imrelp on
port 2514).

.. code-block:: none

   module(load="omrelp")
   action(type="omrelp" target="centralserv" port="2514")


Sending msgs with omrelp via TLS
------------------------------------

This is the same as the previous example but uses TLS (via OpenSSL) for
operations.

Certificate files must exist at configured locations. Note that authmode
"certvalid" is not very strong - you may want to use a different one for
actual deployments. For details, see parameter descriptions.

.. code-block:: none

   module(load="omrelp" tls.tlslib="openssl")
   action(type="omrelp"
		target="centralserv" port="2514" tls="on"
		tls.cacert="tls-certs/ca.pem"
		tls.mycert="tls-certs/cert.pem"
		tls.myprivkey="tls-certs/key.pem"
		tls.authmode="certvalid"
		tls.permittedpeer="rsyslog")


|FmtObsoleteName| directives
============================

This module uses old-style action configuration to keep consistent with
the forwarding rule. So far, no additional configuration directives can
be specified. To send a message via RELP, use

.. code-block:: none

   *.*  :omrelp:<server>:<port>;<template>


