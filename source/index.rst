Welcome to rsyslog
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

.. toctree::
   :maxdepth: 3

   queues
   configuration/index
   development/index
