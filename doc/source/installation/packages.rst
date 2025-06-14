Installing rsyslog from Package
===============================
Installing from package is usually the most convenient way to install
rsyslog. Usually, the regular package manager can be used.

Package Availability
--------------------

**Rsyslog is included in all major distributions.** So you do not
necessarily need to take care of where packages can be found - they
are "just there". Unfortunately, the distros provide often rather old
versions. This is especially the case for so-called enterprise
distributions.

As long as you do not run into trouble with one of these old versions, using
the distribution-provided packages is easy and a good idea. If you need
new features, better performance and sometimes even a fix for a bug that
the distro did not backport, you can use alternative packages. Please note
that the project team does not support outdated versions. While we probably
can help with simple config questions, for anything else we concentrate on
current versions.

The rsyslog project offers current packages for a number of major distributions.
More information about these can be found at the |RsyslogPackageDownloads|_
page.

If you do not find a suitable package for your distribution, there is no
reason to panic. You can use official rsyslog docker containers or
install rsyslog from the source tarball.

.. seealso::

   - :doc:`rsyslog_docker`
   - :doc:`install_from_source`

Package Structure
-----------------
Almost all distributions package rsyslog in multiple packages. This is also
the way Adiscon packages are created. The reason is that rsyslog has so many
input and output plugins that enable it to connect to different systems
like MariaDB/MySQL, Kafka, ElasticSearch and so on. If everything were provided in a
single gigantic package, you would need to install all of these dependencies,
even though they are mostly not needed.

For that reason, rsyslog comes with multiple packages:

* *core package* (usually just called "rsyslog") - this contains core
  technology that is required as a base for all other packages. It also
  contains modules like the file writer or syslog forwarder that is extremely
  often used and has little dependencies.
* *feature package* (usually called "rsyslog-feature") - there are
  multiple of these packages. What exactly is available and how it is
  named depends on the distro. This unfortunately is a bit inconsistent.
  Usually, it is a good guess that the package is intuitively named,
  e.g. "rsyslog-mysql" for the MariaDB/MySQL component and "rsyslog-elasticsearch"
  for ElasticSearch support. If in doubt, it is suggested to use the
  distro's package manager and search for "rsyslog*".

Contributing
------------
**Packaging is a community effort.** If you would like to see support for an
additional distribution and know how to build packages, please consider
contributing to the project and joining the packaging team. Also, rsyslog's
presence on github also contains the sources for the currently
maintained packages. They can be found at the |GitHubSourceProject|_.
