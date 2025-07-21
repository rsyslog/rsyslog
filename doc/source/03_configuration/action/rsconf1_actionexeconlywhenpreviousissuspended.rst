How to Convert Deprecated `$ActionExecOnlyWhenPreviousIsSuspended` to Modern Style
==================================================================================

.. warning::
   The ``$ActionExecOnlyWhenPreviousIsSuspended`` syntax is **deprecated**. We
   strongly recommend using the modern action() syntax for all new configurations.

Modern Equivalent: `action.execOnlyWhenPreviousIsSuspended`
-------------------------------------------------------------

The modern equivalent for `$ActionExecOnlyWhenPreviousIsSuspended` is the **action.execOnlyWhenPreviousIsSuspended** parameter. It serves the same purpose of creating a failover chain where an action is only executed if the previous one is suspended (e.g., a remote server is unreachable).

For full details and a complete list of modern action parameters, please see the :doc:`primary Actions reference page <../actions>`.

Quick Conversion Example
------------------------

This example shows how a legacy failover chain is converted to the modern style.

**Before (Legacy Syntax):**

.. code-block:: rst

   *.* @@primary-syslog.example.com
   $ActionExecOnlyWhenPreviousIsSuspended on
   & @@secondary-1-syslog.example.com
   & @@secondary-2-syslog.example.com
   & /var/log/localbuffer
   $ActionExecOnlyWhenPreviousIsSuspended off

**After (Modern `action()` Syntax):**

.. code-block:: rsyslog

   # Primary action
   action(type="omfwd" target="primary-syslog.example.com" protocol="tcp")

   # First backup action
   action(type="omfwd" target="secondary-1-syslog.example.com" protocol="tcp"
          action.execOnlyWhenPreviousIsSuspended="on")

   # Second backup action
   action(type="omfwd" target="secondary-2-syslog.example.com" protocol="tcp"
          action.execOnlyWhenPreviousIsSuspended="on")

   # Final local file buffer
   action(type="omfile" file="/var/log/localbuffer"
          action.execOnlyWhenPreviousIsSuspended="on")
