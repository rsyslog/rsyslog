.. _param-omhttp-httpheadervalue:
.. _omhttp.parameter.module.httpheadervalue:

httpheadervalue
===============

.. index::
   single: omhttp; httpheadervalue
   single: httpheadervalue

.. summary-start

Provides the value for the single custom HTTP header defined by :ref:`param-omhttp-httpheaderkey`.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: httpheadervalue
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: Not specified

Description
-----------
The header value for :ref:`param-omhttp-httpheaderkey`.

Module usage
------------
.. _omhttp.parameter.module.httpheadervalue-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       httpHeaderKey="X-Insert-Key"
       httpHeaderValue="apikey"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
