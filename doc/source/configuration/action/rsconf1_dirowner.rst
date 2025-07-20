How to Convert Deprecated `$DirOwner` to Modern Style
======================================================

.. warning::
   The ``$DirOwner`` syntax is **deprecated**. We strongly recommend
   using the modern omfile module syntax for all new configurations.

Modern Equivalent: `dirOwner`
-----------------------------

The modern equivalent for the global `$DirOwner` directive is the **dirOwner** parameter, which is set within an `action()` object using the `omfile` module. It serves the same purpose of setting the user for newly created directories.

For full details, please see the :doc:`omfile module reference page </configuration/modules/omfile>`.

Quick Conversion Example
------------------------

This example shows how a legacy configuration is converted to the modern style.

**Before (Legacy Syntax):**

.. code-block:: rst

   $DirOwner loguser
   *.* /var/log/some-app/logfile.log

**After (Modern `action()` Syntax):**

.. code-block:: rsyslog

   action(type="omfile"
          file="/var/log/some-app/logfile.log"
          dirOwner="loguser"
         )
