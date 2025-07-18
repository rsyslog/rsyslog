
*******************************
Impcap: network traffic capture
*******************************

====================  =====================================
**Module Name:**      **impcap**
**Author:**           Theo Bertin <theo.bertin@advens.fr>
====================  =====================================

Purpose
=======

Impcap is an input module based upon `tcpdump's libpcap <https://www.tcpdump.org/>`_ library for network traffic capture.

Its goal is to capture network traffic with efficiency, parse network packets metadata AND data, and allow users/modules
to make full use of it.



Configuration Parameters
========================

.. note::
    Parameter names are case-insensitive

Module Parameter
----------------

metadata_container
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "!impcap", "no", "none"

Defines the container to place all the parsed metadata of the network packet.

.. Warning::
    if overwritten, this parameter should always begin with '!' to define the JSON object accompanying messages. No checks are done to ensure that
    and not complying with this rule will prevent impcap/rsyslog from running, or will result in unexpected behaviours.


data_container
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "!data", "no", "none"

Defines the container to place all the data of the network packet. 'data' here defines everything above transport layer
in the OSI model, and is a string representation of the hexadecimal values of the stream.

.. Warning::
    if overwritten, this parameter should always begin with '!' to define the JSON object accompanying messages. No checks are done to ensure that
    and not complying with this rule will prevent impcap/rsyslog from running, or will result in unexpected behaviours.



snap_length
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "number", "65535", "no", "none"

Defines the maximum size of captured packets.
If captured packets are longer than the defined value, they will be capped.
Default value allows any type of packet to be captured entirely but can be much shorter if only metadata capture is
desired (500 to 2000 should still be safe, depending on network protocols).
Be wary though, as impcap won't be able to parse metadata correctly if the value is not high enough.


Input Parameters
----------------

interface
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

This parameter specifies the network interface to listen to. **If 'interface' is not specified, 'file' must be in order
for the module to run.**

.. note::
    The name must be a valid network interface on the system (such as 'lo').
    see :ref:`Supported interface types` for an exhaustive list of all supported interface link-layer types.


file
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

This parameter specifies a pcap file to read.
The file must respect the `pcap file format specification <https://www.tcpdump.org/manpages/pcap-savefile.5.html>`_. **If 'file' is not specified, 'interface' must be in order
for the module to run.**

.. Warning::
    This functionality is not intended for production environments,
    it is designed for development/tests. 


promiscuous
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "off", "no", "none"

When a valid interface is provided, sets the capture to promiscuous for this interface.

.. warning::
    Setting your network interface to promiscuous can come against your local laws and
    regulations, maintainers cannot be held responsible for improper use of the module.


filter
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "none", "no", "none"

Set a filter for the capture.
Filter semantics are defined `on pcap manpages <https://www.tcpdump.org/manpages/pcap-filter.7.html>`_.


tag
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Set a tag to messages coming from this input.


ruleset
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Assign messages from this input to a specific Rsyslog ruleset.


.. _no_buffer:

no_buffer
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "off", "no", "none"

Disable buffering during capture.
By default, impcap asks the system to bufferize packets (see parameters :ref:`buffer_size`, :ref:`buffer_timeout` and
:ref:`packet_count`), this parameter disables buffering completely. This means packets will be handled as soon as they
arrive, but impcap will make more system calls to get them and might miss some depending on the incoming rate and system
performances.


.. _buffer_size:

buffer_size
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "number (octets)", "15740640", "no", "none"

Set a buffer size in bytes to the capture handle.
This parameter is only relevant when :ref:`no_buffer` is not active, and should be set depending on input packet rates,
:ref:`buffer_timeout` and :ref:`packet_count` values.


.. _buffer_timeout:

buffer_timeout
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "number (ms)", "10", "no", "none"

Set a timeout in milliseconds between two system calls to get bufferized packets. This parameter prevents low input rate
interfaces to keep packets in buffers for too long, but does not guarantee fetch every X seconds (see `pcap manpage <https://www.tcpdump.org/manpages/pcap.3pcap.html>`_ for more details).



.. _packet_count:

packet_count
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "number", "5", "no", "none"

Set a maximum number of packets to process at a time. This parameter allows to limit batch calls to a maximum of X
packets at a time.


.. _Supported interface types:

Supported interface types
=========================

Impcap currently supports IEEE 802.3 Ethernet link-layer type interfaces.
Please contact the maintainer if you need a different interface type !
