.. _param-omsendertrack-cmdfile:
.. _omsendertrack.parameter.module.cmdfile:

cmdfile
=======

.. index::
   single: omsendertrack; cmdfile
   single: cmdfile

.. summary-start

Absolute path to a command file processed on HUP; feature not yet implemented.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsendertrack`.

:Name: cmdfile
:Scope: action
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: action=none
:Required?: no
:Introduced: 8.2506.0

Description
-----------
Intended to point to a file containing commands that the module would read when rsyslog reloads.

Action usage
------------
.. _param-omsendertrack-action-cmdfile:
.. _omsendertrack.parameter.action.cmdfile:

.. code-block:: rsyslog

   action(type="omsendertrack" cmdfile="/path/to/commands.txt")

Notes
-----
- Command file processing is not implemented in the current proof-of-concept.

See also
--------
See also :doc:`../../configuration/modules/omsendertrack`.

