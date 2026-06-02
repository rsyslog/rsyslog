.. _param-imbeats-streamdriver-authmode:
.. _imbeats.parameter.input.streamdriver-authmode:

StreamDriver.AuthMode
=====================

.. meta::
   :description: TLS authentication mode for imbeats.
   :keywords: rsyslog, imbeats, streamdriver authmode

.. index::
   single: imbeats; StreamDriver.AuthMode
   single: StreamDriver.AuthMode

.. summary-start

Set the TLS authentication mode used by the configured imbeats stream driver.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: StreamDriver.AuthMode
:Scope: input
:Type: string
:Default: input=anon
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Set the TLS authentication mode used by the configured imbeats stream driver.

Input usage
-----------
.. _param-imbeats-input-streamdriver-authmode:
.. _imbeats.parameter.input.streamdriver-authmode-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" streamDriver.authMode="anon")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
