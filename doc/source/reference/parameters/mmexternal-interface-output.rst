.. _param-mmexternal-interface-output:
.. _mmexternal.parameter.input.interface-output:

interface.output
================

.. index::
   single: mmexternal; interface.output
   single: interface.output

.. summary-start

Selects whether mmexternal expects a JSON response from the external plugin.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmexternal`.

:Name: interface.output
:Scope: input
:Type: string
:Default: json
:Required?: no
:Introduced: 8.2606.0

Description
-----------
This parameter controls what the external program must write to standard
output after each received message. It can be set to one of the following
values:

* ``"json"`` (default): The external program must write one JSON object,
  followed by a line feed. The object is merged into the rsyslog message.
* ``"none"``: The external program is not expected to write a response.
  ``mmexternal`` sends the message and continues immediately.

Use ``"none"`` only for helpers that do not need to modify the rsyslog
message, for example side-effect-only enrichment or notification programs.
If such a helper writes substantial data to standard output, the helper may
eventually block because rsyslog does not read responses in this mode.

Input usage
-----------
.. _param-mmexternal-input-interface-output:
.. _mmexternal.parameter.input.interface-output-usage:

.. code-block:: rsyslog

   module(load="mmexternal")

   action(
       type="mmexternal"
       binary="/path/to/helper"
       interface.output="none"
   )

See also
--------
See also :doc:`../../configuration/modules/mmexternal`.
