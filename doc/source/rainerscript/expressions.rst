.. SPDX-License-Identifier: Apache-2.0

Expressions
===========

.. meta::
   :description: RainerScript expression syntax, operators, precedence, and
      string matching.
   :keywords: rsyslog, RainerScript, expressions, operators, fnmatch

.. summary-start

RainerScript expressions combine values and operators, including POSIX-style
whole-string pattern matching with ``fnmatch``.

.. summary-end

The language supports arbitrary complex expressions. All usual operators
are supported. The precedence of operations is as follows (with
operations being higher in the list being carried out before those lower
in the list, e.g. multiplications are done before additions.

-  expressions in parenthesis
-  not, unary minus
-  \*, /, % (modulus, as in C)
-  +, -, & (string concatenation)
-  ==, !=, <>, <, >, <=, >=, contains (strings!), startswith (strings!), endswith (strings!), fnmatch (strings!)
-  and
-  or

For example, "not a == b" probably returns not what you intended. The
script processor will first evaluate "not a" and then compare the
resulting boolean to the value of b. What you probably intended to do is
"not (a == b)". And if you just want to test for inequality, we highly
suggest to use "!=" or "<>". Both are exactly the same and are provided
so that you can pick whichever you like best. So inequality of a and b
should be tested as "a <> b". The "not" operator should be reserved to
cases where it actually is needed to form a complex boolean expression.
In those cases, parentheses are highly recommended.

String pattern matching with ``fnmatch``
----------------------------------------

The case-sensitive ``fnmatch`` operator compares its left-hand string against
the pattern on its right, using POSIX ``fnmatch(pattern, subject, 0)``
semantics. Matching covers the whole string: ``*`` matches zero or more
characters, ``?`` matches one character, bracket expressions such as ``[a-z]``
match character classes, and a backslash escapes the following pattern
character. With flags set to zero, slash and leading-period characters have no
special treatment. Unlike regular expressions, these patterns do not match
only a substring unless the pattern uses ``*`` around that substring.

The right-hand side can be one string or an array of patterns. With an array,
the expression is true when any pattern matches. For example:

.. code-block:: rsyslog

   if $programname fnmatch ["app-*", "worker[0-9]"] then {
       action(type="omfile" file="/var/log/matched.log")
   }
