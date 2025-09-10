Installing rsyslog from the source repository
=============================================

In most cases, people install rsyslog either via a package or use an
"official" distribution tarball to generate it. But there may be
situations where it is desirable to build directly from the source
repository. This is useful for people who would like to participate in
development or who would like to use the latest, not-yet-released code.
The later may especially be the case if you are asked to try out an
experimental version.

Building from the repository is not much different than building from the
source tarball, but some files are missing because they are output files
and thus do not belong into the repository.

Obtaining the Source
--------------------

First of all, you need to download the sources. Rsyslog is kept in git.
The "`Where to find the rsyslog source
code <http://www.rsyslog.com/where-to-find-the-rsyslog-source-code/>`_\ "
page on the project site will point you to the current repository
location.

After you have cloned the repository, you are in the main branch by
default. This is where we keep the devel branch. If you need any other
branch, you need to do a "git checkout --track -b branch origin/branch".
For example, the command to check out the beta branch is "git checkout
--track -b beta origin/beta".

Prerequisites
-------------

.. include:: /includes/container_dev_env.inc.rst

To build the compilation system, you need

* GNU autotools (autoconf, automake, ...)
* libtool
* pkg-config

Unfortunately, the actual package names vary between distributions. Doing
a search for the names above inside the packaging system should lead to
the right path, though.

If some of these tools are missing, you will see errors like this one:

::

    checking for SYSLOG_UNIXAF support... yes
    checking for FSSTND support... yes
    ./configure: line 25895: syntax error near unexpected token `RELP,'
    ./configure: line 25895: ` PKG_CHECK_MODULES(RELP, relp >= 0.1.1)'

The actual error message will vary. In the case shown here, pkg-config
was missing.

**Important:** the build dependencies must be present **before** creating
the build environment is begun. Otherwise, some hard to interpret errors may
occur. For example, the error above will also occur if you install
pkg-config, but *after* you have run *autoreconf*. So be sure everything
is in place *before* you create the build environment.

Creating the Build Environment
------------------------------

This is fairly easy: just issue "**autoreconf -fvi**\ ", which should do
everything you need. Once this is done, you can follow the usual
./configure steps just like when you downloaded an official distribution
tarball (see the `rsyslog install guide <install.html>`_, starting at
step 2, for further details about that).


