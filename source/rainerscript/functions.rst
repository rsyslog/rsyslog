Functions
=========

RainerScript supports a currently quite limited set of functions:

-  getenv(str) - like the OS call, returns the value of the environment
   variable, if it exists. Returns an empty string if it does not exist.
-  strlen(str) - returns the length of the provided string
-  tolower(str) - converts the provided string into lowercase
-  cstr(expr) - converts expr to a string value
-  cnum(expr) - converts expr to a number (integer)
-  re\_match(expr, re) - returns 1, if expr matches re, 0 otherwise
-  re\_extract(expr, re, match, submatch, no-found) - extracts data from
   a string (property) via a regular expression match. POSIX ERE regular
   expressions are used. The variable "match" contains the number of the
   match to use. This permits to pick up more than the first expression
   match. Submatch is the submatch to match (max 50 supported). The
   "no-found" parameter specifies which string is to be returned in case
   when the regular expression is not found. Note that match and
   submatch start with zero. It currently is not possible to extract
   more than one submatch with a single call.
-  field(str, delim, matchnbr) - returns a field-based substring. str is
   the string to search, delim is the delimiter and matchnbr is the
   match to search for (the first match starts at 1). This works similar
   as the field based property-replacer option. Versions prior to 7.3.7
   only support a single character as delimiter character. Starting with
   version 7.3.7, a full string can be used as delimiter. If a single
   character is being used as delimiter, delim is the numerical ascii
   value of the field delimiter character (so that non-printable
   characters can by specified). If a string is used as delmiter, a
   multi-character string (e.g. "#011") is to be specified. Samples:
    set $!usr!field = field($msg, 32, 3); -- the third field, delimited
   by space
    set $!usr!field = field($msg, "#011", 3); -- the third field,
   delmited by "#011"
    Note that when a single character is specified as string
   [field($msg, ",", 3)] a string-based extraction is done, which is
   more performance intense than the equivalent single-character
   [field($msg, 44 ,3)] extraction.
-  prifilt(constant) - mimics a traditional PRI-based filter (like
   "\*.\*" or "mail.info"). The traditional filter string must be given
   as a **constant string**. Dynamic string evaluation is not permitted
   (for performance reasons).


The following example can be used to build a dynamic filter based on
some environment variable:

::

    if $msg contains getenv('TRIGGERVAR') then /path/to/errfile
