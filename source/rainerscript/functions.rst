Functions
=========

There are two types of RainerScript functions: built-ins and modules. Built-in
functions can always be called in the confiuration. To use module functions,
you have to load the corresponding module first. To do this, add the following
line to your configuration:

::

	module(load="<name of module>")

If more than one function with the same name is present, the function of the
first module loaded will be used. Also, an error message stating this will be
generated. However, the configuration will not abort. Please note that built-in
functions will always be automatically loaded before any modules. because of this, you
are unable to override any of the built-in functions, since their names are already
in use. The name of a function module starts with fm.


built-in functions
==================

You do not have to load any module to use these functions.

getenv(str)
-----------

   like the OS call, returns the value of the environment
   variable, if it exists. Returns an empty string if it does not exist.

   The following example can be used to build a dynamic filter based on
   some environment variable:

::

    if $msg contains getenv('TRIGGERVAR') then /path/to/errfile


strlen(str)
-----------

   returns the length of the provided string

tolower(str)
------------

   converts the provided string into lowercase

cstr(expr)
----------

   converts expr to a string value

cnum(expr)
----------

   converts expr to a number (integer)
   Note: if the expression does not contain a numerical value,
   behaviour is undefined.

wrap(str, wrapper_str)
----------------------

   returns the str wrapped with wrapper_str. Eg.

::

   wrap("foo bar", "##")

produces

::

   "##foo bar##"

wrap(str, wrapper_str, escaper_str)
-----------------------------------

   returns the str wrapped with wrapper_str.
   But additionally, any instances of wrapper_str appearing in str would be replaced
   by the escaper_str. Eg.

::

   wrap("foo'bar", "'", "_")

produces

::

   "'foo_bar'"

replace(str, substr_to_replace, replace_with)
---------------------------------------------

   returns new string with all instances of substr_to_replace replaced
   by replace_with. Eg.

::

   replace("foo bar baz", " b", ", B")

produces

::

   "foo, Bar, Baz".

re\_match(expr, re)
-------------------

    returns 1, if expr matches re, 0 otherwise. Uses POSIX ERE.

re\_extract(expr, re, match, submatch, no-found)
------------------------------------------------

   extracts data from a string (property) via a regular expression match.
   POSIX ERE regular expressions are used. The variable "match" contains
   the number of the match to use. This permits to pick up more than the
   first expression match. Submatch is the submatch to match (max 50 supported).
   The "no-found" parameter specifies which string is to be returned in case
   when the regular expression is not found. Note that match and
   submatch start with zero. It currently is not possible to extract
   more than one submatch with a single call.

field(str, delim, matchnbr)
---------------------------

   returns a field-based substring. str is
   the string to search, delim is the delimiter and matchnbr is the
   match to search for (the first match starts at 1). This works similar
   as the field based property-replacer option. Versions prior to 7.3.7
   only support a single character as delimiter character. Starting with
   version 7.3.7, a full string can be used as delimiter. If a single
   character is being used as delimiter, delim is the numerical ascii
   value of the field delimiter character (so that non-printable
   characters can by specified). If a string is used as delimiter, a
   multi-character string (e.g. "#011") is to be specified.

   Note that when a single character is specified as string
   ``field($msg, ",", 3)`` a string-based extraction is done, which is
   more performance intensive than the equivalent single-character
   ``field($msg, 44 ,3)`` extraction. Eg.

::

   set $!usr!field = field($msg, 32, 3);  -- the third field, delimited by space

   set $!usr!field = field($msg, "#011", 2); -- the second field, delimited by "#011"

exec\_template
--------------

   Sets a variable through the execution of a template. Basically this permits to easily
   extract some part of a property and use it later as any other variable.

::

   template(name="extract" type="string" string="%msg:F:5%")
   set $!xyz = exec_template("extract");

the variable xyz can now be used to apply some filtering :

::

   if $!xyz contains 'abc' then {action()}

or to build dynamically a file path :

::

   template(name="DynaFile" type="string" string="/var/log/%$!xyz%-data/%timereported%-%$!xyz%.log")

**Read more about it here :** `<http://www.rsyslog.com/how-to-use-set-variable-and-exec_template>`_

