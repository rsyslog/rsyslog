.. _param-imbeats-streamdriver-mode:
.. _imbeats.parameter.input.streamdriver-mode:

StreamDriver.Mode
=================

.. meta::
   :description: Stream driver mode for imbeats.
   :keywords: rsyslog, imbeats, streamdriver mode

.. index::
   single: imbeats; StreamDriver.Mode
   single: StreamDriver.Mode

.. summary-start

Select whether imbeats uses plain TCP or a TLS-enabled stream driver mode.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: StreamDriver.Mode
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Select whether imbeats uses plain TCP or a TLS-enabled stream driver mode.

Input usage
-----------
.. _param-imbeats-input-streamdriver-mode:
.. _imbeats.parameter.input.streamdriver-mode-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" streamDriver.mode="1")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
