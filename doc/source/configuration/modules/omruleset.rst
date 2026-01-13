*******************************************************
omruleset: ruleset output/including module (DEPRECATED)
*******************************************************

===========================  ===========================================================================
**Module Name:**             **omruleset**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================

.. warning::

   **THIS MODULE IS DEPRECATED AND SHOULD NOT BE USED.**

   It is outdated and inefficient. It has been replaced by the much more
   efficient `"call" RainerScript statement <../rainerscript/rainerscript_call.html>`_.

   The documentation below is provided to help you **identify and migrate** legacy configurations.


Identifying and Migrating Legacy Configurations
===============================================

If you encounter configuration files using ``omruleset``, they are using an obsolete method to invoke rulesets. You should convert these to the modern ``call`` statement, which is faster, safer, and easier to read.

Below are common patterns you might find and how to convert them.

Basic Ruleset Invocation
------------------------

**If you see this legacy code:**

.. code-block:: none

   $ActionOmrulesetRulesetName my_ruleset
   *.* :omruleset:

**It means:**
"Send every message (``*.*``) to the ruleset named ``my_ruleset``."

**Replace it with:**

.. code-block:: none

   call my_ruleset


Conditional Ruleset Invocation (Filters)
----------------------------------------

**If you see this legacy code:**

.. code-block:: none

   $ActionOmrulesetRulesetName nested
   :msg, contains, "error" :omruleset:

**It means:**
"If the message contains 'error', send it to the ruleset named ``nested``."

**Replace it with:**

.. code-block:: none

   if($msg contains "error") then {
       call nested
   }


Common Action / Write-to-File Pattern
-------------------------------------

A common use case for ``omruleset`` was to define a complex action (like writing to a specific file with its own queue) and invoke it from multiple places.

**If you see this legacy code:**

.. code-block:: none

   # Definition of the target ruleset
   $RuleSet CommonAction
   $RulesetCreateMainQueue on
   *.* /path/to/file.log

   # ... later in the config ...

   $ActionOmrulesetRulesetName CommonAction
   mail.info :omruleset:

**It means:**
"Define a ruleset ``CommonAction`` that writes to a file (and has its own queue). Then, if a message is ``mail.info``, send it to that ruleset."

**Replace it with:**

.. code-block:: none

   # Definition (using RainerScript)
   ruleset(name="CommonAction" queue.type="linkedList") {
       action(type="omfile" file="/path/to/file.log")
   }

   # Invocation
   if prifilt("mail.info") then {
       call CommonAction
   }

.. note::
   The ``ruleset()`` object definition allows you to specify queue parameters (like ``queue.type="linkedList"``) directly within the definition, making the configuration much clearer than the old ``$RulesetCreateMainQueue`` directives.

Directives Reference (for Identification)
=========================================

You may see these obsolete directives in old configuration files:

*   **$ModLoad omruleset**: Loads the module. Remove this line.
*   **$ActionOmrulesetRulesetName**: Specifies the target ruleset for the *next* ``:omruleset:`` action.
