How to Convert Deprecated `$ActionResumeInterval` to Modern Style
===================================================================

.. warning::
   The ``$ActionResumeInterval`` syntax is **deprecated**. We
   strongly recommend using the modern action() syntax for all new configurations.

Modern Equivalent: `action.resumeInterval`
-------------------------------------------

The modern equivalent for `$ActionResumeInterval` is the **action.resumeInterval** parameter. It serves the same purpose of controlling the retry interval for a failed action.

For full details and a complete list of modern action parameters, please see the :doc:`primary Actions reference page </configuration/actions>`.

Quick Conversion Example
------------------------

This example shows how a legacy configuration is converted to the modern style.

**Before (Legacy Syntax):**

.. code-block:: rst

   $ActionResumeInterval 30
   *.* @@192.168.0.1:514

**After (Modern `action()` Syntax):**

.. code-block:: rsyslog

   action(type="omfwd"
          target="192.168.0.1"
          protocol="tcp"
          action.resumeInterval="30"
         )
