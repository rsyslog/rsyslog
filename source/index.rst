Welcome to Rsyslog
==================

**`Rsyslog <http://www.rsyslog.com/>`_ is an enhanced syslogd suitable
both for small systems as well as large enterprises.**

This page provide a few quick pointers which hopefully make your
experience with rsyslog a pleasant one. These are

-  **Most importantly, the `rsyslog manual <manual.html>`_** - this
   points to locally installed documentation which exactly matches the
   version you have installed. It is highly suggested to at least
   briefly look over these files.
-  The `rsyslog web site <http://www.rsyslog.com>`_ which offers
   probably every information you'll ever need (ok, just kidding...).
-  The `project status page <http://www.rsyslog.com/status>`_ provides
   information on current releases
-  and the `troubleshooting guide <troubleshoot.html>`_ hopefully helps
   if things do not immediately work out

In general, rsyslog supports plain old syslog.conf format, except that
the config file is now called rsyslog.conf. This should help you get
started quickly. To do the really cool things, though, you need to learn
a bit about its new features. The man pages offer a bare minimum of
information (and are still quite long). Read the `html
documentation <manual.html>`_ instead. When you change the
configuration, remember to restart rsyslogd, because otherwise it will
not use your new settings (and you'll end up totally puzzled why this
great config of yours does not even work a bit...;))
=======
rsyslog\ [1]_ is an enhanced syslogd scalable for small systems as well 
as large enterprises. rsyslog supports, among others, :doc:`MySQL <tutorials/database>`
, :doc:`PostgreSQL <tutorials/database>`, :doc:`failover log
destinations <tutorials/failover_syslog_server>`,
syslog/tcp transport, fine grain output format control, high precision timestamps,
queued operations and the ability to filter on any message part.

Rsyslog supports the original syslog.conf configuration formatting in it's
rsyslog.conf config file. This should help with getting an environment started 
quickly. To use the more advanced features, you need to learn
a bit about rsyslog's new features. 

Compatability
-------------

rsyslog is compatible to stock sysklogd and can be used as a drop-in
replacement. Its :doc:`features <features>` make it suitable
for enterprise-class, :doc:`encryption protected syslog <tutorials/tls>` 
relay chains while at the same time being very easy to setup for the
novice user. Knowing the difficulty in creating a well defined Rsyslog 
environment that meets the needs of enterprise users, there is
also professional rsyslog support\ [5]_ available directly from the source!

Sponsors and Community
----------------------

Please visit the rsyslog Sponsor's Page\ [4]_ to honor the project sponsors or 
become one yourself! We are very grateful for any help towards the project 
goals.

Visit the Rsyslog Status Page\ [2]_ to obtain current version information and 
project status.

If you like rsyslog, you might want to lend us a helping hand. It
doesn't require a lot of time - even a single mouse click helps. Learn
:doc:`how to help the rsyslog project <how2help>`. Due to popular
demand, there is now a 
:doc:`side-by-side comparison between rsyslog and syslog-ng <whitepapers/rsyslog_ng_comparison>`.

If you are upgrading from rsyslog v2 or stock sysklogd, be sure to read
the :doc:`rsyslog v3 compatibility notes <compatibility/v3compatibility>`, and if you
are upgrading from v3, read the `rsyslog v4 compatibility
notes <v4compatibility.html>`_ and if you upgrade from v4, read the
:doc:`rsyslog v5 compatibility notes <compatibility/v5compatibility>`.

Rsyslog will work even if you do not read the doc, but doing so will
definitely improve your experience.

Reference
---------

.. toctree::
   :maxdepth: 1

   history
   licensing
   how2help
   community
   features
   proposals/index
   whitepapers/index
   free_support
   compatibility/index
   contributors
   bugs

Manual
------
.. toctree::
   :maxdepth: 3
   
   installation/index
   drivers/index
   rainerscript/index
   messageparser
   configuration/index
   queues
   troubleshooting/index
   development/index
   tutorials/index
   

Related Links
-------------

.. [1] `rsyslog Website <http://www.rsyslog.com/>`_ 
.. [2] `Project Status Page <http://www.rsyslog.com/status>`_
.. [3] `rsyslog Change Log <http://www.rsyslog.com/Topic4.phtml>`_
.. [4] `rsyslog Sponsor's Page <http://www.rsyslog.com/sponsors>`_
.. [5] `Professional rsyslog Support <http://www.rsyslog.com/professional-services>`_
.. [6] `Regular expression checker/generator tool for rsyslog <http://www.rsyslog.com/tool-regex>`_
.. [7] `Rainer's twitter feed <http://twitter.com/rgerhards>`_
.. [8] `Rainer's Blog <http://blog.gerhards.net/>`_

