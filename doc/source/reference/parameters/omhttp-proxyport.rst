.. _param-omhttp-proxyport:
.. _omhttp.parameter.module.proxyport:

proxyport
=========

.. index::
   single: omhttp; proxyport
   single: proxyport

.. summary-start

Sets the port number of the HTTP proxy used by omhttp.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: proxyport
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: Not specified

Description
-----------
Configures the libcurl ``CURLOPT_PROXYPORT`` option for HTTP requests issued by omhttp. For more details see the libcurl documentation for ``CURLOPT_PROXYPORT``.

Module usage
------------
.. _omhttp.parameter.module.proxyport-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       proxyPort="8080"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
