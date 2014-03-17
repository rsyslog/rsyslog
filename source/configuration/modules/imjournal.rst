`back <rsyslog_conf_modules.html>`_

Systemd Journal Input Module
============================

**Module Name:    imjournal**

**Author:**\ Milan Bartos <mbartos@redhat.com> (This module is not
project-supported)

**Description**:

Provides the ability to import structured log messages from systemd
journal to syslog.

Note that this module reads the journal database, what is considered a
relativly performance-intense operation. As such, the performance of a
configuration utilizing this module may be notably slower then when
using `imuxsock <imuxsock.html>`_. The journal provides imuxsock with a
copy of all "classical" syslog messages, however, it does not provide
structured data. If the latter is needed, imjournal must be used.
Otherwise, imjournal may be simply replaced by imuxsock.

We suggest to check out our short presentation on `rsyslog journal
integration <http://youtu.be/GTS7EuSdFKE>`_ to learn more details of
anticipated use cases.

**Warning:** Some versions of systemd journal have problems with
database corruption, which leads to the journal to return the same data
endlessly in a thight loop. This results in massive message duplication
inside rsyslog probably resulting in a denial-of-service when the system
ressouces get exhausted. This can be somewhat mitigated by using proper
rate-limiters, but even then there are spikes of old data which are
endlessly repeated. By default, ratelimiting is activated and permits to
process 20,000 messages within 10 minutes, what should be well enough
for most use cases. If insufficient, use the parameters described below
to adjust the permitted volume. **It is strongly recommended to use this
plugin only if there is hard need to do so.**

**Configuration Directives**:

**Module Directives**

-  **PersistStateInterval** number-of-messages
   This is a global setting. It specifies how often should the journal
   state be persisted. The persists happens after each
   *number-of-messages*. This option is useful for rsyslog to start
   reding from the last journal message it read.
-  **StateFile** /path/to/file
   This is a global setting. It specifies where the state file for
   persisting journal state is located.
-  **ratelimit.interval** seconds (default: 600)
   Specifies the interval in seconds onto which rate-limiting is to be
   applied. If more than ratelimit.burst messages are read during that
   interval, further messages up to the end of the interval are
   discarded. The number of messages discarded is emitted at the end of
   the interval (if there were any discards).
   Setting this to value zero turns off ratelimiting. Note that it is
   **not recommended to turn of ratelimiting**, except that you know for
   sure journal database entries will never be corrupted. Without
   ratelimiting, a corrupted systemd journal database may cause a kind
   of denial of service (we are stressing this point as multiple users
   have reported us such problems with the journal database -
   information current as of June 2013).
-  **ratelimit.burst** messages (default: 20000)
   Specifies the maximum number of messages that can be emitted within
   the ratelimit.interval interval. For futher information, see
   description there.
-  **IgnorePreviousMessages** [**off**/on]
   This option specifies whether imjournal should ignore messages
   currently in journal and read only new messages. This option is only
   used when there is no StateFile to avoid message loss.
-  **DefaultSeverity** <severity>
   Some messages comming from journald don't have the SYSLOG_PRIORITY
   field. These are typically the messages logged through journald's
   native API. This option specifies the default severity for these
   messages. Can be given either as a name or a number. Defaults to 'notice'.
-  **DefaultFacility** <facility>
   Some messages comming from journald don't have the SYSLOG_FACILITY
   field. These are typically the messages logged through journald's
   native API. This option specifies the default facility for these
   messages. Can be given either as a name or a number. Defaults to 'user'.



**Caveats/Known Bugs:**

-  As stated above, a corrupted systemd journal database can cause major
   problems, depending on what the corruption results in. This is beyond
   the control of the rsyslog team.

**Sample:**

The following example shows pulling structured imjournal messages and
saving them into /var/log/ceelog.

module(load="imjournal" PersistStateInterval="100"
StateFile="/path/to/file") #load imjournal module
module(load="mmjsonparse") #load mmjsonparse module for structured logs
template(name="CEETemplate" type="string" string="%TIMESTAMP% %HOSTNAME%
%syslogtag% @cee: %$!all-json%\\n" ) #template for messages
action(type="mmjsonparse") action(type="omfile" file="/var/log/ceelog"
template="CEETemplate")

**Legacy Configuration Directives**:

-  **$imjournalPersistStateInterval**
    Equivalent to: PersistStateInterval
-  **$imjournalStateFile**
    Equivalent to: StateFile
-  **$imjournalRatelimitInterval**
    Equivalent to: ratelimit.interval
-  **$imjournalRatelimitBurst**
    Equivalent to: ratelimit.burst
-  **$ImjournalIgnorePreviousMessages**
    Equivalent to: ignorePreviousMessages
-  **$ImjournalDefaultSeverity**
    Equivalent to: DefaultSeverity
-  **$ImjournalDefaultFacility**
    Equivalent to: DefaultFacility
    

