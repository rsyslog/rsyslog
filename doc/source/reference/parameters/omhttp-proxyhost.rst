.. _param-omhttp-proxyhost:
.. _omhttp.parameter.input.proxyhost:

proxyhost
=========

.. index::
   single: omhttp; proxyhost
   single: proxyhost

.. summary-start

Sets the hostname of the HTTP proxy that omhttp should use.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: proxyhost
:Scope: input
:Type: word
:Default: input=none
:Required?: no
:Introduced: Not specified

Description
-----------
Configures the libcurl ``CURLOPT_PROXY`` option for HTTP requests issued by omhttp. For more details see the `libcurl documentation for CURLOPT_PROXY <https://curl.se/libcurl/c/CURLOPT_PROXY.html>`_.

Input usage
-----------
.. _omhttp.parameter.input.proxyhost-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       proxyHost="proxy.internal.example"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
