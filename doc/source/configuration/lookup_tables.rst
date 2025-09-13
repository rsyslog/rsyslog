Lookup Tables
=============

Lookup tables are a powerful construct to obtain *class* information based
on message content. It works on top of a data-file which maps key (to be looked
up) to value (the result of lookup).

The idea is to use a message properties (or derivatives of it) as an index
into a table which then returns another value. For example, $fromhost-ip
could be used as an index, with the table value representing the type of
server or the department or remote office it is located in.

This can be emulated using if and else-if stack, but implementing it as a
dedicated component allows ``lookup`` to be made fast.

The lookup table itself exists in a separate data file (one per
table). This file is loaded on Rsyslog startup and when a reload is requested.

There are different types of lookup tables (identified by "type" field in json data-file).
These are ``string``, ``array``, ``sparseArray`` and ``regex``.

Types
^^^^^

string
------

The key to be looked up is an arbitrary string.

**Match criterion**: The key must be exactly equal to index from one of the entries.

array
-----

The value to be looked up is an integer number from a consecutive set.
The set does not need to start at zero or one, but there must be no number missing.
So, for example ``5,6,7,8,9`` would be a valid set of index values, while ``1,2,4,5`` would
not be (due to missing ``3``).

**Match criterion**: Looked-up number(key) must be exactly equal to index from one of the entries.

sparseArray
-----------

The value to be looked up is an integer value, but there may be gaps inside the
set of values (usually there are large gaps). A typical use case would be the
matching of IPv4 address information.

**Match criterion**: A match happens on the first index that is less than or equal to the looked-up key.

Note that index integer numbers are represented by unsigned 32 bits.

regex
-----

The key is treated as a string and compared against a list of regular
expression patterns. Each entry in the table contains a ``regex`` field for
the pattern and a ``tag`` field for the value to return. Rsyslog uses the
POSIX extended regular expression engine (``<regex.h>``); PCRE-style features
are not supported. This type requires rsyslog to be compiled with regular
expression support.

**Match criterion**: Patterns are evaluated sequentially and the **first**
regex that matches the key determines the returned tag. If no regex matches,
the ``nomatch`` string is used. Because evaluation is sequential and uses the
regular expression engine, this type is slower than other table types.
Overlapping regexes in the same table can lead to unexpected results; order
the entries carefully and avoid ambiguous patterns.


Lookup Table File Format
^^^^^^^^^^^^^^^^^^^^^^^^

Lookup table files contain a single JSON object. This object consists of a header and a table part.

**Header**

The header is the top-level json.
It has parameters "version", "nomatch", and "type".

Parameters:
    **version** <number, default: 1> : Version of table-definition format (so improvements in the future can be managed in a backward compatible way).

    **nomatch** <string literal, default: ""> : Value to be returned for a lookup when match fails.

    **type** <*string*, *array* or *sparseArray*, default: *string*> : Type of lookup-table (controls how matches are performed).

**Table**

This must be an array of elements, even if only a single value exists (for obvious reasons,
we do not expect this to occur often). Each array element must contain two fields "index"
and "value". When ``type`` is ``regex`` these fields are instead named ``regex``
and ``tag``.

For ``regex`` tables the list is scanned from top to bottom. The first pattern
that matches the looked-up key stops the scan and returns the associated tag.
If none of the regexes match, the ``nomatch`` string is returned.

This is a sample of how an ip-to-office mapping may look like:

::

    { "version" : 1,
      "nomatch" : "unk",
      "type" : "string",
      "table" : [
        {"index" : "10.0.1.1", "value" : "A" },
        {"index" : "10.0.1.2", "value" : "A" },
        {"index" : "10.0.1.3", "value" : "A" },
        {"index" : "10.0.2.1", "value" : "B" },
        {"index" : "10.0.2.2", "value" : "B" },
        {"index" : "10.0.2.3", "value" : "B" }]}


Note: In the example above, if a different IP comes in, the value "unk" is returned thanks to the nomatch parameter in the first line.

This is how a simple regex table looks. Each entry contains a ``regex`` and a
``tag`` field. The ``tag`` of the **first** matching entry is returned:

::

    { "version": 1,
      "nomatch": "unknown",
      "type": "regex",
      "table": [
        {"regex": "^10\\.0\\.1\\.", "tag": "netA"},
        {"regex": "^10\\.0\\.",   "tag": "netB"}]}

