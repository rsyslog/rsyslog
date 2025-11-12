.. _param-omhttp-allowunsignedcerts:
.. _omhttp.parameter.input.allowunsignedcerts:

allowunsignedcerts
==================

.. index::
   single: omhttp; allowunsignedcerts
   single: allowunsignedcerts

.. summary-start

Controls whether omhttp skips server certificate validation when using HTTPS.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: allowunsignedcerts
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: Not specified

Description
-----------
If ``"on"``, this will set the curl `CURLOPT_SSL_VERIFYPEER <https://curl.se/libcurl/c/CURLOPT_SSL_VERIFYPEER.html>`_ option to ``0``. You are strongly discouraged to set this to ``"on"``. It is primarily useful only for debugging or testing.

Input usage
-----------
.. _omhttp.parameter.input.allowunsignedcerts-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       useHttps="on"
       allowUnsignedCerts="on"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
