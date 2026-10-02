.. _param-mmaitag-response-maxbytes:
.. _mmaitag.parameter.action.response-maxbytes:

response.maxBytes
=================

.. index::
   single: mmaitag; response.maxBytes
   single: response.maxBytes

.. summary-start

Sets the maximum Gemini response body size retained for one request.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmaitag`.

:Name: response.maxBytes
:Scope: action
:Type: positive integer
:Default: 1048576
:Maximum: 67108864
:Required?: no
:Introduced: 8.2610.0

Description
-----------
The response is rejected once it would exceed this many bytes. The maximum
value keeps the response allocation finitely bounded even when customized.

Action usage
------------

.. code-block:: rsyslog

   action(type="mmaitag" response.maxBytes="2097152")

See also
--------
* :doc:`../../configuration/modules/mmaitag`
