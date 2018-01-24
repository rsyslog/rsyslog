The rsyslog include() object
============================

The include object is use to include configuration snippets
stored elsewhere into the configuration.

Parameters
----------

  - file

    Name of file to be included. May include wildcards, in which case all
    matching files are included (in order of file name sort order).

  - text

    Text to be included. This is most useful when using backtick string
    constants.

  - mode

    Affects how mising files are to be handled:

    - "abort-if-missing", with rsyslog aborting when the file is not present
    - "required" *(default)*, with rsyslog emitting an error message but otherwise
      continuing when the file is not present
    - "optional", which means non-present files will be skipped without notice

Note: one of the "file" or "text parameters must be specified, but not both.

Example
-------

To include a file and generate an error message if not present, do::

    include(file="/path/to/include.conf")

To include a file that need not necessarily be present, do::

    include(file="/path/to/include.conf" mode="optional")

To include multiple files, do::

    include(file="/etc/rsyslog.d/*.conf")

To include an environment variable as configuration, do::

    include(text=`echo $ENV_VAR`)

To include a file specified via an environment variable, do::

    include(file=`echo $ENV_VAR`)

To include an file specified via an environment variable, do::

    include(file=`echo $ENV_VAR` mode="optional")
