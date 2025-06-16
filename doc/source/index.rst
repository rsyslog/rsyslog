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
It supports, among others, :doc:`MariaDB/MySQL <tutorials/database>`,
:doc:`PostgreSQL <tutorials/database>`,
:doc:`failover log destinations <tutorials/failover_syslog_server>`,
ElasticSearch, syslog/tcp transport, fine grain output format control,
high precision timestamps,
queued operations and the ability to filter on any message part.

Manual
------
.. toctree::
   :maxdepth: 2

   installation/index
   configuration/index
   containers/index
   troubleshooting/index
   faq/index
   concepts/index
   examples/index
   tutorials/index
   development/index
   historical/index

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

Sponsors and Community
----------------------

Please visit the rsyslog `Sponsor's Page`_ to honor the project sponsors or
become one yourself! We are very grateful for any help towards the project
goals.

If you like rsyslog, you might want to lend us a helping hand. It
doesn't require a lot of time - even a single mouse click helps. Learn
:doc:`how2help`.

.. _Sponsor's Page: http://www.rsyslog.com/sponsors

Contributing to the docs
------------------------

This documentation is hosted on `github
<https://github.com/rsyslog/rsyslog-doc>`_. If you find something to be
improved, please feel free to fork and file a patch request with your
improvements. There is also a Button 'Edit in GitHub' on every documentation
page beginning with the main `docs page
<https://www.rsyslog.com/doc/v8-stable/>`_ that allows easy access.

.. only:: dev

    Built on |today| from branch |DOC_BRANCH|, commit |DOC_COMMIT|.
