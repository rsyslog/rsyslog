.. _param-omsendertrack-statefile:
.. _omsendertrack.parameter.module.statefile:

statefile
=========

.. index::
   single: omsendertrack; statefile
   single: statefile

.. summary-start

Absolute path to the JSON file that persists sender statistics.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsendertrack`.

:Name: statefile
:Scope: action
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: action=none
:Required?: yes
:Introduced: 8.2506.0

Description
-----------
Specifies where the module writes and reads persistent sender tracking data.

Action usage
------------
.. _param-omsendertrack-action-statefile:
.. _omsendertrack.parameter.action.statefile:

.. code-block:: rsyslog

   action(type="omsendertrack" statefile="/path/to/senderstats.json")

Notes
-----
- Ensure the rsyslog user has write permission to the chosen path.

See also
--------
See also :doc:`../../configuration/modules/omsendertrack`.