For an input of ``10.0.1.25`` the tag ``netA`` is returned, while ``10.0.2.5``
returns ``netB``. If the second entry were placed before the first, both
addresses would return ``netB`` due to the overlap of the patterns.

Lookup tables can be accessed via the ``lookup()`` built-in function. A common usage pattern is to set a local variable to the lookup result and later use that variable in templates.



Lookup-table configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^

Lookup-table configuration involves a **two part setup** (definition and usage(lookup)), with an optional third part,
which allows reloading table using internal trigger.

lookup_table(name="<table>" file="</path/to/file>"...) (object)
---------------------------------------------------------------

**Defines** the table(identified by the table-name) and allows user to set some properties that control behavior of the table.

::

   lookup_table(name="msg_per_host")

Parameters:
    **name** <string literal, mandatory> : Name of the table.

    **file** <string literal, file path, mandatory> : Path to external json database file.

    **reloadOnHUP** <on|off, default: on> : Whether or not table should be reloaded when process receives HUP signal.

A definition setting all the parameters looks like:

::

   lookup_table(name="host_bu" file="/var/lib/host_billing_unit_mapping.json" reloadOnHUP="on")


lookup("<table>", <expr>) (function)
------------------------------------

**Looks up** and returns the value that is associated with the given key (passed as <variable>)
in lookup table identified by table-name. If no match is found (according to table-type
matching-criteria specified above), the "nomatch" string is returned (or an empty string if it is not defined).

Parameters:
    **name** <string literal, mandatory> : Name of the table.

    **expr** <expression resulting in string or number according to lookup-table type, mandatory> : Key to be looked up.

A ``lookup`` call looks like:

::

   set $.business_unit = lookup("host_business_unit", $hostname);

   if ($.business_unit == "unknown") then {
       ....
   }

Some examples of different match/no-match scenarios:

**string table**:

::

    { "nomatch" : "none",
      "type" : "string",
      "table":[
        {"index" : "foo", "value" : "bar" },
        {"index" : "baz", "value" : "quux" }]}

Match/no-Match behaviour:

======  ==============
key     return
======  ==============
foo     bar
baz     quux
corge   none
======  ==============

**array table**:

::

    { "nomatch" : "nothing",
      "type" : "array",
      "table":[
        {"index" : 9, "value" : "foo" },
        {"index" : 10, "value" : "bar" },
        {"index" : 11, "value" : "baz" }]}

Match/no-Match behaviour:

======  ==============
key     return
======  ==============
9       foo
11      baz
15      nothing
0       nothing
======  ==============

**sparseArray table**:

::

    { "nomatch" : "no_num",
      "type" : "sparseArray",
      "table":[
        {"index" : "9", "value" : "foo" },
        {"index" : "11", "value" : "baz" }]}

Match/no-Match behaviour:

======  ==============
key     return
======  ==============
8       no_num
9       foo
10      foo
11      baz
12      baz
100     baz
======  ==============

**regex table**:

::

    { "nomatch" : "no_match",
      "type" : "regex",
      "table":[
        {"regex" : "^error",       "tag" : "err"},
        {"regex" : "^error.*crit", "tag" : "crit"}]}

Match behaviour depends on table order. The first matching regex wins:

=============  =========
key            return
=============  =========
error1         err
errorcritical  err
warning        no_match
=============  =========

Reversing the entries would return ``crit`` for ``errorcritical``.


reload_lookup_table("<table>", "<stub value>") (statement)
----------------------------------------------------------

**Reloads** lookup table identified by given table name **asynchronously** (by internal trigger, as opposed to HUP).

This statement isn't always useful. It needs to be used only when lookup-table-reload needs to be triggered in response to
a message.

Messages will continue to be processed while table is asynchronously reloaded.

Note: For performance reasons, message that triggers reload should be accepted only from a trusted source.

Parameters:
    **name** <string literal, mandatory> : Name of the table.

    **stub value** <string literal, optional> : Value to stub the table in-case reload-attempt fails.

A ``reload_lookup_table`` invocation looks like:

::

   if ($.do_reload == "y") then {
       reload_lookup_table("host_bu", "unknown")
   }


Implementation Details
^^^^^^^^^^^^^^^^^^^^^^

The lookup table functionality is implemented via efficient algorithms.

The string and sparseArray lookup have O(log(n)) time complexity, while array lookup is O(1).
Regex tables are scanned sequentially and thus operate in O(n) time on top of
the cost of each regular expression evaluation.

To preserve space and, more important, increase cache hit performance, equal data values are only stored once,
no matter how often a lookup index points to them.
