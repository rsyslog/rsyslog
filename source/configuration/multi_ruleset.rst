Multiple Rulesets in rsyslog
============================

Starting with version 4.5.0 and 5.1.1,
`rsyslog <http://www.rsyslog.com>`_ supports multiple rulesets within a
single configuration. This is especially useful for routing the
reception of remote messages to a set of specific rules. Note that the
input module must support binding to non-standard rulesets, so the
functionality may not be available with all inputs.

In this document, I am using `imtcp <imtcp.html>`_, an input module that
supports binding to non-standard rulesets since rsyslog started to
support them.

What is a Ruleset?
------------------

If you have worked with (r)syslog.conf, you know that it is made up of
what I call rules (others tend to call them selectors, a sysklogd term).
Each rule consist of a filter and one or more actions to be carried out
when the filter evaluates to true. A filter may be as simple as a
traditional syslog priority based filter (like "\*.\*" or "mail.info" or
a as complex as a script-like expression. Details on that are covered in
the config file documentation. After the filter come action specifiers,
and an action is something that does something to a message, e.g. write
it to a file or forward it to a remote logging server.

A traditional configuration file is made up of one or more of these
rules. When a new message arrives, its processing starts with the first
rule (in order of appearance in rsyslog.conf) and continues for each
rule until either all rules have been processed or a so-called "discard"
action happens, in which case processing stops and the message is thrown
away (what also happens after the last rule has been processed).

The **multi-ruleset** support now permits to specify more than one such
rule sequence. You can think of a traditional config file just as a
single default rule set, which is automatically bound to each of the
inputs. This is even what actually happens. When rsyslog.conf is
processed, the config file parser looks for the directive

::

    $RuleSet <name>

Where name is any name the user likes (but must not start with
"RSYSLOG\_", which is the name space reserved for rsyslog use). If it
finds this directive, it begins a new rule set (if the name was not yet
know) or switches to an already-existing one (if the name was known).
All rules defined between this $RuleSet directive and the next one are
appended to the named ruleset. Note that the reserved name
"RSYSLOG\_DefaultRuleset" is used to specify rsyslogd's default ruleset.
You can use that name wherever you can use a ruleset name, including
when binding an input to it.

Inside a ruleset, messages are processed as described above: they start
with the first rule and rules are processed in the order of appearance
of the configuration file until either there are no more rules or the
discard action is executed. Note that with multiple rulesets no longer
**all** rsyslog.conf rules are executed but **only** those that are
contained within the specific ruleset.

Inputs must explicitly bind to rulesets. If they don't do, the default
ruleset is bound.

This brings up the next question:

What does "To bind to a Ruleset" mean?
--------------------------------------

This term is used in the same sense as "to bind an IP address to an
interface": it means that a specific input, or part of an input (like a
tcp listener) will use a specific ruleset to "pass its messages to". So
when a new message arrives, it will be processed via the bound ruleset.
Rule from all other rulesets are irrelevant and will never be processed.

This makes multiple rulesets very handy to process local and remote
message via separate means: bind the respective receivers to different
rule sets, and you do not need to separate the messages by any other
method.

Binding to rulesets is input-specific. For imtcp, this is done via the

::

    $InputTCPServerBindRuleset <name>

directive. Note that "name" must be the name of a ruleset that is
already defined at the time the bind directive is given. There are many
ways to make sure this happens, but I personally think that it is best
to define all rule sets at the top of rsyslog.conf and define the inputs
at the bottom. This kind of reverses the traditional recommended
ordering, but seems to be a really useful and straightforward way of
doing things.

Why are rulesets important for different parser configurations?
---------------------------------------------------------------

Custom message parsers, used to handle different (and potentially
otherwise-invalid) message formats, can be bound to rulesets. So
multiple rulesets can be a very useful way to handle devices sending
messages in different malformed formats in a consistent way.
Unfortunately, this is not uncommon in the syslog world. An in-depth
explanation with configuration sample can be found at the
`$RulesetParser <rsconf1_rulesetparser.html>`_ configuration directive.

Can I use a different Ruleset as the default?
---------------------------------------------

