The rsyslog "call_indirect" statement
=====================================

The rsyslog "call_indirect" statement is equivalent to
:doc:`"call" statement <rainerscript_call>`
except that the name of the to be called ruleset is not constant but an
expression and so can be computed at runtime.

If the ruleset name cannot be found when call_indirect is used, an error
message as emitted and the call_indirect statement is ignored. Execution
continues with the next statement.

syntax
------

``call_indirect expression;``

Where "expression" is any valid expression. See 
:doc:`expressions <expressions>`
for more information. Note that the trailing semicolon is needed to
indicate the end of expression. If it is not given, config load will
fail with a syntax error message.

examples
--------

The potentially most useful use-case for "call_indirect" is calling a
ruleset based on a message variable. Let us assume that you have named
your rulesets according to syslog tags expected. Then you can use

``call_indirect $syslogtag;``

To call these rulesets. Note, however, that this may be misused by a
malicious attacker, who injects invalid syslog tags. This could especially
be used to redirect message flow to known standard rulesets. To somewhat
mitigate against this, the ruleset name can be slightly mangled by creating
a **unique** prefix (do **not** use the one from this sample). Let us assume
the prefix "changeme-" is used, then all your rulesets should start with that
string. Then, the following call can be used:

``call_indirect "changeme-" & $syslogtag;``

While it is possible to call a ruleset via a constant name:

``call_indirect "my_ruleset";``

It is advised to use the "call" statement for this, as it offers superior
performance in this case.

additional information
----------------------

We need to have two different statements, "call" and "call_indirect" because
"call" already existed at the time "call_indirect" was added. We could not
extend "call" to support expressions, as that would have broken existing
configs. In that case ``call ruleset`` would have become invalid and
``call "ruleset"`` would have to be used instead. Thus we decided to
add the additional "call_indirect" statement for this use case.
