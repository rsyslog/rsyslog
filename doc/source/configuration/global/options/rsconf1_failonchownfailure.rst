$FailOnChownFailure
-------------------

**Type:** global configuration parameter

**Default:** on

**Description:**

This option modifies behaviour of dynaFile creation. If different owners
or groups are specified for new files or directories and rsyslogd fails
to set these new owners or groups, it will log an error and NOT write to
the file in question if that option is set to "on". If it is set to
"off", the error will be ignored and processing continues. Keep in mind,
that the files in this case may be (in)accessible by people who should
not have permission. The default is "on".

**Sample:**

``$FailOnChownFailure off``

