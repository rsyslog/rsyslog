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

Functions
---------

RainerScript supports a currently quite limited set of functions:

-  getenv(str) - like the OS call, returns the value of the environment
   variable, if it exists. Returns an empty string if it does not exist.
-  strlen(str) - returns the length of the provided string
-  tolower(str) - converts the provided string into lowercase

The following example can be used to build a dynamic filter based on
some environment variable:

::

    if $msg contains getenv('TRIGGERVAR') then /path/to/errfile

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2008, 2009 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
