.. _param-omclickhouse-skipverifyhost:
.. _omclickhouse.parameter.module.skipverifyhost:

skipverifyhost
==============

.. index::
   single: omclickhouse; skipverifyhost
   single: skipverifyhost

.. summary-start

Controls whether the module disables host name verification for HTTPS connections.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omclickhouse`.

:Name: skipverifyhost
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: not specified

Description
-----------
If ``"on"``, this will set the curl ``CURLOPT_SSL_VERIFYHOST`` option to ``0``. You are strongly discouraged to set this to ``"on"``. It is primarily useful only for debugging or testing.

Module usage
------------
.. _param-omclickhouse-module-skipverifyhost:
.. _omclickhouse.parameter.module.skipverifyhost-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" skipverifyhost="on")

See also
--------
See also :doc:`../../configuration/modules/omclickhouse`.
