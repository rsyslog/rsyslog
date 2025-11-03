.. _param-omsendertrack-statefile:
.. _omsendertrack.parameter.input.statefile:

statefile
=========

.. index::
   single: omsendertrack; statefile
   single: statefile

.. summary-start

Specifies the absolute path to the JSON file where omsendertrack persists
sender statistics.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsendertrack`.

:Name: statefile
:Scope: input
:Type: string
:Default: input=none
:Required?: yes
:Introduced: 8.2506.0 (Proof-of-Concept)

Description
-----------
This mandatory parameter specifies the **absolute path to the JSON file** where
sender information is stored. The module updates this file periodically based
on the configured :ref:`interval <param-omsendertrack-interval>` and also upon
rsyslog shutdown to preserve the latest statistics.

**Important:** Ensure that the rsyslog user has appropriate write permissions to
the directory where this ``statefile`` is located. Failure to do so will
prevent the module from saving its state.

Input usage
-----------
.. _param-omsendertrack-input-statefile:
.. _omsendertrack.parameter.input.statefile-usage:

.. code-block:: rsyslog

   module(load="omsendertrack")
   action(type="omsendertrack"
          senderId="%hostname%"
          stateFile="/var/lib/rsyslog/senderstats.json")

See also
--------
See also :doc:`../../configuration/modules/omsendertrack`.

