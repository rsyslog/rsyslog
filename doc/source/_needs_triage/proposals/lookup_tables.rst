Lookup Tables
=============

**NOTE: this is proposed functionality, which is NOT YET IMPLEMENTED!**

**Lookup tables are a powerful construct to obtain "class" information
based on message content (e.g. to build log file names for different
server types, departments or remote offices).**

The base idea is to use a message variable as an index into a table
which then returns another value. For example, $fromhost-ip could be
used as an index, with the table value representing the type of server
or the department or remote office it is located in. A main point with
lookup tables is that the lookup is very fast. So while lookup tables
can be emulated with if-elseif constructs, they are generally much
faster. Also, it is possible to reload lookup tables during rsyslog
runtime without the need for a full restart.

The lookup tables itself exists in a separate configuration file (one
per table). This file is loaded on rsyslog startup and when a reload is
requested.

There are different types of lookup tables:

-  **string** - the value to be looked up is an arbitrary string. Only
   exact some strings match.
-  **array** - the value to be looked up is an integer number from a
   consecutive set. The set does not need to start at zero or one, but
   there must be no number missing. So, for example 5,6,7,8,9 would be a
   valid set of index values, while 1,2,4,5 would not be (due to missing
   2). A match happens if the requested number is present.
-  **sparseArray** - the value to be looked up is an integer value, but
   there may be gaps inside the set of values (usually there are large
   gaps). A typical use case would be the matching of IPv4 address
   information. A match happens on the first value that is less than or
   equal to the requested value.

Note that index integer numbers are represented by unsigned 32 bits.

Lookup tables can be access via the lookup() built-in function. The core
idea is to set a local variable to the lookup result and later on use
that local variable in templates.

More details on usage now follow.

Lookup Table File Format
------------------------

Lookup table files contain a single JSON object. This object contains of
a header and a table part.

Header
~~~~~~

The header is the top-level json. It has parameters "version", "nomatch",
and "type". The version parameter must be given and must always be one
for this version of rsyslog. The nomatch parameter is optional. If
specified, it contains the value to be used if lookup() is provided an
index value for which no entry exists. The default for "nomatch" is the
empty string. Type specifies the type of lookup to be done.

Table
~~~~~

This must be an array of elements, even if only a single value exists
(for obvious reasons, we do not expect this to occur often). Each array
element must contain two fields "index" and "value".

Example
~~~~~~~

This is a sample of how an ip-to-office mapping may look like:

::

    { "version":1, "nomatch":"unk", "type":"string",
      "table":[ {"index":"10.0.1.1", "value":"A" },
              {"index":"10.0.1.2", "value":"A" },
              {"index":"10.0.1.3", "value":"A" },
              {"index":"10.0.2.1", "value":"B" },
              {"index":"10.0.2.2", "value":"B" },
              {"index":"10.0.2.3", "value":"B" }
            ]
    }

Note: if a different IP comes in, the value "unk" is returned thanks to
the nomatch parameter in the first line.

RainerScript Statements
-----------------------

lookup\_table() Object
~~~~~~~~~~~~~~~~~~~~~~

This statement defines and initially loads a lookup table. Its format is
as follows:

::

    lookup_table(name="name" file="/path/to/file" reloadOnHUP="on|off")

Parameters
^^^^^^^^^^

-  **name** (mandatory)

   Defines the name of lookup table for further reference inside the
   configuration. Names must be unique. Note that it is possible, though
   not advisable, to have different names for the same file.
-  **file** (mandatory)

   Specifies the full path for the lookup table file. This file must be
   readable for the user rsyslog is run under (important when dropping
   privileges). It must point to a valid lookup table file as described
   above.
-  **reloadOnHUP** (optional, default "on")

   Specifies if the table shall automatically be reloaded as part of
   HUP processing. For static tables, the default is "off" and
   specifying "on" triggers an error message. Note that the default of
   "on" may be somewhat suboptimal performance-wise, but probably is
   what the user intuitively expects. Turn it off if you know that you
   do not need the automatic reload capability.

lookup() Function
~~~~~~~~~~~~~~~~~

This function is used to actually do the table lookup. Format:

::

    lookup("name", indexvalue)

Parameters
^^^^^^^^^^

-  **return value**

   The function returns the string that is associated with the given
   indexvalue. If the indexvalue is not present inside the lookup table,
   the "nomatch" string is returned (or an empty string if it is not
   defined).
-  **name** (constant string)

   The lookup table to be used. Note that this must be specified as a
   constant. In theory, variable table names could be made possible, but
   their runtime behaviour is not as good as for static names, and we do
   not (yet) see good use cases where dynamic table names could be
   useful.
-  **indexvalue** (expression)

   The value to be looked up. While this is an arbitrary RainerScript
   expression, it's final value is always converted to a string in order
   to conduct the lookup. For example, "lookup(table, 3+4)" would be
   exactly the same as "lookup(table, "7")". In most cases, indexvalue
   will probably be a single variable, but it could also be the result
   of all RainerScript-supported expression types (like string
   concatenation or substring extraction). Valid samples are
   "lookup(name, $fromhost-ip & $hostname)" or "lookup(name,
   substr($fromhost-ip, 0, 5))" as well as of course the usual
   "lookup(table, $fromhost-ip)".

load\_lookup\_table Statement
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Note: in the final implementation, this MAY be implemented as an
action. This is a low-level decision that must be made during the detail
development process. Parameters and semantics will remain the same of
this happens.**

This statement is used to reload a lookup table. It will fail if the
table is static. While this statement is executed, lookups to this table
are temporarily blocked. So for large tables, there may be a slight
performance hit during the load phase. It is assume that always a
triggering condition is used to load the table.

::

    load_lookup_table(name="name" errOnFail="on|off" valueOnFail="value")

Parameters
^^^^^^^^^^

-  **name** (string)

   The lookup table to be used.
-  **errOnFail** (boolean, default "on")

   Specifies whether or not an error message is to be emitted if there
   are any problems reloading the lookup table.
-  **valueOnFail** (optional, string)

   This parameter affects processing if the lookup table cannot be
   loaded for some reason: If the parameter is not present, the previous
   table will be kept in use. If the parameter is given, the previous
   table will no longer be used, and instead an empty table be with
   nomath=valueOnFail be generated. In short, that means when the
   parameter is set and the reload fails, all matches will always return
   what is specified in valueOnFail.

Usage example
~~~~~~~~~~~~~

For clarity, we show only those parts of rsyslog.conf that affect lookup
tables. We use the remote office example that an example lookup table
file is given above for.

::

    lookup_table(name="ip2office" file="/path/to/ipoffice.lu"
                 reloadOnHUP="off")


    template(name="depfile" type="string"
             string="/var/log/%$usr.dep%/messages")

    set $usr.dep = lookup("ip2office", $fromhost-ip);
    action(type="omfile" dynfile="depfile")

    # support for reload "commands"
    if $fromhost-ip == "10.0.1.123"
       and $msg contains "reload office lookup table"
       then
       load_lookup_table(name="ip2office" errOnFail="on")

Note: for performance reasons, it makes sense to put the reload command
into a dedicated ruleset, bound to a specific listener - which than
should also be sufficiently secured, e.g. via TLS mutual auth.

Implementation Details
----------------------

The lookup table functionality is implemented via highly efficient
algorithms. The string lookup has O(log n) time complexity. The array
lookup is O(1). In case of sparseArray, we have O(log n).

To preserve space and, more important, increase cache hit performance,
equal data values are only stored once, no matter how often a lookup
index points to them.

