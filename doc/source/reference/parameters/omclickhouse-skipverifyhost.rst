.. _param-omclickhouse-skipverifyhost:
.. _omclickhouse.parameter.input.skipverifyhost:

skipVerifyHost
==============

.. index::
   single: omclickhouse; skipVerifyHost
   single: skipVerifyHost
   single: omclickhouse; skipverifyhost
   single: skipverifyhost

.. summary-start

Controls whether the module disables host name verification for HTTPS connections.

.. summary-end

This parameter applies to :doc:`/configuration/modules/omclickhouse`.

:Name: skipVerifyHost
:Scope: input
:Type: boolean
:Default: off
:Required?: no
:Introduced: not specified

Description
-----------
If ``"on"``, this will set the curl ``CURLOPT_SSL_VERIFYHOST`` option to ``0``. You are strongly discouraged to set this to ``"on"``. It is primarily useful only for debugging or testing.

Input usage
-----------
.. _omclickhouse.parameter.input.skipverifyhost-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" skipVerifyHost="on")

See also
--------
See also :doc:`/configuration/modules/omclickhouse`.
