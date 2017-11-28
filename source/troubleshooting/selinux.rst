Troubleshooting SELinux-Related Issues
======================================

SELinux by its very nature can block many features of rsyslog (or any
other process, of course), even when run under root. Actually, this is
what it is supposed to do, so there is nothing bad about it.

If you suspect that some issues stems back to SELinux configuration,
do the following:

* *temporarily* disable SELinux
* restart rsyslog
* retry the operation

If it now succeeds, you know that you have a SELinux policy issue.
The solution here is **not** to keep SELinux disabled. Instead do:

* reenable SELinux (set back to previous state, whatever that was)
* add proper SELinux policies for what you want to do with rsyslog

With SELinux running, restart rsyslog
$ sudo audit2allow -a
audit2allow will read the audit.log and list any SELinux infractions, namely the rsyslog infractions
$ sudo audit2allow -a -M <FRIENDLY_NAME_OF_MODULE>.pp
audit2allow will create a module allowing all previous infractions to have access
$ sudo semodule -i <FRIENDLY_NAME_OF_MODULE>.pp
Your module is loaded! Restart rsyslog and continue to audit until no more infractions are detected and rsyslog has proper access. Additionally, you can save these modules and install them on future machines where rsyslog will need the same access.
