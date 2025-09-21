.. _param-mmexternal-output:
.. _mmexternal.parameter.input.output:

output
======

.. index::
   single: mmexternal; output
   single: output

.. summary-start

Writes the external plugin's standard output and standard error to a helper
log file for debugging.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmexternal`.

:Name: output
:Scope: input
:Type: string
:Default: none
:Required?: no
:Introduced: 8.3.0

Description
-----------
This is a debug aid. If set, this is a filename where the plugin's standard
output and standard error are logged. Note that the output is also being
processed as usual by rsyslog. Setting this parameter thus gives insight into
the internal processing that happens between plugin and rsyslog core.

.. warning::

   The external program's standard error stream is captured together with
   standard output. Any data written to stderr is intermingled with the JSON
   response from stdout and will likely cause parsing failures. Ensure the
   external program does not write to stderr.

Input usage
-----------
.. _mmexternal.parameter.input.output-usage:

.. code-block:: rsyslog

   module(load="mmexternal")

   action(
       type="mmexternal"
       binary="/path/to/mmexternal.py"
       output="/var/log/mmexternal-debug.log"
   )

See also
--------
See also :doc:`../../configuration/modules/mmexternal`.
