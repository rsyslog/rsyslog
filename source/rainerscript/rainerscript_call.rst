The rsyslog "call" statement
============================

The rsyslog "call" statement is used to tie rulesets together. It is
modelled after the usual programming language "call" statement. Think of
a ruleset as a subroutine (what it really is!) and you get the picture.

The "call" statement can be used to call into any type of rulesets. If a
rule set has a queue assigned, the message will be posted to that queue
and processed asynchronously. Otherwise, the ruleset will be executed
synchronously and control returns to right after the call when the rule
set has finished execution.

Note that there is an important difference between asynchronous and
synchronous execution in regard to the "stop" statement. It will not
affect processing of the original message when run asynchronously.

The "call" statement replaces the deprecated omruleset module. It offers
all capabilities omruleset has, but works in a much more efficient way.
Note that omruleset was a hack that made calling rulesets possible
within the constraints of the pre-v7 engine. "call" is the clean
solution for the new engine. Especially for rulesets without associated
queues (synchronous operation), it has zero overhead (really!).
omruleset always needs to duplicate messages, which usually means at
least ~250 bytes of memory writes, some allocs and frees - and even more
performance-intense operations.

syntax
------

``call rulesetname``

Where "rulesetname" is the name of a ruleset that is defined elsewhere
inside the configration. If the call is synchronous or asynchronous
depends on the ruleset parameters. This cannot be overriden by the
"call" statement.

related links
-------------

-  `Blog posting announcing "call" statement (with
   sample) <https://rainer.gerhards.net/2012/10/how-to-use-rsyslogs-ruleset-and-call.html>`_

