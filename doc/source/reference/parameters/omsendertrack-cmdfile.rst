.. _param-omsendertrack-cmdfile:
.. _omsendertrack.parameter.input.cmdfile:

cmdfile
=======

.. index::
   single: omsendertrack; cmdfile
   single: cmdfile

.. summary-start

Defines the absolute path to the command file that omsendertrack reads when rsyslog receives a HUP signal.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsendertrack`.

:Name: cmdfile
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: 8.2506.0 (Proof-of-Concept)

Description
-----------
This optional parameter allows you to specify the **absolute path to a command file**.
This file *is designed to be processed when rsyslog receives a HUP signal* (for example via ``systemctl reload rsyslog``).

**Note:** Command file support is currently **not implemented** in this proof-of-concept version of the module.
When implemented, this feature is intended to allow dynamic control over the module's behavior, such as resetting statistics for specific senders, without requiring an rsyslog restart.

Input usage
-----------
.. _param-omsendertrack-input-cmdfile:
.. _omsendertrack.parameter.input.cmdfile-usage:

.. code-block:: rsyslog

   module(load="omsendertrack")
   action(type="omsendertrack"
          senderId="%hostname%"
          stateFile="/var/lib/rsyslog/senderstats.json"
          cmdFile="/var/lib/rsyslog/sendercommands.txt")

See also
--------
See also :doc:`../../configuration/modules/omsendertrack`.

