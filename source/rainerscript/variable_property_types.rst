Variable (Property) types
=========================

All rsyslog properties (see the :doc:`properties
<../configuration/properties>` page for a list) can be used in
RainerScript by prefixing them with "$", for example :
::

   set $.x!host = $hostname;

In addition, it also supports local variables. Local
variables are local to the current message, but are NOT message
properties (e.g. the "$!" all JSON property does not contain them).

Only message json (CEE/Lumberjack) properties can be modified by the
**set**, **unset** and **reset** statements, not any other message property. Obviously,
local variables are also modifiable.

Message JSON property names start with "$!" where the bang character
represents the root.

Local variables names start with "$.", where the dot denotes the root.

Both JSON properties as well as local variables may contain an arbitrary
deep path before the final element. The bang character is always used as
path separator, no matter if it is a message property or a local
variable. For example "$!path1!path2!varname" is a three-level deep
message property where as the very similar looking
"$.path1!path2!varname" specifies a three-level deep local variable. The
bang or dot character immediately following the dollar sign is used by
rsyslog to separate the different types.

Note that the trailing semicolon is needed to indicate the end of expression.
If it is not given, config load will fail with a syntax error message.

Check the following usage examples to understand how these statements behave:

**set**
-------
sets the value of a local-variable or json property, but if the addressed
variable already contains a value its behaviour differs as follows:

**merges** the value if both existing and new value are objects, 
but merges the new value to *root* rather than with value of the given key. Eg. 

::

   set $.x!one = "val_1";
   # results in $. = { "x": { "one": "val_1" } }
   set $.y!two = "val_2";
   # results in $. = { "x": { "one": "val_1" }, "y": { "two": "val_2" } }

   set $.z!var = $.x;
   # results in $. = { "x": { "one": "val_1" }, "y": { "two": "val_2" }, "z": { "var": { "one": "val_1" } } }

   set $.z!var = $.y;
   # results in $. = { "x": { "one": "val_1" }, "y": { "two": "val_2" }, "z": { "var": { "one": "val_1" } }, "two": "val_2" }
   # note that the key *two* is at root level and not  under *$.z!var*.

**ignores** the new value if old value was an object, but new value is a not an object (Eg. string, number etc). Eg:

::

   set $.x!one = "val_1";
   set $.x = "quux";
   # results in $. = { "x": { "one": "val_1" } }
   # note that "quux" was ignored

**resets** variable, if old value was not an object.

::

   set $.x!val = "val_1";
   set $.x!val = "quux";
   # results in $. = { "x": { "val": "quux" } }

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

