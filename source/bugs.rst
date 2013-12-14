rsyslog bugs and annoyances
===========================

**This page lists the known bugs rsyslog has to offer.**  It lists old
and esoteric bugs. A live list of bugs is contained in our bugzilla.
**Please visit
`http://www.rsyslog.com/bugs <http://www.rsyslog.com/bugs>`_** to see
what we have. There, you can also open your own bug report if you think
you found one.

This list has last been updated on 2008-02-12 by `Rainer
Gerhards <http://www.adiscon.com/en/people/rainer-gerhards.php>`_.

rsyslogd
========

EQUALLY-NAMED TEMPLATES
-----------------------

If multiple templates with the SAME name are created, all but the first
definition is IGNORED. So you can NOT (yet) replace a template
definition. I also strongly doubt I will ever support this, because it
does not make an awful lot of sense (after all, why not use two template
names...).

WALLMSG FORMAT (\* selector)
----------------------------

This format is actually not 100% compatible with stock syslogd - the
date is missing. Will be fixed soon and can also be fixed just via the
proper template. Anyone up for this? ;)

MULTIPLE INSTANCES
------------------

If multiple instances are running on a single machine, the one with the
-r switch must start first. Also, UDP-based syslog forwarding between
the instances does not work. Use TCP instead.
