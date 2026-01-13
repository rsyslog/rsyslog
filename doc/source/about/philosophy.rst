.. _core_philosophy:

Our Core Philosophy: Stability and Backwards Compatibility
==========================================================

The most important promise rsyslog makes to its users is **extreme backwards compatibility**.

We understand that rsyslog is infrastructure software. Once you deploy a configuration—whether it's on a single server or across a fleet of thousands—you expect it to keep working, effectively forever. We respect that investment. This commitment to stability drives every decision we make, from how we handle configuration files to how we structure our documentation.

The Evolution of Configuration
==============================

New users are often surprised to find three different ways to configure rsyslog. This is not an accident or a lack of cleanup; it is the visible result of over 40 years of history.

Phase 1: :doc:`sysklogd format <../configuration/sysklogd_format>` (The Standard)
---------------------------------------------------------------------------------
This is the original format from the 1980s BSD syslogd, and later Linux `sysklogd`. It looks like this:

.. code-block:: text

   mail.info    /var/log/mail.log

It is simple, concise, and universally understood. We support this format because it is the standard. Countless tutorials, scripts, and automation tools rely on it. Breaking this would break Linux logging for the world.

Phase 2: :doc:`obsolete legacy format <../configuration/conf_formats>` (The Early Extensions)
---------------------------------------------------------------------------------------------
In the early 2000s, as requirements grew, rsyslog needed to add features that the original syntax couldn't handle (like TCP forwarding or high-precision timestamps). Before a new language was designed, we added directives starting with a dollar sign:

.. code-block:: text

   $ActionFileDefaultTemplate RSYSLOG_TraditionalFileFormat

This is now known as the **obsolete legacy** format. While you will still see it in older tutorials and existing production configs, it is **obsolete**. It has limitations in scope and clarity that make it unsuitable for modern, complex configurations. However, we continue to support it fully so that those older systems continue to function without modification.

Phase 3: :doc:`RainerScript <../rainerscript/index>` (The Modern Era)
---------------------------------------------------------------------
To support the complex requirements of modern observability pipelines—filtering, JSON parsing, variable manipulation, and conditional logic—we created **RainerScript**.

.. code-block:: rsyslog

   if $syslogfacility-text == 'mail' then {
       action(type="omfile" file="/var/log/mail.log")
   }

This is the current, recommended standard for all new configurations. It provides the structure and precision needed for today's logging environments.

Philosophy of Coexistence
=========================

We do not "deprecate and remove" in the way many modern software projects do. In the rsyslog world, **deprecated** means "we recommend you don't use this for *new* work," but it does **not** mean "we will remove this in the next release."

We maintain all three formats simultaneously because we believe you should never be forced to rewrite a working configuration just to upgrade the binary. You can upgrade rsyslog to get the latest security fixes and performance improvements while your 10-year-old ``syslog.conf`` continues to work exactly as it always has.

Our Documentation Approach
==========================

Our documentation reflects this additive philosophy. Because we rarely remove features, our feature set is a permanent, growing collection.

*   **Version Numbers**: When you see a version note (e.g., "Available since 8.1901"), it tells you when a feature was *introduced*.
*   **No Version Note**: If a feature has no version note, you can assume it has been a stable part of rsyslog for a very long time.

We design our features to be permanent. Once something is in rsyslog, we intend to support it.

Conclusion: The Rsyslog Way
===========================

The trade-off for this extreme stability is a slightly steeper initial learning curve. You have to learn to distinguish between the "old way" and the "new way" when reading documentation.

But the reward is a system you can trust. A system that doesn't break your world with every update. A system that respects the work you've already done. That is the rsyslog way.
