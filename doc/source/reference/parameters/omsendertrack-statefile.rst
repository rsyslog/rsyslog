.. _param-omsendertrack-statefile:
.. _omsendertrack.parameter.action.statefile:

.. meta::
   :description: Configure the JSON sender-statistics state file for omsendertrack.
   :keywords: rsyslog, omsendertrack, stateFile, JSON, recovery

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
:Scope: action
:Type: string
:Default: action=none
:Required?: yes
:Introduced: 8.2506.0 (Proof-of-Concept)

Description
-----------
This mandatory parameter specifies the **absolute path to the JSON file** where
sender information is stored. The module updates this file periodically based
on the configured :ref:`interval <param-omsendertrack-interval>` and also upon
rsyslog shutdown to preserve the latest statistics.

A missing file is a normal first start. An empty file is accepted as an empty
state by default. Invalid nonempty files are preserved under a unique
``.corrupt.*`` name and replaced by later normal persistence when
:ref:`IgnoreInvalidStatefile <param-omsendertrack-ignoreinvalidstatefile>` is
enabled. If preserving the invalid file fails, rsyslog starts with empty
statistics but disables later writes for that action, leaving the original
file untouched. Disable recovery when an invalid state file must stop rsyslog
instead.

Atomic updates retain the permission mode of an existing state file. Therefore,
ensure that an existing state file has restrictive permissions, such as
owner-read/write only (``0600``), before configuring it. A new state file is
created with owner-only permissions.

**Important:** Ensure that the rsyslog user has appropriate write permissions to
the directory where this ``statefile`` is located, and that the directory is
not writable by untrusted users. Failure to do so will prevent the module from
saving its state or allow another user to replace the state file.

The parent directory must already exist. Place state files on persistent local
storage; if the directory is on a separate mount, ensure it is available before
the rsyslog service starts.

Action usage
------------
.. _omsendertrack.parameter.action.statefile-usage:

.. code-block:: rsyslog

   module(load="omsendertrack")
   action(type="omsendertrack"
          senderId="%hostname%"
          stateFile="/var/lib/rsyslog/senderstats.json")

See also
--------
See also :doc:`../../configuration/modules/omsendertrack`.
