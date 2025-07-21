NetStream Drivers
=================

Network stream drivers are a layer between various parts of rsyslogd
(e.g. the imtcp module) and the transport layer. They provide sequenced
delivery, authentication and confidentiality to the upper layers.
Drivers implement different capabilities.

Users need to know about netstream drivers because they need to
configure the proper driver, and proper driver properties, to achieve
desired results (e.g. a :doc:`../07_tutorials/tls`).

Current Network Stream Drivers
------------------------------

.. toctree::
   :maxdepth: 2
   
   ns_ptcp
   ns_gtls
   ns_ossl
