$DirGroup
---------

**Type:** global configuration parameter

**Default:**

**Description:**

Set the group for directories newly created. Please note that this
setting does not affect the group of directories already existing. The
parameter is a group name, for which the groupid is obtained by rsyslogd
on during startup processing. Interim changes to the user mapping are
not detected.

**Sample:**

``$DirGroup loggroup``

