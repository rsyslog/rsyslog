.. _param-omhttp-skipverifyhost:
.. _omhttp.parameter.input.skipverifyhost:

skipverifyhost
==============

.. index::
   single: omhttp; skipverifyhost
   single: skipverifyhost

.. summary-start

Controls whether omhttp verifies the HTTPS server hostname.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: skipverifyhost
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: Not specified

Description
-----------
If ``"on"``, this will set the curl `CURLOPT_SSL_VERIFYHOST <https://curl.se/libcurl/c/CURLOPT_SSL_VERIFYHOST.html>`_ option to ``0``. You are strongly discouraged to set this to ``"on"``. It is primarily useful only for debugging or testing.

Input usage
-----------
.. _omhttp.parameter.input.skipverifyhost-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       useHttps="on"
       skipVerifyHost="on"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
