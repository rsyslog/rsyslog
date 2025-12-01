************************************
omuxsock: Unix sockets Output Module
************************************

===========================  ===========================================================================
**Module Name:**             **omuxsock**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
**Available since:**         4.7.3, 5.5.7
===========================  ===========================================================================


Purpose
=======

This module supports sending syslog messages to local Unix sockets. Thus
it provided a fast message-passing interface between different rsyslog
instances. The counterpart to omuxsock is `imuxsock <imuxsock.html>`_.
Note that the template used together with omuxsock must be suitable to
be processed by the receiver.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.

|FmtObsoleteName| directives
----------------------------

-  **$OMUxSockSocket**
   Name of the socket to send data to. This has no default and **must**
   be set, ideally via the socketName parameter (module or action), and
   not via this legacy parameter.
-  **$OMUxSockDefaultTemplate**
   This can be used to override the default template to be used
   together with omuxsock. This is primarily useful if there are many
   forwarding actions and each of them should use the same template.
   Prefer use of the template parameter (module or action) instead.

Module Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omuxsock-socketname`
     - .. include:: ../../reference/parameters/omuxsock-socketname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omuxsock-sockettype`
     - .. include:: ../../reference/parameters/omuxsock-sockettype.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omuxsock-abstract`
     - .. include:: ../../reference/parameters/omuxsock-abstract.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omuxsock-networknamespace`
     - .. include:: ../../reference/parameters/omuxsock-networknamespace.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omuxsock-template`
     - .. include:: ../../reference/parameters/omuxsock-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


Action Parameters
-----------------

These parameters can be used with the "action()" statement.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omuxsock-socketname`
     - .. include:: ../../reference/parameters/omuxsock-socketname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omuxsock-sockettype`
     - .. include:: ../../reference/parameters/omuxsock-sockettype.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omuxsock-abstract`
     - .. include:: ../../reference/parameters/omuxsock-abstract.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omuxsock-networknamespace`
     - .. include:: ../../reference/parameters/omuxsock-networknamespace.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omuxsock-template`
     - .. include:: ../../reference/parameters/omuxsock-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

Caveats/Known Bugs
==================

Prior to version v8.2512, only datagram sockets were supported.

As of v8.2512, datagram and stream socket types are supported.  In addition, sequenced packet socket types and abstract socket addresses are supported when implemented by the underlying operating system (in general, this is only Linux).


Examples
========

Write all messages to socket (version 1)
----------------------------------------

The following sample writes all messages to the "/tmp/socksample"
socket.  This uses the original (now deprecated) syntax.

.. code-block:: none

   $ModLoad omuxsock
   $OMUxSockSocket /tmp/socksample
   *.* :omuxsock:

Write all messages to socket (version 2)
----------------------------------------

The following sample writes all messages to the "/tmp/socksample"
socket.  This is the same as the previous example, except using module parameters. This can be considered a short-hand style to avoid configuring an instance.

.. code-block:: none

   module(
       load = "omuxsock"
       SocketName = "/tmp/socksample"
   )
   *.* :omuxsock:

Write all messages to socket (version 3)
----------------------------------------

The following sample writes all messages to the "/tmp/socksample"
socket.  This is the same as the previous two examples, except using instance parameters.

.. code-block:: none

   module(
       load = "omuxsock"
   )
   *.* {
       action(
           type = "omuxsock"
           SocketName = "/tmp/socksample"
       )
   }

Write messages to multiple sockets
----------------------------------------

The following sample writes messages to different sockets based on a filter.  In addition, it shows how abstract sockets, socket types, and network namespaces can be used.  Note that abstract sockets do not use names in the filesystem.

.. code-block:: none

   module(
       load = "omuxsock"
   )
   *.* {
       action(
           type = "omuxsock"
           SocketName = "/tmp/socksample"
           SocketType = "DGRAM"
       )
   }
   *.* {
       action(
           type = "omuxsock"
           SocketName = "local"
           Abstract = "1"
           SocketType = "SEQPACKET"
           NetworkNamespace = "routing"
       )
   }
   :msg, contains, "ERROR" {
       action(
           type = "omuxsock"
           SocketName = "syslog/errors"
           Abstract = "1"
           SocketType = "STREAM"
           NetworkNamespace = ""
       )
   }

.. toctree::
   :hidden:

   ../../reference/parameters/omuxsock-abstract.rst
   ../../reference/parameters/omuxsock-networknamespace.rst
   ../../reference/parameters/omuxsock-socketname.rst
   ../../reference/parameters/omuxsock-sockettype.rst
   ../../reference/parameters/omuxsock-template.rst

