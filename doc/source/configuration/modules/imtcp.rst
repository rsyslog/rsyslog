******************************
imtcp: TCP Syslog Input Module
******************************

===========================  ===========================================================================
**Module Name:**             **imtcp**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

Provides the ability to receive syslog messages via TCP. Encryption is
natively provided by selecting the appropriate network stream driver
and can also be provided by using `stunnel <rsyslog_stunnel.html>`_ (an
alternative is the use the `imgssapi <imgssapi.html>`_ module).

.. note::
   Reverse DNS lookups for remote senders are cached. To control refresh
   intervals, see :ref:`reverse_dns_cache`.

Notable Features
================

- :ref:`imtcp-statistic-counter`

The ``imtcp`` module runs on all platforms but is **optimized for Linux** and other systems that 
support **epoll in edge-triggered mode**. While earlier versions of imtcp operated exclusively 
in **single-threaded** mode, starting with **version 8.2504.0**, a **worker pool** is used on 
**epoll-enabled systems**, significantly improving performance.

The **number of worker threads** can be configured to match system requirements.

Starvation Protection
---------------------

A common issue in high-volume logging environments is **starvation**, where a few high-traffic 
sources overwhelm the system. Without protection, a worker may become stuck processing a single 
connection continuously, preventing other clients from being served.

For example, if two worker threads are available and one machine floods the system with data, 
**only one worker remains** to handle all other connections. If multiple sources send large 
amounts of data, **all workers could become monopolized**, preventing other connections from 
being processed.

To mitigate this, **imtcp allows limiting the number of consecutive requests a worker can handle 
per session**. Once the limit is reached, the worker temporarily stops processing that session 
and switches to other active connections. This ensures **fair resource distribution** while 
preventing any single sender from **monopolizing rsyslog’s processing power**.

Even in **single-threaded mode**, a high-volume sender may consume significant resources, but it 
will no longer block all other connections.

Configurable Behavior
---------------------

- The **maximum number of requests per session** before switching to another connection can be 
  adjusted.
- In **epoll mode**, the **number of worker threads** can also be configured. More workers 
  provide better protection against single senders dominating processing.

Monitoring and Performance Insights
-----------------------------------

**Statistics counters** provide insights into key metrics, including starvation prevention. 
These counters are **critical for monitoring system health**, especially in **high-volume 
datacenter deployments**.

Configuration Parameters
========================

Module Parameters
-----------------

