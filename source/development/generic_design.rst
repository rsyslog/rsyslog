Generic design of a syslogd
---------------------------

Written 2007-04-10 by `Rainer Gerhards <https://rainer.gerhards.net/>`_

The text below describes a generic approach on how a syslogd can be
implemented. I created this description for some other project, where it
was not used. Instead of throwing it away, I thought it would be a good
addition to the rsyslog documentation. While rsyslog differs in details
from the description below, it is sufficiently close to it. Further
development of rsyslog will probably match it even closer to the
description.

If you intend to read the rsyslog source code, I recommend reading this
document here first. You will not find the same names and not all of the
concepts inside rsyslog. However, I think your understanding will
benefit from knowing the generic architecture.

::


       +-----------------+
       | "remote" PLOrig |
       +-----------------+
           |
           I  +--------+-----+-----+          +-----+-------+------+-----+
           P  | PLOrig | GWI | ... |          | GWO | Store | Disc | ... |
           C  +--------+-----+-----+          +-----+-------+------+-----+
           |        |                                          ^
           v        v                                          |
          +--------------+        +------------+          +--------------+
          | PLGenerator  |        |  RelayEng  |          | CollectorEng |
          +--------------+        +------------+          +--------------+
                 |                      ^                       ^
                 |                      |                       |
                 v                      v                       |
          +-------------+         +------------+          +--------------+
          |   PLG Ext   |         | RelEng Ext |          | CollcEng Ext |
          +-------------+         +------------+          +--------------+
                 |                      ^                       ^
                 |                      |                       |
                 v                      v                       |
          +--------------------------------------------------------------+
          |                      Message Router                          |
          +--------------------------------------------------------------+
                             |                            ^
                             v                            |
          +--------------------------------------------------------------+
          |           Message CoDec (e.g. RFC 3164, RFCYYYY)             |
          +--------------------------------------------------------------+
                             |                            ^
                             v                            |
          +---------------------+-----------------------+----------------+
          |    transport UDP    |    transport TLS      |      ...       |
          +---------------------+-----------------------+----------------+

                    Generic Syslog Application Architecture

-  A "syslog application" is an application whose purpose is the
   processing of syslog messages. It may be part of a larger application
   with a broader purpose. An example: a database application might come
   with its own syslog send subsystem and not go through a central
   syslog application. In the sense of this document, that application
   is called a "syslog application" even though a casual observer might
   correctly call it a database application and may not even know that
   it supports sending of syslog messages.
-  Payload is the information that is to be conveyed. Payload by itself
   may have any format and is totally independent from to format
   specified in this document. The "Message CoDec" of the syslog
   application will bring it into the required format.
-  Payload Originators ("PLOrig") are the original creators of payload.
   Typically, these are application programs.
-  A "Remote PLOrig" is a payload originator residing in a different
   application than the syslog application itself. That application may
   reside on a different machine and may talk to the syslog application
   via RPC.
-  A "PLOrig" is a payload originator residing within the syslog
   application itself. Typically, this PLOrig emits syslog application
   startup, shutdown, error and status log messages.
-  A "GWI" is a inbound gateway. For example, an SNMP-to-syslog gateway
   may receive SNMP messages and translate them into syslog.
-  The ellipsis after "GWI" indicates that there are potentially a
   variety of different other ways to originally generate payload.
-  A "PLGenerator" is a payload generator. It takes the information from
   the payload-generating source and integrates it into the syslog
   subsystem of the application. This is a highly theoretical concept.
   In practice, there may not actually be any such component. Instead,
   the payload generators (or other parts like the GWI) may talk
   directly to the syslog subsystem. Conceptually, the "PLGenerator" is
   the first component where the information is actually syslog content.
-  A "PLG Ext" is a payload generator extension. It is used to modify
   the syslog information. An example of a "PLG Ext" might be the
   addition of cryptographic signatures to the syslog information.
-  A "Message Router" is a component that accepts in- and outbound
   syslog information and routes it to the proper next destination
   inside the syslog application. The routing information itself is
   expected to be learnt by operator configuration.
-  A "Message CoDec" is the message encoder/decoder. The encoder takes
   syslog information and encodes them into the required format
   for a syslog message. The decoder takes a syslog message and decodes
   it into syslog information. Codecs for multiple syslog formats may be
   present inside a single syslog application.
-  A transport (UDP, TLS, yet-to-be-defined ones) sends and receives
   syslog messages. Multiple transports may be used by a single
   syslog application at the same time. A single transport instance may
   be used for both sending and receiving. Alternatively, a single
   instance might be used for sending and receiving exclusively.
   Multiple instances may be used for different listener ports and
   receivers.
-  A "RelayEng" is the relaying engine. It provides functionality
   necessary for receiving syslog information and sending it to another
   syslog application.
-  A "RelEng Ext" is an extension that processes syslog information as
   it enters or exits a RelayEng. An example of such a component might
   be a relay cryptographically signing received syslog messages. Such a
   function might be useful to guarantee authenticity starting from a
   given point inside a relay chain.
-  A "CollectorEng" is a collector engine. At this component, syslog
   information leaves the syslog system and is translated into some
   other form. After the CollectorEng, the information is no longer
   defined to be of native syslog type.
-  A "CollcEng Ext" is a collector engine extension. It modifies syslog
   information before it is passed on to the CollectorEng. An example
   for this might be the verification of cryptographically signed syslog
   message information. Please note that another implementation approach
   would be to do the verification outside of the syslog application or
   in a stage after "CollectorEng".
-  A "GWO" is an outbound gateway. An example of this might be the
   forwarding of syslog information via SNMP or SMTP. Please note that
   when a GWO directly connects to a GWI on a different syslog
   application, no native exchange of syslog information takes place.
   Instead, the native protocol of these gateways (e.g. SNMP) is used.
   The syslog information is embedded inside that protocol. Depending on
   protocol and gateway implementation, some of the native syslog
   information might be lost.
-  A "Store" is any way to persistently store the extracted syslog
   information, e.g. to the file system or to a data base.
-  "Disc" means the discarding of messages. Operators often find it
   useful to discard noise messages and so most syslog applications
   contain a way to do that.
-  The ellipsis after "Disc" indicates that there are potentially a
   variety of different other ways to consume syslog information.
-  There may be multiple instances of each of the described components
   in a single syslog application.
-  A syslog application is made up of all or some of the above mentioned
   components.
