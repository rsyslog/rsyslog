.. meta::
   :description: Troubleshoot rsyslog service sandboxing, AppArmor, SELinux, and permission issues affecting external programs and file access.
   :keywords: rsyslog, troubleshooting, systemd, sandbox, AppArmor, SELinux, omprog, sudo, permissions

.. _troubleshooting_service_sandboxing:

Service sandboxing and external programs
========================================

.. summary-start

rsyslog runs inside the execution context provided by the init system and
distribution security policy. External programs started by modules such as
``omprog`` inherit that context, so root inside ``rsyslog.service`` may still be
restricted by systemd sandboxing, AppArmor, SELinux, Linux capabilities, or
private mount namespaces.

.. summary-end

Why root may still be restricted
--------------------------------

Many distributions harden ``rsyslog.service`` with systemd options such as
``NoNewPrivileges=``, ``ProtectHome=``, ``ProtectSystem=``, ``PrivateTmp=``,
``PrivateDevices=``, ``SystemCallFilter=``, and ``CapabilityBoundingSet=``.
These settings apply to rsyslog and to helper programs that rsyslog starts.

This means that a command can work from an interactive root shell but fail when
started by rsyslog. Common symptoms include:

* an ``omprog`` helper cannot execute or cannot read files it needs;
* ``sudo`` fails even though the sudoers rule looks correct;
* a path under ``/home`` is not visible to the helper;
* output written to ``/tmp`` does not appear in the host ``/tmp`` because the
  service uses a private temporary directory;
* file writes outside common log paths fail despite root ownership.

The exact defaults are distribution-specific. Check the active unit on the
affected host instead of assuming that it matches another system.

Check the service context
-------------------------

Use systemd to inspect the active unit and its hardening settings:

.. code-block:: bash

   systemctl cat rsyslog.service
   systemd-analyze security rsyslog.service

Then inspect rsyslog and kernel security logs for the actual denial:

.. code-block:: bash

   journalctl -u rsyslog.service
   journalctl -k

On SELinux systems, also review audit denials. See
:doc:`SELinux troubleshooting <selinux>` for the SELinux-specific workflow.
On AppArmor systems, check the AppArmor status and kernel or journal messages
for denied accesses.

Using sudo from omprog
----------------------

Running ``sudo`` from an ``omprog`` helper can be convenient, but sudo is often
affected by service hardening. A passwordless sudoers rule for a specific
command is not enough if the systemd sandbox prevents privilege changes or
blocks access to files that sudo needs.

If you choose this approach, use both of these controls:

* a narrow sudoers rule for the exact command and arguments; avoid broad
  ``NOPASSWD: ALL`` rules;
* a systemd drop-in that relaxes only the sandbox setting required by that
  command.

Create a drop-in with:

.. code-block:: bash

   sudo systemctl edit rsyslog.service

Depending on the failure, ``NoNewPrivileges=no`` may be required for sudo. If
the helper or its inputs are below a home directory, ``ProtectHome=`` may also
be relevant. After changing the service unit, reload and restart:

.. code-block:: bash

   sudo systemctl daemon-reload
   sudo systemctl restart rsyslog

Prefer avoiding privilege changes
---------------------------------

The cleaner design is to avoid privilege transitions from inside rsyslog
helpers. Place helper programs in a location intended for rsyslog-owned helper
code, such as ``/usr/libexec/rsyslog``, and set file ownership and permissions
so the helper can access only what it needs.

For more isolated setups, run a small dedicated service as the target user and
let rsyslog communicate with it through a file, pipe, socket, or another
explicit interface. This usually takes more setup work, but it avoids weakening
the rsyslog service sandbox.