.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imtcp-addtlframedelimiter`
     - .. include:: ../../reference/parameters/imtcp-addtlframedelimiter.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-disablelfdelimiter`
     - .. include:: ../../reference/parameters/imtcp-disablelfdelimiter.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-maxframesize`
     - .. include:: ../../reference/parameters/imtcp-maxframesize.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-notifyonconnectionopen`
     - .. include:: ../../reference/parameters/imtcp-notifyonconnectionopen.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-notifyonconnectionclose`
     - .. include:: ../../reference/parameters/imtcp-notifyonconnectionclose.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-keepalive`
     - .. include:: ../../reference/parameters/imtcp-keepalive.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-keepalive-probes`
     - .. include:: ../../reference/parameters/imtcp-keepalive-probes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-keepalive-time`
     - .. include:: ../../reference/parameters/imtcp-keepalive-time.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-keepalive-interval`
     - .. include:: ../../reference/parameters/imtcp-keepalive-interval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-flowcontrol`
     - .. include:: ../../reference/parameters/imtcp-flowcontrol.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-maxlisteners`
     - .. include:: ../../reference/parameters/imtcp-maxlisteners.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-maxsessions`
     - .. include:: ../../reference/parameters/imtcp-maxsessions.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-streamdriver-name`
     - .. include:: ../../reference/parameters/imtcp-streamdriver-name.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-workerthreads`
     - .. include:: ../../reference/parameters/imtcp-workerthreads.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-starvationprotection-maxreads`
     - .. include:: ../../reference/parameters/imtcp-starvationprotection-maxreads.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-streamdriver-mode`
     - .. include:: ../../reference/parameters/imtcp-streamdriver-mode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-streamdriver-authmode`
     - .. include:: ../../reference/parameters/imtcp-streamdriver-authmode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-streamdriver-permitexpiredcerts`
     - .. include:: ../../reference/parameters/imtcp-streamdriver-permitexpiredcerts.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-streamdriver-checkextendedkeypurpose`
     - .. include:: ../../reference/parameters/imtcp-streamdriver-checkextendedkeypurpose.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-streamdriver-prioritizesan`
     - .. include:: ../../reference/parameters/imtcp-streamdriver-prioritizesan.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-streamdriver-tlsverifydepth`
     - .. include:: ../../reference/parameters/imtcp-streamdriver-tlsverifydepth.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-permittedpeer`
     - .. include:: ../../reference/parameters/imtcp-permittedpeer.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-discardtruncatedmsg`
     - .. include:: ../../reference/parameters/imtcp-discardtruncatedmsg.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-gnutlsprioritystring`
     - .. include:: ../../reference/parameters/imtcp-gnutlsprioritystring.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-preservecase`
     - .. include:: ../../reference/parameters/imtcp-preservecase.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/imtcp-addtlframedelimiter
   ../../reference/parameters/imtcp-disablelfdelimiter
   ../../reference/parameters/imtcp-maxframesize
   ../../reference/parameters/imtcp-notifyonconnectionopen
   ../../reference/parameters/imtcp-notifyonconnectionclose
   ../../reference/parameters/imtcp-keepalive
   ../../reference/parameters/imtcp-keepalive-probes
   ../../reference/parameters/imtcp-keepalive-time
   ../../reference/parameters/imtcp-keepalive-interval
   ../../reference/parameters/imtcp-flowcontrol
   ../../reference/parameters/imtcp-maxlisteners
   ../../reference/parameters/imtcp-maxsessions
   ../../reference/parameters/imtcp-streamdriver-name
   ../../reference/parameters/imtcp-workerthreads
   ../../reference/parameters/imtcp-starvationprotection-maxreads
   ../../reference/parameters/imtcp-streamdriver-mode
   ../../reference/parameters/imtcp-streamdriver-authmode
   ../../reference/parameters/imtcp-streamdriver-permitexpiredcerts
   ../../reference/parameters/imtcp-streamdriver-checkextendedkeypurpose
   ../../reference/parameters/imtcp-streamdriver-prioritizesan
   ../../reference/parameters/imtcp-streamdriver-tlsverifydepth
   ../../reference/parameters/imtcp-permittedpeer
   ../../reference/parameters/imtcp-discardtruncatedmsg
   ../../reference/parameters/imtcp-gnutlsprioritystring
   ../../reference/parameters/imtcp-preservecase
   ../../reference/parameters/imtcp-port
   ../../reference/parameters/imtcp-listenportfilename
   ../../reference/parameters/imtcp-address
   ../../reference/parameters/imtcp-name
   ../../reference/parameters/imtcp-ruleset
   ../../reference/parameters/imtcp-supportoctetcountedframing
   ../../reference/parameters/imtcp-socketbacklog
   ../../reference/parameters/imtcp-ratelimit-interval
   ../../reference/parameters/imtcp-ratelimit-burst
   ../../reference/parameters/imtcp-streamdriver-cafile
   ../../reference/parameters/imtcp-streamdriver-crlfile
   ../../reference/parameters/imtcp-streamdriver-keyfile
   ../../reference/parameters/imtcp-streamdriver-certfile

Input Parameters
----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imtcp-port`
     - .. include:: ../../reference/parameters/imtcp-port.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-listenportfilename`
     - .. include:: ../../reference/parameters/imtcp-listenportfilename.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-address`
     - .. include:: ../../reference/parameters/imtcp-address.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-name`
     - .. include:: ../../reference/parameters/imtcp-name.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-ruleset`
     - .. include:: ../../reference/parameters/imtcp-ruleset.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-supportoctetcountedframing`
     - .. include:: ../../reference/parameters/imtcp-supportoctetcountedframing.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-socketbacklog`
     - .. include:: ../../reference/parameters/imtcp-socketbacklog.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-ratelimit-interval`
     - .. include:: ../../reference/parameters/imtcp-ratelimit-interval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-ratelimit-burst`
     - .. include:: ../../reference/parameters/imtcp-ratelimit-burst.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-streamdriver-name`
     - .. include:: ../../reference/parameters/imtcp-streamdriver-name.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-streamdriver-mode`
     - .. include:: ../../reference/parameters/imtcp-streamdriver-mode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-streamdriver-authmode`
     - .. include:: ../../reference/parameters/imtcp-streamdriver-authmode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-streamdriver-permitexpiredcerts`
     - .. include:: ../../reference/parameters/imtcp-streamdriver-permitexpiredcerts.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-streamdriver-checkextendedkeypurpose`
     - .. include:: ../../reference/parameters/imtcp-streamdriver-checkextendedkeypurpose.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-streamdriver-prioritizesan`
     - .. include:: ../../reference/parameters/imtcp-streamdriver-prioritizesan.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-streamdriver-tlsverifydepth`
     - .. include:: ../../reference/parameters/imtcp-streamdriver-tlsverifydepth.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-streamdriver-cafile`
     - .. include:: ../../reference/parameters/imtcp-streamdriver-cafile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-streamdriver-crlfile`
     - .. include:: ../../reference/parameters/imtcp-streamdriver-crlfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-streamdriver-keyfile`
     - .. include:: ../../reference/parameters/imtcp-streamdriver-keyfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-streamdriver-certfile`
     - .. include:: ../../reference/parameters/imtcp-streamdriver-certfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-permittedpeer`
     - .. include:: ../../reference/parameters/imtcp-permittedpeer.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-gnutlsprioritystring`
     - .. include:: ../../reference/parameters/imtcp-gnutlsprioritystring.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-maxsessions`
     - .. include:: ../../reference/parameters/imtcp-maxsessions.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-maxlisteners`
     - .. include:: ../../reference/parameters/imtcp-maxlisteners.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-flowcontrol`
     - .. include:: ../../reference/parameters/imtcp-flowcontrol.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-disablelfdelimiter`
     - .. include:: ../../reference/parameters/imtcp-disablelfdelimiter.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-discardtruncatedmsg`
     - .. include:: ../../reference/parameters/imtcp-discardtruncatedmsg.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-notifyonconnectionclose`
     - .. include:: ../../reference/parameters/imtcp-notifyonconnectionclose.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-addtlframedelimiter`
     - .. include:: ../../reference/parameters/imtcp-addtlframedelimiter.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-maxframesize`
     - .. include:: ../../reference/parameters/imtcp-maxframesize.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-preservecase`
     - .. include:: ../../reference/parameters/imtcp-preservecase.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-keepalive`
     - .. include:: ../../reference/parameters/imtcp-keepalive.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-keepalive-probes`
     - .. include:: ../../reference/parameters/imtcp-keepalive-probes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-keepalive-time`
     - .. include:: ../../reference/parameters/imtcp-keepalive-time.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imtcp-keepalive-interval`
     - .. include:: ../../reference/parameters/imtcp-keepalive-interval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
