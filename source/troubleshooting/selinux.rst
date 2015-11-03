Troubleshooting SELinux-Related Issues
======================================

SELinux by its very nature can block many features of rsyslog (or any
other process, of course), even when run under root. Actually, this is
what it is supposed to do, so there is nothing bad about it.

If you suspect that some issues stems back to SELinux configuration,
do the following:

* *temporarily* disabling SELinux
* restart rsyslog
* retry the operation

If it now succeeds, you know that you have a SELinux policy issue.
The solution here is **not** to keep SELinux disabled. Instead do:

* reenable SELinux (set back to previous state, whatever that was)
* add proper SELinux policies for what you want to do with rsyslog
