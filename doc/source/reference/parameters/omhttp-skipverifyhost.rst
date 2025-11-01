.. _param-omhttp-skipverifyhost:
.. _omhttp.parameter.module.skipverifyhost:

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
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: Not specified

Description
-----------
If ``"on"``, this will set the curl ``CURLOPT_SSL_VERIFYHOST`` option to ``0``. You are strongly discouraged to set this to ``"on"``. It is primarily useful only for debugging or testing.

Module usage
------------
.. _omhttp.parameter.module.skipverifyhost-usage:

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
