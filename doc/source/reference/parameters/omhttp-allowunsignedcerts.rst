.. _param-omhttp-allowunsignedcerts:
.. _omhttp.parameter.module.allowunsignedcerts:

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
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: Not specified

Description
-----------
If ``"on"``, this will set the curl ``CURLOPT_SSL_VERIFYPEER`` option to ``0``. You are strongly discouraged to set this to ``"on"``. It is primarily useful only for debugging or testing.

Module usage
------------
.. _param-omhttp-module-allowunsignedcerts:
.. _omhttp.parameter.module.allowunsignedcerts-usage:

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
