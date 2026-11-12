.. SPDX-License-Identifier: Apache-2.0

.. index:: ! imklog; Ruleset

.. _param-imklog-ruleset:

Ruleset
=======

.. summary-start

**Default:** default ruleset

**Type:** string

**Description:**

Binds all kernel log messages read by imklog to the named
:doc:`ruleset <../../concepts/multi_ruleset>`. imklog has no ``input()``
objects, so this module-level parameter is the only way to select a ruleset
for kernel messages. If it is not set, or the ruleset does not exist (an
error is logged in that case), messages go to the default ruleset.

.. summary-end

Example:

.. code-block:: rsyslog

   module(load="imklog" ruleset="kernel_debug")

   ruleset(name="kernel_debug") {
       action(type="omfile" file="/var/log/kernel.log")
   }

Messages that reach rsyslog through other inputs, such as kernel messages
forwarded by systemd-journald to ``imuxsock``, are not affected by this
parameter; bind those inputs separately.