prifilt(constant)
-----------------

   mimics a traditional PRI-based filter (like
   "\*.\*" or "mail.info"). The traditional filter string must be given
   as a **constant string**. Dynamic string evaluation is not permitted
   (for performance reasons).

dyn_inc(bucket_name_literal_string, str)
-----------------------------------------

   Increments counter identified by ``str`` in dyn-stats bucket identified
   by ``bucket_name_literal_string``. Returns 0 when increment is successful,
   any other return value indicates increment failed.

   Counters updated here are reported by **impstats**.

   Except for special circumstances (such as memory allocation failing etc),
   increment may fail due to metric-name cardinality being under-estimated.
   Bucket is configured to support a maximum cardinality (to prevent abuse)
   and it rejects increment-operation if it encounters a new(previously unseen)
   metric-name(``str``) when full.

   **Read more about it here** :doc:`Dynamic Stats<../configuration/dyn_stats>`

lookup(table_name_literal_string, key)
---------------------------------------

   Lookup tables are a powerful construct to obtain *class* information based
   on message content. It works on top of a data-file which maps key (to be looked
   up) to value (the result of lookup).

   The idea is to use a message properties (or derivatives of it) as an index
   into a table which then returns another value. For example, $fromhost-ip
   could be used as an index, with the table value representing the type of
   server or the department or remote office it is located in.

   **Read more about it here** :doc:`Lookup Tables<../configuration/lookup_tables>`

num2ipv4
--------

   Converts an integer into an IPv4-address and returns the address as string.
   Input is an integer with a value between 0 and 4294967295. The output format
   is '>decimal<.>decimal<.>decimal<.>decimal<' and '-1' if the integer input is invalid
   or if the function encounters a problem.

ipv42num
--------

   Converts an IPv4-address into an integer and returns the integer. Input is a string;
   the expected address format may include spaces in the beginning and end, but must not
   contain any other characters in between (except dots). If the format does include these, the
   function results in an error and returns -1.

random(max)
-----------

   Generates a random number between 0 and the number specified, though
   the maximum value supported is platform specific.

   - If a number is not specified then 0 is returned.
   - If 0 is provided as the maximum value, then 0 is returned.
   - If the specified value is greater than the maximum supported
     for the current platform, then rsyslog will log this in
     the debug output and use the maximum value supported instead.

   While the original intent of this function was for load balancing, it
   is generic enough to be used for other purposes as well.

.. warning::
   The random number must not be assumed to be crypto-grade.

.. versionadded:: 8.12.0


ltrim
-----

   Removes any spaces at the start of a given string. Input is a string, output
   is the same string starting with the first non-space character.

rtrim
-----

   Removes any spaces at the end of a given string. Input is a string, output
   is the same string ending with the last non-space character.

substring(str, start, subStringLength)
--------------------------------------

   Creates a substring from str. The substring begins at start and is
   at most subStringLength characters long.

int2hex(num)
------------

   returns a hexadecimal number string of a given positive integer num.

script_error
------------

  Returns the error state of functions that support it. C-Developers note that this
  is similar to ``errno`` under Linux. The error state corresponds to the function
  immediatly called before. The next function call overrides it.

  Right now, the value 0 means that that the previous functions succeeded, any other
  value that it failed. In the future, we may have more fine-grain error codes.

  Function descriptions mention if a function supports error state information. If not,
  the function call will always set ``script_error()`` to 0.

previous_action_suspended
-------------------------
  This boolenan function returns 1 (true) if the previous action is suspended,
  0 (false) otherwise. It can be used to initiate action that shall happen if
  a function failed. Please note that an action failure may not be immediately
  detected, so the function return value is a bit fuzzy. It is guaranteed, however
  that a suspension will be detected with the next batch of messages that is
  being processed.

Use
...

  If, for example, you want to execute a rule set in case of failure of an
  action, do this::

   ruleset(name="output_writer") {
       action(type="omfile" file="rsyslog.log")
   }

   action(type="omfwd" protocol="tcp" target="10.1.1.1")
   if previous_action_suspended() then
          call output_writer

