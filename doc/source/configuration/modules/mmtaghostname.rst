******************************************
mmtaghostname: message modification module
******************************************

================  ==============================================================
**Module Name:**  **mmtaghostname**
**Authors:**      Jean-Philippe Hilaire <jean-philippe.hilaire@pmu.fr> & Philippe Duveau <philippe.duveau@free.fr>
================  ==============================================================


Purpose
=======

As a message modification, it can be used in a different step of the
message processing without interfering in the parsers' chain process
and can be applied before or after parsing process using rulesets.

The purposes are :
 
- to add a tag on message produce by input module which does not provide
  a tag like imudp or imtcp. Useful when the tag is used for routing the
  message.
   
- to force message hostname to the rsyslog value.
  AWS Use case : applications in auto-scaling systems provides logs to rsyslog
  through udp/tcp. As a result of auto-scaling, the name of the host is based
  on an ephemeral IPs (short term meaning). In this situation rsyslog local
  hostname is generally closed to business rule. So replacing hostname received
  by the rsyslog local Hostname provide values to the logs collected.

Compile
=======

To successfully compile mmtaghostname module.

.. code-block:: none

 ./configure --enable-mmtaghostname ...

Configuration Parameters
========================

Input Parameters
----------------

.. note::

   Parameter names are case-insensitive. The camelCase convention is recommended.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-mmtaghostname-tag`
     - .. include:: ../../reference/parameters/mmtaghostname-tag.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmtaghostname-forcelocalhostname`
     - .. include:: ../../reference/parameters/mmtaghostname-forcelocalhostname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/mmtaghostname-tag
   ../../reference/parameters/mmtaghostname-forcelocalhostname

Sample
======

In this sample, the message received is parsed by RFC5424 parser and then 
the HOSTNAME is overwritten and a tag is set. 

.. code-block:: none

    module(load='mmtaghostname')
    module(load='imudp')
    global(localhostname="sales-front")
    
    ruleset(name="TagUDP" parser=[ "rsyslog.rfc5424" ]) {
        action(type="mmtaghostname" tag="front" forceLocalHostname="on")
        call ...
    }
    input(type="imudp" port="514" ruleset="TagUDP")
