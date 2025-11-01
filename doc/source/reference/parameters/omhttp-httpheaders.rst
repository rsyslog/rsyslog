.. _param-omhttp-httpheaders:
.. _omhttp.parameter.module.httpheaders:

httpheaders
===========

.. index::
   single: omhttp; httpheaders
   single: httpheaders

.. summary-start

Configures an array of additional HTTP headers that omhttp sends with each request.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: httpheaders
:Scope: module
:Type: array
:Default: module=none
:Required?: no
:Introduced: Not specified

Description
-----------
An array of strings that defines a list of one or more HTTP headers to send with each message. Keep in mind that some HTTP headers are added using other parameters. ``Content-Type`` can be configured using :ref:`param-omhttp-httpcontenttype`, and ``Content-Encoding: gzip`` is added when using the :ref:`param-omhttp-compress` parameter.

Module usage
------------
.. _omhttp.parameter.module.httpheaders-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       httpHeaders=[
           "X-Insert-Key: key",
           "X-Event-Source: logs"
       ]
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
