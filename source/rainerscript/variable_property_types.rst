Variable (Property) types
=========================

All rsyslog properties (see the :doc:`property
replacer <property_replacer>` page for a list) can be used in
RainerScript. In addition, it also supports local variables. Local
variables are local to the current message, but are NOT message
properties (e.g. the "$!" all JSON property does not contain them).

Only message json (CEE/Lumberjack) properties can be modified by the
"set" and "unset" statements, not any other message property. Obviously,
local variables are also modifieable.

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

