Functions
=========

RainerScript supports a currently quite limited set of functions:


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
   characters can by specified). If a string is used as delmiter, a
   multi-character string (e.g. "#011") is to be specified.

   Note that when a single character is specified as string
   ``field($msg, ",", 3)`` a string-based extraction is done, which is
   more performance intense than the equivalent single-character
   ``field($msg, 44 ,3)`` extraction. Eg.

::
   
   set $!usr!field = field($msg, 32, 3);  -- the third field, delimited by space
   
   set $!usr!field = field($msg, "#011", 2); -- the second field, delmited by "#011"


prifilt(constant)
-----------------

   mimics a traditional PRI-based filter (like
   "\*.\*" or "mail.info"). The traditional filter string must be given
   as a **constant string**. Dynamic string evaluation is not permitted
   (for performance reasons).

dyn_inc(bucket_name_litteral_string, str)
-----------------------------------------

   Increments counter identified by ``str`` in dyn-stats bucket identified
   by ``bucket_name_litteral_string``. Returns 0 when increment is successful,
   any other return value indicates increment failed.

   Counters updated here are reported by **impstats**.

   Except for special circumstances (such as memory allocation failing etc),
   increment may fail due to metric-name cardinality being under-estimated.
   Bucket is configured to support a maximum cardinality (to prevent abuse)
   and it rejects increment-operation if it encounters a new(previously unseen)
   metric-name(``str``) when full.

   **Read more about it here** :doc:`Dynamic Stats<../configuration/dyn_stats>`
   
lookup(table_name_litteral_string, key)
---------------------------------------

   Lookup tables are a powerful construct to obtain *class* information based
   on message content. It works on top of a data-file which maps key (to be looked
   up) to value (the result of lookup).

   The idea is to use a message properties (or derivatives of it) as an index
   into a table which then returns another value. For example, $fromhost-ip
   could be used as an index, with the table value representing the type of
   server or the department or remote office it is located in.

   **Read more about it here** :doc:`Lookup Tables<../configuration/lookup_tables>`
   
