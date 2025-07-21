.. index:: ! droppriv

Privilege Drop Configuration
============================

.. versionadded:: 8.2110.0

This document describes how to configure privilege dropping in rsyslog.

Overview
----------

Rsyslog can drop privileges after startup to run with reduced permissions.
This is configured using global parameters in the configuration.

Configuration
--------------

The following global parameters control privilege dropping:

- ``privdrop.user.id`` - Numerical user ID to drop to
- ``privdrop.user.name`` - User name to drop to  
- ``privdrop.group.id`` - Numerical group ID to drop to
- ``privdrop.group.name`` - Group name to drop to

See the :doc:`global parameters <../06_reference/rainerscript/global>` documentation
for detailed parameter descriptions.

Example
---------

.. code-block:: rsyslog

   # Drop to user/group after startup
   global(privdrop.user.id="1000")
   global(privdrop.group.id="1000")

Security Notes
--------------

- Use numerical IDs when possible for reliability
- Ensure the target user/group has necessary permissions
- Test thoroughly in your environment
