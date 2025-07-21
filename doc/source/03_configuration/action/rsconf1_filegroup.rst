$FileGroup
----------

**Type:** global configuration parameter

**Default:**

**Description:**

Set the group for dynaFiles newly created. Please note that this setting
does not affect the group of files already existing. The parameter is a
group name, for which the groupid is obtained by rsyslogd during startup
processing. Interim changes to the user mapping are not detected.

**Sample:**

``$FileGroup loggroup``

