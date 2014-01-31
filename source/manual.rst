RSyslog - Documentation
=======================

**`Rsyslog <http://www.rsyslog.com/>`_ is an enhanced syslogd
supporting, among others, `MySQL <rsyslog_mysql.html>`_, PostgreSQL,
`failover log
destinations <http://wiki.rsyslog.com/index.php/FailoverSyslogServer>`_,
syslog/tcp, fine grain output format control, high precision timestamps,
queued operations and the ability to filter on any message part.** It is
quite compatible to stock sysklogd and can be used as a drop-in
replacement. Its `advanced features <features.html>`_ make it suitable
for enterprise-class, `encryption protected syslog <rsyslog_tls.html>`_
relay chains while at the same time being very easy to setup for the
novice user. And as we know what enterprise users really need, there are
also `rsyslog professional
services <http://www.rsyslog.com/professional-services>`_ available
directly from the source!

**Please visit the `rsyslog sponsor's
page <http://www.rsyslog.com/sponsors>`_ to honor the project sponsors
or become one yourself!** We are very grateful for any help towards the
project goals.

**This documentation is for version 7.5.7 (devel branch) of rsyslog.**
Visit the *`rsyslog status page <http://www.rsyslog.com/status>`_* to
obtain current version information and project status.

**If you like rsyslog, you might want to lend us a helping hand.**\ It
doesn't require a lot of time - even a single mouse click helps. Learn
`how to help the rsyslog project <how2help.html>`_. Due to popular
demand, there is now a `side-by-side comparison between rsyslog and
syslog-ng <rsyslog_ng_comparison.html>`_.

If you are upgrading from rsyslog v2 or stock sysklogd, `be sure to read
the rsyslog v3 compatibility notes <v3compatibility.html>`_, and if you
are upgrading from v3, read the `rsyslog v4 compatibility
notes <v4compatibility.html>`_, if you upgrade from v4, read the
`rsyslog v5 compatibility notes <v5compatibility.html>`_, and if you
upgrade from v5, read the `rsyslog v6 compatibility
notes <v6compatibility.html>`_. if you upgrade from v6, read the
`rsyslog v7 compatibility notes <v7compatibility.html>`_.

Rsyslog will work even if you do not read the doc, but doing so will
definitely improve your experience.

**Follow the links below for the**

-  `troubleshooting rsyslog problems <troubleshoot.html>`_
-  `configuration file format (rsyslog.conf) <rsyslog_conf.html>`_
-  `a regular expression checker/generator tool for
   rsyslog <http://www.rsyslog.com/tool-regex>`_
-  `property replacer, an important core
   component <property_replacer.html>`_
-  `rsyslog bug list <bugs.html>`_
-  `understanding rsyslog message parsers <messageparser.html>`_
-  `backgrounder on generic syslog application
   design <generic_design.html>`_
-  `description of rsyslog modules <modules.html>`_
-  `rsyslog packages <rsyslog_packages.html>`_

**To keep current on rsyslog development, follow `Rainer's twitter
feed <http://twitter.com/rgerhards>`_.**

**We have some in-depth papers on**

-  `installing rsyslog <install.html>`_
-  `obtaining rsyslog from the source
   repository <build_from_repo.html>`_
-  `rsyslog and IPv6 <ipv6.html>`_ (which is fully supported)
-  `native TLS encryption for syslog <rsyslog_secure_tls.html>`_
-  `using multiple rule sets in rsyslog <multi_ruleset.html>`_
-  `ssl-encrypting syslog with stunnel <rsyslog_stunnel.html>`_
-  `writing syslog messages to MySQL (and other databases as
   well) <rsyslog_mysql.html>`_
-  `writing syslog messages to PostgreSQL (and other databases as
   well) <rsyslog_pgsql.html>`_
-  `writing massive amounts of syslog messages to a
   database <rsyslog_high_database_rate.html>`_
-  `reliable forwarding to a remote
   server <rsyslog_reliable_forwarding.html>`_
-  `using php-syslog-ng with rsyslog <rsyslog_php_syslog_ng.html>`_
-  `recording the syslog priority (severity and facility) to the log
   file <rsyslog_recording_pri.html>`_
-  `preserving syslog sender over
   NAT <http://www.rsyslog.com/Article19.phtml>`_ (online only)
-  `an overview and howto of rsyslog gssapi support <gssapi.html>`_
-  `debug support in rsyslog <debug.html>`_
-  Developer Documentation

   -  `building rsyslog from the source
      repository <build_from_repo.html>`_
   -  `writing rsyslog output plugins <dev_oplugins.html>`_
   -  `the rsyslog message queue object (developer's
      view) <dev_queue.html>`_

Our `rsyslog history <history.html>`_ page is for you if you would like
to learn a little more on why there is an rsyslog at all. If you are
interested why you should care about rsyslog at all, you may want to
read Rainer's essay on "`why the world needs another
syslogd <http://rgerhards.blogspot.com/2007/08/why-does-world-need-another-syslogd.html>`_\ ".

Documentation is added continuously. Please note that the documentation
here matches only the current version of rsyslog. If you use an older
version, be sure to use the doc that came with it.

**You can also browse the following online resources:**

-  the `rsyslog wiki <http://wiki.rsyslog.com/>`_, a community resource
   which includes `rsyslog configuration
   examples <http://wiki.rsyslog.com/index.php/Configuration_Samples>`_
-  `rsyslog online documentation (most current version
   only) <http://www.rsyslog.com/module-Static_Docs-view-f-manual.html.phtml>`_
-  `rsyslog discussion forum - use this for technical
   support <http://kb.monitorware.com/rsyslog-f40.html>`_
-  `rsyslog video tutorials <http://www.rsyslog.com/Topic8.phtml>`_
-  `rsyslog change log <http://www.rsyslog.com/Topic4.phtml>`_
-  `rsyslog FAQ <http://www.rsyslog.com/Topic3.phtml>`_
-  `syslog device configuration
   guide <http://www.monitorware.com/en/syslog-enabled-products/>`_
   (off-site)
-  `rsyslog discussion forum - use this for technical
   support <http://www.rsyslog.com/PNphpBB2.phtml>`_
-  `deutsches rsyslog
   forum <http://kb.monitorware.com/rsyslog-f49.html>`_ (forum in German
   language)

And don't forget about the `rsyslog mailing
list <http://lists.adiscon.net/mailman/listinfo/rsyslog>`_. If you are
interested in the "backstage", you may find
`Rainer <http://www.gerhards.net/rainer>`_'s
`blog <http://blog.gerhards.net/>`_ an interesting read (filter on
syslog and rsyslog tags). Or meet `Rainer Gerhards at
Facebook <http://www.facebook.com/people/Rainer-Gerhards/1349393098>`_
or `Google+ <https://plus.google.com/112402185904751517878/posts>`_. If
you would like to use rsyslog source code inside your open source
project, you can do that without any restriction as long as your license
is GPLv3 compatible. If your license is incompatible to GPLv3, you may
even be still permitted to use rsyslog source code. However, then you
need to look at the way `rsyslog is licensed <licensing.html>`_.

Feedback is always welcome, but if you have a support question, please
do not mail Rainer directly (`why not? <free_support.html>`_) - use the
`rsyslog mailing
list <http://lists.adiscon.net/mailman/listinfo/rsyslog>`_ or `rsyslog
formum <http://kb.monitorware.com/rsyslog-f40.html>`_ instead.
