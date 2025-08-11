$ModDir
-------

**Type:** global configuration parameter

**Default:** system default for user libraries, e.g.
/usr/local/lib/rsyslog/

**Description:**

Provides the default directory in which loadable modules reside. This
may be used to specify an alternate location that is not based on the
system default. If the system default is used, there is no need to
specify this parameter. Please note that it is vitally important to end
the path name with a slash, else module loads will fail.

**Sample:**

``$ModDir /usr/rsyslog/libs/  # note the trailing slash!``

