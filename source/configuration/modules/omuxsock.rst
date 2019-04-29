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
   be set.
-  **$OMUxSockDefaultTemplate**
   This can be used to override the default template to be used
   together with omuxsock. This is primarily useful if there are many
   forwarding actions and each of them should use the same template.


Caveats/Known Bugs
==================

Currently, only datagram sockets are supported.


Examples
========

Write all messages to socket
----------------------------

The following sample writes all messages to the "/tmp/socksample"
socket.

.. code-block:: none

   $ModLoad omuxsock
   $OMUxSockSocket /tmp/socksample
   *.* :omuxsock:

