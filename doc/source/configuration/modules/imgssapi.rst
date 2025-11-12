************************************
imgssapi: GSSAPI Syslog Input Module
************************************

===========================  ===========================================================================
**Module Name:**             **imgssapi**
**Author:**                  varmojfekoj
===========================  ===========================================================================


Purpose
=======

Provides the ability to receive syslog messages from the network
protected via Kerberos 5 encryption and authentication. This module also
accept plain tcp syslog messages on the same port if configured to do
so. If you need just plain tcp, use :doc:`imtcp <imtcp>` instead.

Note: This is a contributed module, which is not supported by the
rsyslog team. We recommend to use RFC5425 TLS-protected syslog
instead.

Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.

For client-side forwarding using GSSAPI, see :doc:`omgssapi <omgssapi>`.


Input Parameters
----------------

.. note::

   Parameters are only available in Legacy Format.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imgssapi-inputgssserverrun`
     - .. include:: ../../reference/parameters/imgssapi-inputgssserverrun.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imgssapi-inputgssserverservicename`
     - .. include:: ../../reference/parameters/imgssapi-inputgssserverservicename.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imgssapi-inputgssserverpermitplaintcp`
     - .. include:: ../../reference/parameters/imgssapi-inputgssserverpermitplaintcp.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imgssapi-inputgssservermaxsessions`
     - .. include:: ../../reference/parameters/imgssapi-inputgssservermaxsessions.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imgssapi-inputgssserverkeepalive`
     - .. include:: ../../reference/parameters/imgssapi-inputgssserverkeepalive.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imgssapi-inputgsslistenportfilename`
     - .. include:: ../../reference/parameters/imgssapi-inputgsslistenportfilename.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/imgssapi-inputgssserverrun
   ../../reference/parameters/imgssapi-inputgssserverservicename
   ../../reference/parameters/imgssapi-inputgssserverpermitplaintcp
   ../../reference/parameters/imgssapi-inputgssservermaxsessions
   ../../reference/parameters/imgssapi-inputgssserverkeepalive
   ../../reference/parameters/imgssapi-inputgsslistenportfilename


Caveats/Known Bugs
==================

-  module always binds to all interfaces
-  only a single listener can be bound

Example
=======

This sets up a GSS server on port 1514 that also permits to receive
plain tcp syslog messages (on the same port):

.. code-block:: none

   $ModLoad imgssapi # needs to be done just once
   $InputGSSServerRun 1514
   $InputGSSServerPermitPlainTCP on


