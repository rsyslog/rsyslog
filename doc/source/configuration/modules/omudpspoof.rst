**************************************
omudpspoof: UDP spoofing output module
**************************************

===========================  ===========================================================================
**Module Name:**             **omudpspoof**
**Author:**                  David Lang <david@lang.hm> and `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
**Available Since:**         5.1.3
===========================  ===========================================================================


Purpose
=======

This module is similar to the regular UDP forwarder, but permits to
spoof the sender address. Also, it enables to circle through a number of
source ports.

**Important**: This module **requires root permissions**. This is a hard
requirement because raw socket access is necessary to fake UDP sender
addresses. As such, rsyslog cannot drop privileges if this module is
to be used. Ensure that you do **not** use `$PrivDropToUser` or
`$PrivDropToGroup`. Many distro default configurations (notably Ubuntu)
contain these statements. You need to remove or comment them out if you
want to use `omudpspoof`.


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
   * - :ref:`param-omudpspoof-template`
     - .. include:: ../../reference/parameters/omudpspoof-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


Action Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omudpspoof-target`
     - .. include:: ../../reference/parameters/omudpspoof-target.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omudpspoof-port`
     - .. include:: ../../reference/parameters/omudpspoof-port.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omudpspoof-sourcetemplate`
     - .. include:: ../../reference/parameters/omudpspoof-sourcetemplate.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omudpspoof-sourceport-start`
     - .. include:: ../../reference/parameters/omudpspoof-sourceport-start.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omudpspoof-sourceport-end`
     - .. include:: ../../reference/parameters/omudpspoof-sourceport-end.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omudpspoof-mtu`
     - .. include:: ../../reference/parameters/omudpspoof-mtu.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omudpspoof-template`
     - .. include:: ../../reference/parameters/omudpspoof-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


Caveats/Known Bugs
==================

-  **IPv6** is currently not supported. If you need this capability,
   please let us know via the rsyslog mailing list.

-  Throughput is MUCH smaller than when using omfwd module.


Examples
========

Forwarding message through multiple ports
-----------------------------------------

Forward the message to 192.168.1.1, using original source and port between 10000 and 19999.

.. code-block:: rsyslog

   action(
     type="omudpspoof"
     target="192.168.1.1"
     sourcePort.start="10000"
     sourcePort.end="19999"
   )


Forwarding message using another source address
-----------------------------------------------

Forward the message to 192.168.1.1, using source address 192.168.111.111 and default ports.

.. code-block:: rsyslog

   module(
     load="omudpspoof"
   )
   template(
     name="spoofaddr"
     type="string"
     string="192.168.111.111"
   )
   action(
     type="omudpspoof"
     target="192.168.1.1"
     sourceTemplate="spoofaddr"
   )

.. toctree::
   :hidden:

   ../../reference/parameters/omudpspoof-template
   ../../reference/parameters/omudpspoof-target
   ../../reference/parameters/omudpspoof-port
   ../../reference/parameters/omudpspoof-sourcetemplate
   ../../reference/parameters/omudpspoof-sourceport-start
   ../../reference/parameters/omudpspoof-sourceport-end
   ../../reference/parameters/omudpspoof-mtu


