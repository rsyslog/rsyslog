.. _param-omhttp-httpheaderkey:
.. _omhttp.parameter.module.httpheaderkey:

httpheaderkey
=============

.. index::
   single: omhttp; httpheaderkey
   single: httpheaderkey

.. summary-start

Defines the single custom HTTP header name to send with each omhttp request.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: httpheaderkey
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: Not specified

Description
-----------
The header key. Currently only a single additional header/key pair is configurable with this parameter. To specify multiple headers use the :ref:`param-omhttp-httpheaders` parameter. This parameter, along with :ref:`param-omhttp-httpheadervalue`, may be deprecated in the future.

Module usage
------------
.. _param-omhttp-module-httpheaderkey:
.. _omhttp.parameter.module.httpheaderkey-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       httpHeaderKey="X-Event-Source"
       httpHeaderValue="logs"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
