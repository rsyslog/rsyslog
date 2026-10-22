.. SPDX-License-Identifier: Apache-2.0

.. _rainerscript-variable-property-types:

.. meta::
   :description: RainerScript message, local, and process-wide global variables.
   :keywords: rsyslog, RainerScript, variables, global variables, JSON

Variable (Property) types
=========================

.. summary-start

RainerScript uses ``$!`` for message JSON properties, ``$.`` for
message-local variables, and ``$/`` for process-wide global variables.

.. summary-end

All rsyslog properties (see the :doc:`properties
<../configuration/properties>` page for a list) can be used in
RainerScript by prefixing them with "$", for example :
::

   set $.x!host = $hostname;

In addition, it also supports local variables. Local
variables are local to the current message, but are NOT message
properties (e.g. the "$!" all JSON property does not contain them).

Message JSON (CEE/Lumberjack) properties, local variables, and global
variables can be modified by the **set**, **unset**, and **reset**
statements. Other message properties cannot be modified by these statements.

Message JSON property names start with "$!" where the bang character
represents the root.

Local variables names start with "$.", where the dot denotes the root.

Global variable names start with ``$/``. They form an in-memory JSON
hierarchy shared by messages and rulesets in the same rsyslogd process.
For example, ``set $/status!last = "seen";`` changes a value that later
messages can read. This state is lost when the daemon process exits; it
is not stored in a work file or restored after a restart. These variables
are distinct from the :doc:`global() configuration object <global>`,
which sets daemon configuration parameters.

An individual global-variable read, write, or delete is protected by a
lock. A read/modify/write expression is not atomic: for example,
``set $/count = $/count + 1;`` reads and writes separately, so concurrent
workers can lose increments. Do not use such a sequence as an exact
cross-worker counter or rate limit without a mechanism that provides the
required synchronization. Current local-queue validation rejects ``$/``
references and updates in local queue graphs.

Message JSON properties, local variables, and global variables may contain
an arbitrarily deep path before the final element. The bang character is
always the path separator, no matter which type of variable it is. For example
"$!path1!path2!varname" is a three-level deep
message property, whereas the similar-looking
"$.path1!path2!varname" specifies a three-level deep local variable. The
bang, dot, or slash character immediately following the dollar sign
distinguishes the types.

Note that the trailing semicolon is needed to indicate the end of expression.
If it is not given, config load will fail with a syntax error message.

Check the following usage examples to understand how these statements behave:

**set**
-------
sets the value of a local-variable or json property, but if the addressed
variable already contains a value its behaviour differs as follows:

**merges** the value if both existing and new value are objects,
but merges the new value to *root* rather than with value of the given key.
If a key exists in both the existing object and the new object, the value from
the new object overwrites the existing one.


**unset**
---------
removes the key. Eg:

::

   set $.x!val = "val_1";
   unset $.x!val;
   # results in $. = { "x": { } }

**reset**
---------
force sets the new value regardless of what the variable
originally contained or if it was even set. Eg.

::

   # to contrast with the set example above, here is how results would look with reset
   set $.x!one = "val_1";
   set $.y!two = "val_2";
   set $.z!var = $.x;
   # results in $. = { "x": { "one": "val_1" }, "y": { "two": "val_2" }, "z": { "var": { "one": "val_1" } } }
   # 'set' or 'reset' can be used interchangeably above(3 lines), they both have the same behaviour, as variable doesn't have an existing value

   reset $.z!var = $.y;
   # results in $. = { "x": { "one": "val_1" }, "y": { "two": "val_2" }, "z": { "var": { "two": "val_2" } } }
   # note how the value of $.z!var was replaced

   reset $.x = "quux";
   # results in $. = { "x": "quux", "y": { "two": "val_2" }, "z": { "var": { "two": "val_2" } } }

