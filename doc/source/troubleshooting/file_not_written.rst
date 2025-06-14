Output File is not Being Written
================================

Note: current rsyslog versions have somewhat limited error reporting
inside omfile. If a problem persists, you may want to generate a
rsyslog debug log, which often can help you pinpoint the actual root
cause of the problem more quickly.

To learn more about the current state of error reporting, follow
our `bug tracker <https://github.com/rsyslog/rsyslog/issues/548>`_
for this issue.

The following subsections list frequent causes for file writing
problems. You can quickly check this without the need to create a
debug log.

SELinux
-------
This often stems back to **selinux** permission errors, especially
if files outside of the ``/var/log`` directory shall be written
to.

Follow the :doc:`SELinux troubleshooting guide <selinux>`
to check for this condition.

Max Number of Open Files
------------------------
This can also be caused by a too low limit on number of open
file handles, especially when dynafiles are being written.

Note that some versions of systemd limit the process
to 1024 files by default.  The current
set limit can be validated by doing::

  cat /proc/<pid>/limits

and the currently open number of files can be obtained by doing::

  ls /proc/<pid>/fd | wc -l

Also make sure the system-wide max open files is appropriate using::

  sysctl fs.file-max

Some versions of systemd completely ignore
``/etc/security/limits*``. To change limits for a service in systemd, edit
``/usr/lib/systemd/system/rsyslog.service`` and under ``[Service]`` add:
``LimitNOFILE=<new value>``.

Then run::

  systemctl daemon-reload
  systemctl restart rsyslog