This is possible by using the

::

    $DefaultRuleset <name>

Directive. Please note, however, that this directive is actually global:
that is, it does not modify the ruleset to which the next input is bound
but rather provides a system-wide default rule set for those inputs that
did not explicitly bind to one. As such, the directive can not be used
as a work-around to bind inputs to non-default rulesets that do not
support ruleset binding.

Rulesets and Main Queues
------------------------

By default, rulesets do not have their own queue. It must be activated
via the $RulesetCreateMainQueue directive, or if using rainerscript
format, by specifying queue parameters on the ruleset directive, e.g.
ruleset(name="whatever" queue.type="fixedArray" queue. ...) See
`http://www.rsyslog.com/doc/queue\_parameters.html <http://www.rsyslog.com/doc/queue_parameters.html>`_
for more details.

Examples
--------

Split local and remote logging
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Let's say you have a pretty standard system that logs its local messages
to the usual bunch of files that are specified in the default
rsyslog.conf. As an example, your rsyslog.conf might look like this:

::

    # ... module loading ...
    # The authpriv file has restricted access.
    authpriv.*  /var/log/secure
    # Log all the mail messages in one place.
    mail.*      /var/log/maillog
    # Log cron stuff
    cron.*      /var/log/cron
    # Everybody gets emergency messages
    *.emerg     *
    ... more ...

Now, you want to add receive messages from a remote system and log these
to a special file, but you do not want to have these messages written to
the files specified above. The traditional approach is to add a rule in
front of all others that filters on the message, processes it and then
discards it:

::

    # ... module loading ...
    # process remote messages
    :fromhost-ip, isequal, "192.0.2.1"    /var/log/remotefile
    & ~
    # only messages not from 192.0.21 make it past this point

    # The authpriv file has restricted access.
    authpriv.*                            /var/log/secure
    # Log all the mail messages in one place.
    mail.*                                /var/log/maillog
    # Log cron stuff
    cron.*                                /var/log/cron
    # Everybody gets emergency messages
    *.emerg                               *
    ... more ...

Note the tilde character, which is the discard action!. Also note that
we assume that 192.0.2.1 is the sole remote sender (to keep it simple).

With multiple rulesets, we can simply define a dedicated ruleset for the
remote reception case and bind it to the receiver. This may be written
as follows:

::

    # ... module loading ...
    # process remote messages
    # define new ruleset and add rules to it:
    $RuleSet remote
    *.*           /var/log/remotefile
    # only messages not from 192.0.21 make it past this point

    # bind ruleset to tcp listener
    $InputTCPServerBindRuleset remote
    # and activate it:
    $InputTCPServerRun 10514

    # switch back to the default ruleset:
    $RuleSet RSYSLOG_DefaultRuleset
    # The authpriv file has restricted access.
    authpriv.*    /var/log/secure
    # Log all the mail messages in one place.
    mail.*        /var/log/maillog
    # Log cron stuff
    cron.*        /var/log/cron
    # Everybody gets emergency messages
    *.emerg       *
    ... more ...

Here, we need to switch back to the default ruleset after we have
defined our custom one. This is why I recommend a different ordering,
which I find more intuitive. The sample below has it, and it leads to
the same results:

::

    # ... module loading ...
    # at first, this is a copy of the unmodified rsyslog.conf
    # The authpriv file has restricted access.
    authpriv.*    /var/log/secure
    # Log all the mail messages in one place.
    mail.*        /var/log/maillog
    # Log cron stuff
    cron.*        /var/log/cron
    # Everybody gets emergency messages
    *.emerg       *
    ... more ...
    # end of the "regular" rsyslog.conf. Now come the new definitions:

    # process remote messages
    # define new ruleset and add rules to it:
    $RuleSet remote
    *.*           /var/log/remotefile

    # bind ruleset to tcp listener
    $InputTCPServerBindRuleset remote
    # and activate it:
    $InputTCPServerRun 10514

Here, we do not switch back to the default ruleset, because this is not
needed as it is completely defined when we begin the "remote" ruleset.

