.. meta::
   :tag: module:omfwd
   :tag: category:basic

Basic Parameters
################

These settings apply either when loading the `omfwd` module (global defaults)
or on individual `action(type="omfwd")` instances.

Target
======

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array/word", "none", "no", "none"

Description
~~~~~~~~~~~

Specifies the **name or IP address** of the system(s) receiving logs.  
You can configure:

- A **single target** (hostname or IP).  
- An **array of targets**, forming a *target pool* for load‑balancing.

Target Pools (Load‑Balancing)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When you specify multiple targets, rsyslog forms a **target pool**:

- Round‑robin delivery among all **online** targets.  
- Unreachable hosts are removed and retried every 30 s.  
- If **all** targets fail, the action suspends until one recovers.  
- **Note:** Pools require TCP; UDP sends only to the first target.

Examples
~~~~~~~~

**Single‑Target Action**

.. code-block:: rsyslog

   action(
       type="omfwd"              # forwarding module
       target="syslog.example.net"
       port="10514"              # custom TCP port
       protocol="tcp"            # reliable transport
       queue.type="linkedList"   # prevent blocking
   )

**Target‑Pool Action**

.. code-block:: rsyslog

   action(
       type="omfwd"
       target=[
           "syslog1.example.net",
           "syslog2.example.net",
           "syslog3.example.net"
       ]
       port="10514"              # same custom port for all
       protocol="tcp"
       queue.type="linkedList"
   )

Visual Flow
~~~~~~~~~~~

.. mermaid::

   flowchart LR
       LogGen[Log Generator] --> OMFWD[omfwd Action]
       OMFWD --> T1[syslog1.example.net]
       OMFWD --> T2[syslog2.example.net]
       OMFWD --> T3[syslog3.example.net]


targetSrv
=========

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Description
~~~~~~~~~~~

Automatically builds the target list from DNS SRV records instead of a static
``target`` entry. The lookup depends on the transport:

- UDP: ``_syslog._udp.<targetSrv>``
- TCP/TLS: ``_syslog._tcp.<targetSrv>``

`targetSrv` is **mutually exclusive** with ``target``. If no usable SRV records
are returned, initialization fails with a clear error message.

SRV Order and Failover
~~~~~~~~~~~~~~~~~~~~~~

SRV priority and weight are honored when building the candidate list. Existing
pool/load-balancing behavior is reused after the SRV answers are translated into
host/port pairs.

Example
~~~~~~~

Use SRV discovery to find UDP receivers under ``_syslog._udp.example.com``:

.. code-block:: rsyslog

   action(
       type="omfwd"
       targetSrv="example.com"
       protocol="udp"
   )


Port
====

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array/word", "514", "no", "none"

Name or numerical value of the port to use when connecting to the target.
If multiple targets are defined, different ports can be defined for each target.
To do so, use array mode. The first port will be used for the first target, the
second for the second target and so on. If fewer ports than targets are defined,
the remaining targets will use the first port configured. This also means that you
also need to define a single port, if all targets should use the same port.

Note: if more ports than targets are defined, the remaining ports are ignored and
an error message is emitted.

Protocol
========

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "udp", "no", "none"

Type of protocol to use for forwarding. Note that ``tcp`` includes both legacy 
plain TCP syslog and 
`RFC5425 <https://datatracker.ietf.org/doc/html/rfc5425>`_-based TLS-encrypted 
syslog. The selection depends on the StreamDriver parameter. If StreamDriver is 
set to "ossl", "gtls" or "mbedtls", it will use TLS-encrypted syslog.

Template
========

**Type**: word  

**Default**: inherited from module default  

**Mandatory**: no  

When used inside an `action(type="omfwd" ...)`, `template=` overrides the
global template only for that action instance. This lets you customize
message formatting per destination.

Example
~~~~~~~

.. code-block:: rsyslog

   action(
       type="omfwd"
       target="syslog.example.net"
       port="10514"
       protocol="tcp"
       template="JsonFormat"       # Overrides the default with a JSON layout
       queue.type="linkedList"
   )