.. _imtcp-statistic-counter:

Statistic Counter
=================

This plugin maintains :doc:`statistics <../rsyslog_statistic_counter>` for each listener. The statistic is named
after the given input name (or "imtcp" if none is configured), followed by
the listener port in parenthesis. For example, the counter for a listener
on port 514 with no set name is called "imtcp(514)".

The following properties are maintained for each listener:

-  **submitted** - total number of messages submitted for processing since startup


.. _imtcp-worker-statistics:

Worker Statistics Counters
--------------------------

When ``imtcp`` operates with **multiple worker threads** (``workerthreads > 1``),  
it **automatically generates statistics counters** to provide insight into worker  
activity and system health. These counters are part of the ``impstats`` module and  
can be used to monitor system performance, detect bottlenecks, and analyze load  
distribution among worker threads.

**Note:** These counters **do not exist** if ``workerthreads`` is set to ``1``,  
as ``imtcp`` runs in single-threaded mode in that case.

**Statistics Counters**

Each worker thread reports its statistics using the format ``tcpsrv/wX``,  
where ``X`` is the worker thread number (e.g., ``tcpsrv/w0`` for the first worker).  
The following counters are available:

- **runs** → Number of times the worker thread has been invoked.
- **read** → Number of read calls performed by the worker.  
  - For TLS connections, this includes both **read** and **write** calls.
- **accept** → Number of times this worker has processed a new connection via ``accept()``.
- **starvation_protect** → Number of times a socket was placed back into the queue  
  due to reaching the ``StarvationProtection.MaxReads`` limit.

**Example Output**
An example of ``impstats`` output with three worker threads:

.. code-block:: none

    10 Thu Feb 27 16:40:02 2025: tcpsrv/w0: origin=imtcp runs=72 read=2662 starvation_protect=1 accept=2
    11 Thu Feb 27 16:40:02 2025: tcpsrv/w1: origin=imtcp runs=74 read=2676 starvation_protect=2 accept=0
    12 Thu Feb 27 16:40:02 2025: tcpsrv/w2: origin=imtcp runs=72 read=1610 starvation_protect=0 accept=0

In this case:

- Worker ``w0`` was invoked **72 times**, performed **2662 reads**,  
  applied **starvation protection once**, and accepted **2 connections**.
- Worker ``w1`` handled more reads but did not process any ``accept()`` calls.
- Worker ``w2`` processed fewer reads and did not trigger starvation protection.

**Usage and Monitoring**

- These counters help analyze how load is distributed across worker threads.
- High ``starvation_protect`` values indicate that some connections are consuming  
  too many reads, potentially impacting fairness.
- If a single worker handles **most** of the ``accept()`` calls, this may  
  indicate an imbalance in connection handling.
- Regular monitoring can help optimize the ``workerthreads`` and  
  ``StarvationProtection.MaxReads`` parameters for better system efficiency.

By using these statistics, administrators can fine-tune ``imtcp`` to ensure  
**fair resource distribution and optimal performance** in high-traffic environments.


Caveats/Known Bugs
==================

-  module always binds to all interfaces
-  can not be loaded together with `imgssapi <imgssapi.html>`_ (which
   includes the functionality of imtcp)


Examples
========

Example 1
---------

This sets up a TCP server on port 514 and permits it to accept up to 500
connections:

.. code-block:: none

   module(load="imtcp" MaxSessions="500")
   input(type="imtcp" port="514")


Note that the global parameters (here: max sessions) need to be set when
the module is loaded. Otherwise, the parameters will not apply.


Additional Resources
====================

- `rsyslog video tutorial on how to store remote messages in a separate file <http://www.rsyslog.com/howto-store-remote-messages-in-a-separate-file/>`_ (for legacy syntax, but you get the idea).

