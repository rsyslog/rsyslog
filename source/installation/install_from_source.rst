Installing rsyslog from Source
==============================

*Written by* `Rainer Gerhards <https://rainer.gerhards.net>`_

**In this paper, I describe how to install**
`rsyslog <http://www.rsyslog.com/>`_. It is intentionally a brief
step-by-step guide, targeted to those who want to quickly get it up and
running. For more elaborate information, please consult the rest of the
:doc:`manual set <../index>`.

How to make your life easier...
-------------------------------

In addition to building from source, you can also install |PRODUCT|
using packages. If you use them, you can spare yourself many of the steps
below. This is highly recommended if there is a package for your
distribution available. See :doc:`packages` for instructions.

Steps To Do
-----------

Step 1 - Download Software
~~~~~~~~~~~~~~~~~~~~~~~~~~

For obvious reasons, you need to download rsyslog. Here, I assume that
you use a distribution tarball. If you would like to use a version
directly from the repository, see :doc:`build_from_repo` instead.

Load the most recent build from
`http://www.rsyslog.com/downloads <http://www.rsyslog.com/downloads>`_.
Extract the software with "tar xzf -nameOfDownloadSet-". This will
create a new subdirectory rsyslog-version in the current working
directory. cd into that.

Depending on your system configuration, you also need to install some
build tools, most importantly make, the gcc compiler and the MariaDB/
MySQL development system (if you intend to use MySQL - the package is 
often named "mysql-dev"). On many systems, these things should already 
be present. If you don't know exactly, simply skip this step for now 
and see if nice error messages pop up during the compile process. If 
they do, you can still install the missing build environment tools. So 
this is nothing that you need to look at very carefully.


Build Requirements
~~~~~~~~~~~~~~~~~~

.. include:: /includes/container_dev_env.inc.rst

At a minimum, the following development tools must be present on the
system:

* C compiler (usually gcc)
* make
* libtool
* rst2man (part of Python docutils) if you want to generate the man files
* Bison and Flex (preferably, otherwise yacc and lex)
* zlib development package (usually *libz-dev*)
* json-c (usually named *libjson0-dev* or similar)
* libuuid (usually *uuid-dev*, if not present use --disable-uuid)
* libgcrypt (usually *libgcrypt-dev*)

Also, development versions of the following supporting libraries
that the rsyslog project provides are necessary:

* liblogging (only stdlog component is hard requirement)
* libfastjson
* libestr

In contrast to the other dependencies, recent versions of rsyslog may
require recent versions of these libraries as well, so there is a chance
that they must be built from source, too.

Depending on which plugins are enabled, additional dependencies exist.
These are reported during the ./configure run.

**Important**: you need the **development** version of the packages in
question. That is the version which is used by developers to build software
that uses the respective package. Usually, they are separate from the
regular user package. So if you just install the regular package but not
the development one, ./configure will fail.

As a concrete example, you may want to build ommysql. It obviously requires
a package like *mysql-client*, but that is just the regular package and not
sufficient to build rsyslog successfully. To do so, you need to also install
something named like *mysql-client-dev*.

Usually, the regular package is
automatically installed, when you select the development package, but not
vice versa. The exact behaviour and names depend on the distribution you use.
It is quite common to name development packages something along the line of
*pkgname-dev* or *pkgname-devel* where *pkgname* is the regular package name
(like *mysql-client* in the above example).


Step 2 - Run ./configure
~~~~~~~~~~~~~~~~~~~~~~~~

Run ./configure to adopt rsyslog to your environment. While doing so,
you can also enable options. Configure will display selected options
when it is finished. For example, to enable MariaDB/MySQL support, run::

 ./configure --enable-mysql

Please note that MariaDB/MySQL support by default is NOT disabled.

To learn which ./configure options are available and what their
default values are, use

``./configure --help``


Step 3 - Compile
~~~~~~~~~~~~~~~~

That is easy. Just type "make" and let the compiler work. On any recent
system, that should be a very quick task, on many systems just a matter
of a few seconds. If an error message comes up, most probably a part of
your build environment is not installed. Check with step 1 in those
cases.

Step 4 - Install
~~~~~~~~~~~~~~~~

Again, that is quite easy. All it takes is a "sudo make install". That will
copy the rsyslogd and the man pages to the relevant directories.

Step 5 - Configure rsyslogd
~~~~~~~~~~~~~~~~~~~~~~~~~~~

