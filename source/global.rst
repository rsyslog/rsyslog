global() configuration object
=============================

The global configuration object permits to set global parameters. Note
that each parameter can only be set once and cannot be re-set
thereafter. If a parameter is set multiple times, the behaviour is
unpredictable.

The following paramters can be set:

-  **action.reportSuspension** - binary, default "on", v7.5.8+
   If enabled ("on") action will log message under *syslog.\** when an
   action suspends or resume itself. This usually happens when there are
   problems connecting to backend systems. If disabled ("off"), these
   messages are not generated. These messages can be useful in detecting
   problems with backend systems. Most importantly, frequent suspension
   and resumption points to a problem area.
- **action.reportSuspensionContinuation** - binary, default "off", v7.6.1+, v8.2.0+
  If enabled ("on") the action will not only report the first suspension but
  each time the suspension is prolonged. Otherwise, the follow-up messages
  are not logged. If this setting is set to "on", action.reportSuspension is
  also automaticaly turned "on".
-  workDirectory
-  dropMsgsWithMaliciousDNSPtrRecords
-  localHostname
-  preserveFQDN
-  defaultNetstreamDriverCAFile
    For `TLS syslog <http://www.rsyslog.com/doc/rsyslog_secure_tls.html>`_, the CA certificate that can verify the machine keys and certs (see below)
-  defaultNetstreamDriverKeyFile
    Machine private key

- defaultNetstreamDriverCertFile - v8.2.0+

  Machine public key (certificate)

- **processInternalMessages** binary (on/off), available in v7.4.9+, 7.6.0+, 8.1.5+

  This tell rsyslog if it shall process internal messages itself. This is
  the default mode of operations ("on") and usually the best. However, if
  this (instance) of rsyslog is not the main instance and there is another
  main logging system, rsyslog internal messages will not be inserted into
  the main instance's syslog stream. To do this, set the parameter to "off",
  in which case rsyslog will send messages to the system log sink (and if
  it is the only instance, receive them back from there). This also works
  with systemd journal and will make rsyslog messages show up in the
  systemd status control information. 

- **stdlog.channelspec** - v8.2.0+

  Permits to set the liblogging-stdlog channel specifier string. This
  in turn permits to send rsyslog log messages to a destination different
  from the system default. Note that this parameter has only effect if
  *processInternalMessages* is set to "off". Otherwise it is silently
  ignored.

-  defaultNetstreamDriver
    Set it to "gtls" to enable TLS for `TLS syslog <http://www.rsyslog.com/doc/rsyslog_secure_tls.html>`_
-  maxMessageSize
-  **debug.onShutdown** available in 7.5.8+
    If enabled ("on"), rsyslog will log debug messages when a system
   shutdown is requested. This can be used to track issues that happen
   only during shutdown. During normal operations, system performance is
   NOT affected.
   Note that for this option to be useful, the debug.logFile parameter
   must also be set (or the respective environment variable).
-  **debug.logFile** available in 7.5.8+
    This is used to specify the debug log file name. It is used for all
   debug output. Please note that the RSYSLOG\_DEBUGLOG environment
   variable always **overrides** the value of debug.logFile.

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`RainerScript
reference <rainerscript.html>`_\ ] [`rsyslog
site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2013 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under ASL 2.0 or
higher.
