.. _param-omhttp-httpheadervalue:
.. _omhttp.parameter.input.httpheadervalue:

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
:Scope: input
:Type: word
:Default: input=none
:Required?: no
:Introduced: Not specified

Description
-----------
The header value for :ref:`param-omhttp-httpheaderkey`.

Input usage
-----------
.. _omhttp.parameter.input.httpheadervalue-usage:

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