In this step, you tell rsyslogd what to do with received messages. If
you are upgrading from stock syslogd, /etc/syslog.conf is probably a
good starting point. Rsyslogd understands stock syslogd syntax, so you
can simply copy over /etc/syslog.conf to /etc/rsyslog.conf. Note since
version 3 rsyslog requires to load plug-in modules to perform useful
work.

.. seealso::

   :doc:`/compatibility/v3compatibility`

To load the most common plug-ins, add the following to the top of
rsyslog.conf:

::

  $ModLoad immark # provides --MARK-- message capability
  $ModLoad imudp # provides UDP syslog reception
  $ModLoad imtcp # provides TCP syslog reception
  $ModLoad imuxsock # provides support for local system logging
  $ModLoad imklog # provides kernel logging support

Change rsyslog.conf for any further enhancements you would like to see.
For example, you can add database writing as outlined in the paper
:doc:`/tutorials/database` (remember you need to enable MariaDB/MySQL
support during step 2 if you want to do that!).

Step 6 - Disable stock syslogd
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**You can skip this and the following steps if rsyslog was already
installed as the stock
syslogd on your system (e.g. via a distribution default or package).**
In this case, you are finished.

If another syslogd is installed, it must be disabled and rsyslog set
to become the default. This is because
both it and rsyslogd listen to the same sockets, they can NOT be run
concurrently. So you need to disable the stock syslogd. To do this, you
typically must change your rc.d startup scripts.

For example, under `Debian <http://www.debian.org/>`_ this must be done
as follows: The default runlevel is 2. We modify the init scripts for
runlevel 2 - in practice, you need to do this for all run levels you
will ever use (which probably means all). Under /etc/rc2.d there is a
S10sysklogd script (actually a symlink). Change the name to
\_S10sysklogd (this keeps the symlink in place, but will prevent further
execution - effectively disabling it).

Step 7 - Enable rsyslogd Autostart
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This step is very close to step 3. Now, we want to enable rsyslogd to
start automatically. The rsyslog package contains a (currently small)
number of startup scripts. They are inside the distro-specific directory
(e.g. debian). If there is nothing for your operating system, you can
simply copy the stock syslogd startup script and make the minor
modifications to run rsyslogd (the samples should be of help if you
intend to do this).

In our Debian example, the actual scripts are stored in /etc/init.d.
Copy the standard script to that location. Then, you need to add a
symlink to it in the respective rc.d directory. In our sample, we modify
rc2.d, and can do this via the command "ln -s ../init.d/rsyslogd
S10rsyslogd". Please note that the S10 prefix tells the system to start
rsyslogd at the same time stock sysklogd was started.

**Important:** if you use the database functionality, you should make
sure that MariaDB/MySQL starts before rsyslogd. If it starts later, you will
receive an error message during each restart (this might be acceptable
to you). To do so, either move MariaDB/MySQL's start order before rsyslogd or
rsyslogd's after MariaDB/MySQL.

Step 8 - Check daily cron scripts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Most distributions come pre-configured with some daily scripts for log
rotation. As long as you use the same log file names, the log rotation
scripts will probably work quite well. There is one caveat, though. The
scripts need to tell syslogd that the files have been rotated. To do
this, they typically have a part using syslogd's init script to do that.
Obviously, scripts for other default daemons do not know about rsyslogd, so they
manipulate the other one. If that happens, in most cases an additional
instance of that daemon is started.  It also means that rsyslogd
is not properly told about the log rotation, which will lead it to
continue to write to the now-rotated files.

So you need to fix these scripts. See your distro-specific documentation
how they are located.

Done
~~~~

This concludes the steps necessary to install rsyslog. Of course, it is
always a good idea to test everything thoroughly. At a minimalist level,
you should do a reboot and after that check if everything has come up
correctly. Pay attention not only to running processes, but also check
if the log files (or the database) are correctly being populated.

If rsyslogd encounters any serious errors during startup, you should be
able to see them at least on the system console. They might not be in
log file, as errors might occur before the log file rules are in place.
So it is always a good idea to check system console output when things
don't go smooth. In some rare cases, enabling debug logging (-d option)
in rsyslogd can be helpful. If all fails, go to
`www.rsyslog.com <http://www.rsyslog.com>`_ and check the forum or
mailing list for help with your issue.
