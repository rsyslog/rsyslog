FAQ: some general topics often asked
====================================


.. _faq_message_duplication:

FAQ: Message Duplication with rsyslog
-------------------------------------

**Q: Why do I see message duplication with rsyslog?**

**A:** rsyslog follows an "at least once" delivery principle, meaning it's possible to encounter some message duplication. This typically occurs when forwarding data and the connection is interrupted. This is often the case when load balancers are involved.

One common scenario involves the `omfwd` module with TCP. If the connection breaks, `omfwd` cannot precisely determine which messages were successfully stored by the remote peer, leading to potential resending of more messages than necessary. To mitigate this, consider using the `omrelp` module, which provides reliable event logging protocol (RELP) and ensures exact message delivery without duplication.

While the `omfwd` case is common, other configurations might also cause duplication. Always ensure that your queue and retry settings are properly configured to minimize this issue.


.. _faq_root_command_fails_under_rsyslog:

FAQ: A command works as root but fails when rsyslog starts it
--------------------------------------------------------------

**Q: Why does a script or command work from an interactive root shell but fail
when rsyslog starts it, for example through** ``omprog`` **?**

**A:** rsyslog usually runs inside a service context created by the init system
and the distribution's security policy. Programs started by rsyslog inherit that
context. The systemd sandbox, AppArmor, SELinux, Linux capabilities, private
``/tmp`` directories, and home-directory protection can all change what the
child process may read, write, execute, or change into.

This is also why ``sudo`` may fail inside an ``omprog`` helper even though
``rsyslogd`` is running as root and the command works from a root shell. Inspect
the active ``rsyslog.service`` unit and security logs before changing the
rsyslog configuration. See
:doc:`service sandboxing and external programs <../troubleshooting/service_sandboxing>`
for the diagnostic workflow.