format_time(unix_timestamp, format_str)
---------------------------------------
   **NOTE: this is EXPERIMENTAL code** - it may be removed or altered in
   later versions than 8.30.0. Please watch the ChangeLog closely for
   updates.

   Converts a UNIX timestamp to a formatted RFC 3164 or RFC 3339 date/time string.
   The first parameter is expected to be an integer value representing the number of
   seconds since 1970-01-01T00:00:0Z (UNIX epoch). The second parameter can be one of
   ``"date-rfc3164"`` or ``"date-rfc3339"``. The output is a string containing
   the formatted date/time. Date/time strings are expressed in **UTC** (no time zone
   conversion is provided).

   * **Note**: If the input to the function is NOT a proper UNIX timestamp, a string
     containing the *original value of the parameter* will be returned instead of a
     formatted date/time string.

::

   format_time(1507165811, "date-rfc3164")

produces

::

   Oct  5 01:10:11

and

::

   format_time(1507165811, "date-rfc3339")

produces

::

   2017-10-05T01:10:11Z

In the case of an invalid UNIX timestamp:

::

   format_time("foo", "date-rfc3339")

it produces the original value:

::

   foo

parse_json(string, container)
-----------------------------

   Parses the json string ``string`` and places the resulting json object
   into ``container`` where container can be any valid rsyslog variable.
   Returns 0 on success and something otherwise if ``string`` does **not**
   contain valid json.


parse_time(timestamp)
---------------------------------------

   Converts an RFC 3164 or RFC 3339 formatted date/time string to a UNIX timestamp
   (an integer value representing the number of seconds since the UNIX epoch:
   1970-01-01T00:00:0Z).

   If the input to the function is not a properly formatted RFC 3164 or RFC 3339
   date/time string, or cannot be parsed, ``0`` is returned and ``script_error()``
   will be set to error state.

   * **Note**: This function does not support unusual RFC 3164 dates/times that
     contain year or time zone information.

   * **Note**: Fractional seconds (if present) in RFC 3339 date/time strings will
     be discarded.


::

   parse_time("Oct  5 01:10:11") # Assumes the current year (2017, in this example)

produces

::

   1507165811

and

::

   parse_time("2017-10-05T01:10:11+04:00")

produces

::

   1507151411

is_time(timestamp, format_str)
---------------------------------------

   Checks the given timestamp to see if it is a valid date/time string (RFC 3164,
   or RFC 3339), or a UNIX timestamp.

   This function returns ``1`` for valid date/time strings and UNIX timestamps,
   ``0`` otherwise. Additionally, if the input cannot be parsed, or there is
   an error, ``script_error()`` will be set to error state.

   The ``format_str`` parameter is optional, and can be one of ``"date-rfc3164"``,
   ``"date-rfc3339"`` or ``"date-unix"``. If this parameter is specified, the
   function will only succeed if the input matches that format. If omitted, the
   function will compare the input to all of the known formats (indicated above)
   to see if one of them matches.

   * **Note**: This function does not support unusual RFC 3164 dates/times that
     contain year or time zone information.

::

   is_time("Oct  5 01:10:11")
   is_time("2017-10-05T01:10:11+04:00")
   is_time(1507165811)

all produce

::

   1

and

::

   is_time("2017-10-05T01:10:11+04:00", "date-rfc3339")

produces

::

   1

and

::

   is_time("2017-10-05T01:10:11+04:00", "date-unix")

produces

::

   0


module functions
================

You can make module funtions accessible for the configuration by loading the corresponding
module. Once they are loaded, you can use them like any other rainerscript function. If
more than one function are part of the same module, all functions will be available once
the module is loaded.
Here is an example for how to use module functions (in this case http_request)

::

  module(load="../plugins/imtcp/.libs/imtcp")
  module(load="../plugins/fmhttp/.libs/fmhttp")
  input(type="imtcp" port="13514")

  template(name="outfmt" type="string" string="%$!%\n")

  if $msg contains "msgnum:" then {
  	set $.url = "http://www.rsyslog.com/testbench/echo-get.php?content=" & ltrim($msg);
  	set $!reply = http_request($.url);
  	action(type="omfile" file="rsyslog.out.log" template="outfmt")
  }


http_request(target)
--------------------

module: fmhttp

performs a http request to target and returns the result of said request.

Please note that this function is very slow and therefore we suggest using it only seldomly
to insure adequate performance.
