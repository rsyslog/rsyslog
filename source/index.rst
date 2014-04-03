Welcome to Rsyslog
==================

`Rsyslog <http://www.rsyslog.com/>`_ is a **r**\ ocket-fast **sys**\ tem for **log** processing.
It offers high-performance, great security features and a modular design.
While it started as a regular syslogd, rsyslog has evolved into a kind of
**swiss army knife of logging**, being able to

- accept inputs from a wide variety of sources,
- transform them,
- and output the results to diverse destinations.

Rsyslog has a strong enterprise focus but also scales down to small
systems.
It supports, among others, :doc:`MySQL <tutorials/database>`
, :doc:`PostgreSQL <tutorials/database>`, :doc:`failover log
destinations <tutorials/failover_syslog_server>`,
syslog/tcp transport, fine grain output format control, high precision timestamps,
queued operations and the ability to filter on any message part.

Rsyslog supports the original syslog.conf configuration formatting in it's
rsyslog.conf config file. This should help with getting an environment started 
quickly. To use the more advanced features, you need to learn
a bit about rsyslog's new features. 

Compatibility
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
:doc:`how to help the rsyslog project <how2help>`.

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

Manual
------
.. toctree::
   :maxdepth: 3
   
   configuration/index
   installation/index
   drivers/index
   rainerscript/index
   messageparser
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