Now look at the examples and compare them to the single-ruleset
solution. You will notice that we do **not** need a real filter in the
multi-ruleset case: we can simply use "\*.\*" as all messages now means
all messages that are being processed by this rule set and all of them
come in via the TCP receiver! This is what makes using multiple rulesets
so much easier.

Split local and remote logging for three different ports
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This example is almost like the first one, but it extends it a little
bit. While it is very similar, I hope it is different enough to provide
a useful example why you may want to have more than two rulesets.

Again, we would like to use the "regular" log files for local logging,
only. But this time we set up three syslog/tcp listeners, each one
listening to a different port (in this example 10514, 10515, and 10516).
Logs received from these receivers shall go into different files. Also,
logs received from 10516 (and only from that port!) with "mail.\*"
priority, shall be written into a specif file and **not** be written to
10516's general log file.

This is the config:

::

    # ... module loading ...
    # at first, this is a copy of the unmodified rsyslog.conf
    # The authpriv file has restricted access.
    authpriv.* /var/log/secure
    # Log all the mail messages in one place.
    mail.*  /var/log/maillog
    # Log cron stuff
    cron.*  /var/log/cron
    # Everybody gets emergency messages
    *.emerg       *
    ... more ...
    # end of the "regular" rsyslog.conf. Now come the new definitions:

    # process remote messages

    #define rulesets first
    $RuleSet remote10514
    *.*     /var/log/remote10514

    $RuleSet remote10515
    *.*     /var/log/remote10515

    $RuleSet remote10516
    mail.*  /var/log/mail10516
    &       ~
    # note that the discard-action will prevent this messag from 
    # being written to the remote10516 file - as usual...
    *.*     /var/log/remote10516

    # and now define listners bound to the relevant ruleset
    $InputTCPServerBindRuleset remote10514
    $InputTCPServerRun 10514

    $InputTCPServerBindRuleset remote10515
    $InputTCPServerRun 10515

    $InputTCPServerBindRuleset remote10516
    $InputTCPServerRun 10516

Note that the "mail.\*" rule inside the "remote10516" ruleset does not
affect processing inside any other rule set, including the default rule
set.

Performance
-----------

Fewer Filters
~~~~~~~~~~~~~

No rule processing can be faster than not processing a rule at all. As
such, it is useful for a high performance system to identify disjunct
actions and try to split these off to different rule sets. In the
example section, we had a case where three different tcp listeners need
to write to three different files. This is a perfect example of where
multiple rule sets are easier to use and offer more performance. The
performance is better simply because there is no need to check the
reception service - instead messages are automatically pushed to the
right rule set and can be processed by very simple rules (maybe even
with "\*.\*"-filters, the fastest ones available).

Partitioning of Input Data
~~~~~~~~~~~~~~~~~~~~~~~~~~

Starting with rsyslog 5.3.4, rulesets permit higher concurrency. They
offer the ability to run on their own "main" queue. What that means is
that a own queue is associated with a specific rule set. That means that
inputs bound to that ruleset do no longer need to compete with each
other when they enqueue a data element into the queue. Instead, enqueue
operations can be completed in parallel.

An example: let us assume we have three TCP listeners. Without rulesets,
each of them needs to insert messages into the main message queue. So if
each of them wants to submit a newly arrived message into the queue at
the same time, only one can do so while the others need to wait. With
multiple rulesets, its own queue can be created for each ruleset. If now
each listener is bound to its own ruleset, concurrent message submission
is possible. On a machine with a sufficiently large number of cores,
this can result in dramatic performance improvement.

It is highly advised that high-performance systems define a dedicated
ruleset, with a dedicated queue for each of the inputs.

By default, rulesets do **not** have their own queue. It must be
activated via the
`$RulesetCreateMainQueue <rsconf1_rulesetcreatemainqueue.html>`_
directive.

Future Enhancements
~~~~~~~~~~~~~~~~~~~

In the long term, multiple rule sets will probably lay the foundation
for even better optimizations. So it is not a bad idea to get aquainted
with them.

[`manual index <manual.html>`_\ ] [`rsyslog
site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2009 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
