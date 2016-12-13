$IncludeConfig
--------------

**Type:** global configuration directive

**Default:**

**Description:**

This directive allows to include other files into the main configuration
file. As soon as an IncludeConfig directive is found, the contents of
the new file is processed. IncludeConfigs can be nested. Please note
that from a logical point of view the files are merged. Thus, if the
include modifies some parameters (e.g. $DynaFileChacheSize), these new
parameters are in place for the "calling" configuration file when the
include is completed. To avoid any side effects, do a
$ResetConfigVariables after the $IncludeConfig. It may also be a good
idea to do a $ResetConfigVariables right at the start of the include, so
that the module knows exactly what it does. Of course, one might
specifically NOT do this to inherit parameters from the main file. As
always, use it as it best fits...

Note: if multiple files are included, they are processed in ascending
sort order of the file name. We use the "glob()" C library function
for obtaining the sorted list. On most platforms, especially Linux,
this means the the sort order is the same as for bash.

If all regular files in the /etc/rsyslog.d directory are included, then
files starting with "." are ignored - so you can use them to place
comments into the dir (e.g. "/etc/rsyslog.d/.mycomment" will be
ignored). `Michael Biebl had the idea to this
functionality <http://sourceforge.net/tracker/index.php?func=detail&aid=1764088&group_id=123448&atid=696555>`_.
Let me quote him:

    Say you can add an option
     $IncludeConfig /etc/rsyslog.d/
     (which probably would make a good default)
     to /etc/rsyslog.conf, which would then merge and include all
    \*.conf files
     in /etc/rsyslog.d/.
     This way, a distribution can modify its packages easily to drop a
    simple
     config file into this directory upon installation.
     As an example, the network-manager package could install a simple
    config
     file /etc/rsyslog.d/network-manager.conf which would contain.
     :programname, contains, "NetworkManager"
    -/var/log/NetworkManager.log
     Upon uninstallation, the file could be easily removed again. This
    approach
     would be much cleaner and less error prone, than having to munge
    around
     with the /etc/rsyslog.conf file directly.

**Sample:**

``$IncludeConfig /etc/some-included-file.conf``

Directories can also be included. To do so, the name must end on a
slash:

``$IncludeConfig /etc/rsyslog.d/``

**And finally, only specific files matching a wildcard my be included
from a directory:**

``$IncludeConfig /etc/rsyslog.d/*.conf``

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2007 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 2 or higher.
