.. _param-omstdout-ensurelfending:
.. _omstdout.parameter.input.ensurelfending:

EnsureLFEnding
==============

.. index::
   single: omstdout; EnsureLFEnding
   single: EnsureLFEnding

.. summary-start

Ensures each message is written with a terminating line feed when sent to stdout.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omstdout`.

:Name: EnsureLFEnding
:Scope: input
:Type: boolean
:Default: input=on
:Required?: no
:Introduced: 4.1.6

Description
-----------
Ensures that each message is written with a terminating LF. If the message
already contains a trailing LF, none is added. This is needed for the automated
tests.

Input usage
-----------
.. _param-omstdout-input-ensurelfending:
.. _omstdout.parameter.input.ensurelfending-usage:

.. code-block:: rsyslog

   action(type="omstdout" ensureLFEnding="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omstdout.parameter.legacy.actionomstdoutensurelfending:
- ``$ActionOMStdoutEnsureLFEnding`` — maps to EnsureLFEnding (status: legacy)

.. index::
   single: omstdout; $ActionOMStdoutEnsureLFEnding
   single: $ActionOMStdoutEnsureLFEnding

See also
--------
See also :doc:`../../configuration/modules/omstdout`.
