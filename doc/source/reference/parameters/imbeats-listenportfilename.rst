.. _param-imbeats-listenportfilename:
.. _imbeats.parameter.input.listenportfilename:

listenPortFileName
==================

.. meta::
   :description: Port file for imbeats listeners.
   :keywords: rsyslog, imbeats, listenPortFileName

.. index::
   single: imbeats; listenPortFileName
   single: listenPortFileName

.. summary-start

Write the actual bound port to a file after an imbeats listener starts.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: listenPortFileName
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Write the actual bound port to a file after an imbeats listener starts.
Production configurations normally use a fixed Beats listener port such as
``5044``. Dynamic port allocation with ``port="0"`` is intended for testbench
and automation scenarios that need to discover an ephemeral listener port.

Input usage
-----------
.. _param-imbeats-input-listenportfilename:
.. _imbeats.parameter.input.listenportfilename-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" listenPortFileName="/run/rsyslog/imbeats.port")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
