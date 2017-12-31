$FileOwner
----------

**Type:** global configuration parameter

**Default:**

**Description:**

Set the file owner for dynaFiles newly created. Please note that this
setting does not affect the owner of files already existing. The
parameter is a user name, for which the userid is obtained by rsyslogd
during startup processing. Interim changes to the user mapping are not
detected.

**Sample:**

``$FileOwner loguser``

