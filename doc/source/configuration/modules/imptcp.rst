************************
imptcp: Plain TCP Syslog
************************

===========================  ===========================================================================
**Module Name:**             **imptcp**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

Provides the ability to receive syslog messages via plain TCP syslog.
This is a specialized input plugin tailored for high performance on
Linux. It will probably not run on any other platform. Also, it does not
provide TLS services. Encryption can be provided by using
`stunnel <rsyslog_stunnel.html>`_.

This module has no limit on the number of listeners and sessions that
can be used.

.. note::
   Reverse DNS lookups for remote senders are cached. Set the TTL via
   :ref:`reverse_dns_cache`.

Notable Features
================

- :ref:`imptcp-statistic-counter`
- :ref:`error-messages`


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
   * - :ref:`param-imptcp-threads`
     - .. include:: ../../reference/parameters/imptcp-threads.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-maxsessions`
     - .. include:: ../../reference/parameters/imptcp-maxsessions.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-processonpoller`
     - .. include:: ../../reference/parameters/imptcp-processonpoller.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


Input Parameters
----------------

These parameters can be used with the "input()" statement. They apply to
the input they are specified with.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imptcp-port`
     - .. include:: ../../reference/parameters/imptcp-port.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-path`
     - .. include:: ../../reference/parameters/imptcp-path.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-discardtruncatedmsg`
     - .. include:: ../../reference/parameters/imptcp-discardtruncatedmsg.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-fileowner`
     - .. include:: ../../reference/parameters/imptcp-fileowner.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-fileownernum`
     - .. include:: ../../reference/parameters/imptcp-fileownernum.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-filegroup`
     - .. include:: ../../reference/parameters/imptcp-filegroup.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-filegroupnum`
     - .. include:: ../../reference/parameters/imptcp-filegroupnum.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-filecreatemode`
     - .. include:: ../../reference/parameters/imptcp-filecreatemode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-failonchownfailure`
     - .. include:: ../../reference/parameters/imptcp-failonchownfailure.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-unlink`
     - .. include:: ../../reference/parameters/imptcp-unlink.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-name`
     - .. include:: ../../reference/parameters/imptcp-name.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-ruleset`
     - .. include:: ../../reference/parameters/imptcp-ruleset.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-maxframesize`
     - .. include:: ../../reference/parameters/imptcp-maxframesize.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-maxsessions`
     - .. include:: ../../reference/parameters/imptcp-maxsessions.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-address`
     - .. include:: ../../reference/parameters/imptcp-address.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-addtlframedelimiter`
     - .. include:: ../../reference/parameters/imptcp-addtlframedelimiter.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-supportoctetcountedframing`
     - .. include:: ../../reference/parameters/imptcp-supportoctetcountedframing.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-notifyonconnectionclose`
     - .. include:: ../../reference/parameters/imptcp-notifyonconnectionclose.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-notifyonconnectionopen`
     - .. include:: ../../reference/parameters/imptcp-notifyonconnectionopen.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-keepalive`
     - .. include:: ../../reference/parameters/imptcp-keepalive.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-keepalive-probes`
     - .. include:: ../../reference/parameters/imptcp-keepalive-probes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-keepalive-interval`
     - .. include:: ../../reference/parameters/imptcp-keepalive-interval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-keepalive-time`
     - .. include:: ../../reference/parameters/imptcp-keepalive-time.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-ratelimit-interval`
     - .. include:: ../../reference/parameters/imptcp-ratelimit-interval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-ratelimit-burst`
     - .. include:: ../../reference/parameters/imptcp-ratelimit-burst.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-ratelimit-name`
     - .. include:: ../../reference/parameters/imptcp-ratelimit-name.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-compression-mode`
     - .. include:: ../../reference/parameters/imptcp-compression-mode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-flowcontrol`
     - .. include:: ../../reference/parameters/imptcp-flowcontrol.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-multiline`
     - .. include:: ../../reference/parameters/imptcp-multiline.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-framing-delimiter-regex`
     - .. include:: ../../reference/parameters/imptcp-framing-delimiter-regex.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-socketbacklog`
     - .. include:: ../../reference/parameters/imptcp-socketbacklog.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-defaulttz`
     - .. include:: ../../reference/parameters/imptcp-defaulttz.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-framingfix-cisco-asa`
     - .. include:: ../../reference/parameters/imptcp-framingfix-cisco-asa.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imptcp-listenportfilename`
     - .. include:: ../../reference/parameters/imptcp-listenportfilename.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
