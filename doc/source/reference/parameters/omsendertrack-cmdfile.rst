.. _param-omsendertrack-cmdfile:
.. _omsendertrack.parameter.action.cmdfile:

.. meta::
   :description: Compatibility command-file parameter for omsendertrack.
   :keywords: rsyslog, omsendertrack, cmdFile, compatibility

cmdfile
=======

.. index::
   single: omsendertrack; cmdfile
   single: cmdfile

.. summary-start

Compatibility parameter reserved for a future omsendertrack command file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsendertrack`.

:Name: cmdfile
:Scope: action
:Type: string
:Default: action=none
:Required?: no
:Introduced: 8.2506.0 (Proof-of-Concept)

Description
-----------
This optional compatibility parameter currently has no effect. Command-file
processing is not implemented: ``omsendertrack`` does not read, create, or
delete the configured file, and a missing file cannot prevent startup. Do not
create a command file solely for the current module version.

Action usage
------------
.. _omsendertrack.parameter.action.cmdfile-usage:

.. code-block:: rsyslog

   module(load="omsendertrack")
   action(type="omsendertrack"
          senderId="%hostname%"
          stateFile="/var/lib/rsyslog/senderstats.json"
          cmdFile="/var/lib/rsyslog/sendercommands.txt")

See also
--------
See also :doc:`../../configuration/modules/omsendertrack`.
