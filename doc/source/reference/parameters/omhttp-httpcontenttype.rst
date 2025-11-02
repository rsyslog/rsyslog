.. _param-omhttp-httpcontenttype:
.. _omhttp.parameter.input.httpcontenttype:

httpcontenttype
===============

.. index::
   single: omhttp; httpcontenttype
   single: httpcontenttype

.. summary-start

Overrides the HTTP ``Content-Type`` header that omhttp sends with each request.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: httpcontenttype
:Scope: input
:Type: word
:Default: input=application/json; charset=utf-8
:Required?: no
:Introduced: Not specified

Description
-----------
The HTTP ``Content-Type`` header sent with each request. This parameter will override other defaults. If a batching mode is specified, the correct content type is automatically configured. The ``Content-Type`` header can also be configured using the :ref:`param-omhttp-httpheaders` parameter; configure it in only one place.

Input usage
-----------
.. _omhttp.parameter.input.httpcontenttype-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       httpContentType="application/cloudevents+json"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
