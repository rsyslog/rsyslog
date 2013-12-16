`back <rsyslog_conf_modules.html>`_

Input Module to Generate Periodic Statistics of Internal Counters
=================================================================

**Module Name:    impstats**

**Available since:**\ 5.7.0+, 6.1.1+

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

This module provides periodic output of rsyslog internal counters. Note
that the whole statistics system is currently under development. So
availabilty and format of counters may change and is not yet stable (so
be prepared to change your trending scripts when you upgrade to a newer
rsyslog version).

The set of available counters will be output as a set of syslog
messages. This output is periodic, with the interval being configurable
(default is 5 minutes). Be sure that your configuration records the
counter messages (default is syslog.info).

Note that loading this module has impact on rsyslog performance.
Depending on settings, this impact may be noticable (for high-load
environments).

The rsyslog website has an updated overview of available `rsyslog
statistic counters <http://rsyslog.com/rsyslog-statistic-counter/>`_.

**Configuration Directives**:

-  $PStatInterval <Seconds>
    Sets the interval, in **seconds** at which messages are generated.
   Please note that the actual interval may be a bit longer. We do not
   try to be precise and so the interval is actually a sleep period
   which is entered after generating all messages. So the actual
   interval is what is configured here plus the actual time required to
   generate messages. In general, the difference should not really
   matter.
-  $PStatFacility <numerical facility>
    The numerical syslog facility code to be used for generated
   messages. Default is 5 (syslog).This is useful for filtering
   messages.
-  $PStatSeverity <numerical severity>
    The numerical syslog severity code to be used for generated
   messages. Default is 6 (info).This is useful for filtering messages.

**Caveats/Known Bugs:**

-  This module MUST be loaded right at the top of rsyslog.conf,
   otherwise stats may not get turned on in all places.
-  experimental code

**Sample:**

This activates the module and records messages to /var/log/rsyslog-stats
in 10 minute intervals:

$ModLoad impstats $PStatInterval 600 $PStatSeverity 7 syslog.debug
/var/log/rsyslog-stats

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2010 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
