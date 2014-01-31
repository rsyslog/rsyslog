RainerScript
============

**RainerScript is a scripting language specifically designed and
well-suited for processing network events and configuring event
processors** (with the most prominent sample being syslog). While
RainerScript is theoritically usable with various softwares, it
currently is being used, and developed for, rsyslog. Please note that
RainerScript may not be abreviated as rscript, because that's somebody
elses trademark.

RainerScript is currently under development. It has its first appearance
in rsyslog 3.12.0, where it provides complex expression support.
However, this is only a very partial implementatio of the scripting
language. Due to technical restrictions, the final implementation will
have a slightly different syntax. So while you are invited to use the
full power of expresssions, you unfortunatley need to be prepared to
change your configuration files at some later points. Maintaining
backwards-compatibility at this point would cause us to make too much
compromise. Defering the release until everything is perfect is also not
a good option. So use your own judgement.

A formal definition of the language can be found in `RainerScript
ABNF <rscript_abnf.html>`_. The rest of this document describes the
language from the user's point of view. Please note that this doc is
also currently under development and can (and will) probably improve as
time progresses. If you have questions, use the rsyslog forum. Feedback
is also always welcome.

Data Types
----------

RainerScript is a typeless language. That doesn't imply you don't need
to care about types. Of course, expressions like "A" + "B" will not
return a valid result, as you can't really add two letters (to
concatenate them, use the concatenation operator &).  However, all type
conversions are automatically done by the script interpreter when there
is need to do so.

Constant Strings
~~~~~~~~~~~~~~~~

String constants are necessary in many places: comparisons,
configuration parameter values and function arguments, to name a few
important ones.

In constant strings, special characters are escape by prepending a
backslash in front of them -- just in the same way this is done in the C
programming language or PHP.

If in doubt how to properly escape, use the `RainerScript String Escape
Online
Tool <http://www.rsyslog.com/rainerscript-constant-string-escaper/>`_.

Expressions
-----------

The language supports arbitrary complex expressions. All usual operators
are supported. The precedence of operations is as follows (with
operations being higher in the list being carried out before those lower
in the list, e.g. multiplications are done before additions.

-  expressions in parenthesis
-  not, unary minus
-  \*, /, % (modulus, as in C)
-  +, -, & (string concatenation)
-  ==, !=, <>, <, >, <=, >=, contains (strings!), startswith (strings!)
-  and
-  or

For example, "not a == b" probably returns not what you intended. The
script processor will first evaluate "not a" and then compare the
resulting boolean to the value of b. What you probably intended to do is
"not (a == b)". And if you just want to test for inequality, we highly
suggest to use "!=" or "<>". Both are exactly the same and are provided
so that you can pick whichever you like best. So inquality of a and b
should be tested as "a <> b". The "not" operator should be reserved to
cases where it actually is needed to form a complex boolean expression.
In those cases, parenthesis are highly recommended.

configuration objects
---------------------

action()
~~~~~~~~

The `action <rsyslog_conf_actions.html>`_ object is the primary means of
describing actions to be carried out.

global()
~~~~~~~~

This is used to set global configuration parameters. For details, please
see the `rsyslog global configuration object <global.html>`_.

Lookup Tables
-------------

`Lookup tables <lookup_tables.html>`_ are a powerful construct to obtain
"class" information based on message content (e.g. to build log file
names for different server types, departments or remote offices).

Functions
---------

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

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2008-2013 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
