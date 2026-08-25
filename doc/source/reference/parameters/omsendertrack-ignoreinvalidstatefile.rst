.. _param-omsendertrack-ignoreinvalidstatefile:
.. _omsendertrack.parameter.action.ignoreinvalidstatefile:

.. meta::
   :description: Control omsendertrack recovery from invalid JSON state files.
   :keywords: rsyslog, omsendertrack, IgnoreInvalidStatefile, state file, recovery

IgnoreInvalidStatefile
======================

.. index::
   single: omsendertrack; IgnoreInvalidStatefile
   single: IgnoreInvalidStatefile

.. summary-start

Controls whether omsendertrack recovers from empty or invalid persisted state.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsendertrack`.

:Name: IgnoreInvalidStatefile
:Scope: action
:Type: boolean
:Default: action=on
:Required?: no
:Introduced: 8.2610.0

Description
-----------

When ``on``, an empty state file starts with empty statistics. A nonempty file
that is not a complete valid sender-statistics JSON array is moved aside under a
unique ``.corrupt.*`` name and rsyslog starts with empty statistics. If that
backup cannot be made, rsyslog continues but disables later state-file writes
for that action so the evidence is not overwritten.

When ``off``, an empty or invalid existing state file causes rsyslog startup to
fail and is left unchanged. A missing state file is always accepted as a normal
first start.

Action usage
------------
.. _param-omsendertrack-action-ignoreinvalidstatefile:
.. _omsendertrack.parameter.action.ignoreinvalidstatefile-usage:

.. code-block:: rsyslog

   action(type="omsendertrack"
          stateFile="/var/lib/rsyslog/senderstats.json"
          ignoreInvalidStatefile="off")

YAML usage
----------

.. code-block:: yaml

   actions:
     - type: omsendertrack
       stateFile: /var/lib/rsyslog/senderstats.json
       ignoreInvalidStatefile: off

See also
--------

See also :doc:`omsendertrack-statefile`.