.. _imptcp-statistic-counter:

Statistic Counter
=================

This plugin maintains :doc:`statistics <../rsyslog_statistic_counter>` for each listener. The statistic is
named "imtcp" , followed by the bound address, listener port and IP
version in parenthesis. For example, the counter for a listener on port
514, bound to all interfaces and listening on IPv6 is called
"imptcp(\*/514/IPv6)".

The following properties are maintained for each listener:

-  **submitted** - total number of messages submitted for processing since startup


.. _error-messages:

Error Messages
==============

When a message is to long it will be truncated and an error will show the remaining length of the message and the beginning of it. It will be easier to comprehend the truncation.


Caveats/Known Bugs
==================

-  module always binds to all interfaces


Examples
========

Example 1
---------

This sets up a TCP server on port 514:

.. code-block:: none

   module(load="imptcp") # needs to be done just once
   input(type="imptcp" port="514")


Example 2
---------

This creates a listener that listens on the local loopback
interface, only.

.. code-block:: none

   module(load="imptcp") # needs to be done just once
   input(type="imptcp" port="514" address="127.0.0.1")


Example 3
---------

Create a unix domain socket:

.. code-block:: none

   module(load="imptcp") # needs to be done just once
   input(type="imptcp" path="/tmp/unix.sock" unlink="on")


.. toctree::
   :hidden:

   ../../reference/parameters/imptcp-threads
   ../../reference/parameters/imptcp-maxsessions
   ../../reference/parameters/imptcp-processonpoller
   ../../reference/parameters/imptcp-port
   ../../reference/parameters/imptcp-path
   ../../reference/parameters/imptcp-discardtruncatedmsg
   ../../reference/parameters/imptcp-fileowner
   ../../reference/parameters/imptcp-fileownernum
   ../../reference/parameters/imptcp-filegroup
   ../../reference/parameters/imptcp-filegroupnum
   ../../reference/parameters/imptcp-filecreatemode
   ../../reference/parameters/imptcp-failonchownfailure
   ../../reference/parameters/imptcp-unlink
   ../../reference/parameters/imptcp-name
   ../../reference/parameters/imptcp-ruleset
   ../../reference/parameters/imptcp-maxframesize
   ../../reference/parameters/imptcp-address
   ../../reference/parameters/imptcp-addtlframedelimiter
   ../../reference/parameters/imptcp-supportoctetcountedframing
   ../../reference/parameters/imptcp-notifyonconnectionclose
   ../../reference/parameters/imptcp-notifyonconnectionopen
   ../../reference/parameters/imptcp-keepalive
   ../../reference/parameters/imptcp-keepalive-probes
   ../../reference/parameters/imptcp-keepalive-interval
   ../../reference/parameters/imptcp-keepalive-time
   ../../reference/parameters/imptcp-ratelimit-interval
   ../../reference/parameters/imptcp-ratelimit-burst
   ../../reference/parameters/imptcp-ratelimit-name
   ../../reference/parameters/imptcp-compression-mode
   ../../reference/parameters/imptcp-flowcontrol
   ../../reference/parameters/imptcp-multiline
   ../../reference/parameters/imptcp-framing-delimiter-regex
   ../../reference/parameters/imptcp-socketbacklog
   ../../reference/parameters/imptcp-defaulttz
   ../../reference/parameters/imptcp-framingfix-cisco-asa
   ../../reference/parameters/imptcp-listenportfilename
