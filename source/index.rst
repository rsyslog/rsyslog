Welcome to Rsyslog
==================

Rsyslog\ [1]_ is an enhanced syslogd suitable both for small systems as well 
as large enterprises that supports, among others, :doc:`MySQL <tutorials/database>`
, :doc:`PostgreSQL <tutorials/database>`, :doc:`failover log
destinations <tutorials/failover_syslog_server>`,
syslog/tcp, fine grain output format control, high precision timestamps,
queued operations and the ability to filter on any message part.

Rsyslog supports plain old syslog.conf format, except that
the config file is now called rsyslog.conf. This should help you get
started quickly. To use the more advanced features, you need to learn
a bit about its new features. When you change the configuration, remember 
to restart rsyslogd, because otherwise the newly added configurations settings 
will not be loaded.

Compatability
-------------

Rsyslog is compatible to stock sysklogd and can be used as a drop-in
replacement. Its :doc:`features <features>` make it suitable
for enterprise-class, :doc:`encryption protected syslog <tutorials/tls>` 
relay chains while at the same time being very easy to setup for the
novice user. Knowing the difficulty in creating a well defined Rsyslog 
environment that meets the needs of enterprise users, there is
also professional rsyslog support\ [5]_ available directly from the source!

Sponsors and Community
----------------------

Please visit the Rsyslog Sponsor's Page\ [4]_ to honor the project sponsors or 
become one yourself! We are very grateful for any help towards the project 
goals.

Visit the Rsyslog Status Page\ [2]_ to obtain current version information and 
project status.

If you like rsyslog, you might want to lend us a helping hand. It
doesn't require a lot of time - even a single mouse click helps. Learn
:doc:`how to help the rsyslog project <how2help>`. Due to popular
demand, there is now a 
:doc:`side-by-side comparison between rsyslog and syslog-ng <whitepapers/rsyslog_ng_comparison>`.

If you are upgrading from rsyslog v2 or stock sysklogd, `be sure to read
the rsyslog v3 compatibility notes <v3compatibility.html>`_, and if you
are upgrading from v3, read the `rsyslog v4 compatibility
notes <v4compatibility.html>`_ and if you upgrade from v4, read the
:doc:`rsyslog v5 compatibility notes <v5compatibility>`.

Rsyslog will work even if you do not read the doc, but doing so will
definitely improve your experience.

Manual
------

.. toctree::
   :maxdepth: 3

   history
   licensing
   contributors
   how2help
   features
   installation/index
   queues
   messageparser
   configuration/index
   drivers/index
   rainerscript/rainerscript
   troubleshooting/index
   development/index
   tutorials/index
   free_support
   compatibility/index
   bugs

Related Links
=============

.. [1] `Rsyslog Website <http://www.rsyslog.com/>`_ 
.. [2] `Project Status Page <http://www.rsyslog.com/status>`_
.. [3] `Rsyslog Change Log <http://www.rsyslog.com/Topic4.phtml>`_
.. [4] `Rsyslog Sponsor's Page <http://www.rsyslog.com/sponsors>`_
.. [5] `Professional Rsyslog Support <http://www.rsyslog.com/professional-services>`_