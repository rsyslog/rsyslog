Multiple Rulesets in rsyslog
============================

Starting with version 4.5.0 and 5.1.1,
`rsyslog <http://www.rsyslog.com>`_ supports multiple rulesets within a
single configuration. This is especially useful for routing the
reception of remote messages to a set of specific rules. Note that the
input module must support binding to non-standard rulesets, so the
functionality may not be available with all inputs.

In this document, I am using :doc:`imtcp <../configuration/modules/imtcp>`, an input module that
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

    ruleset(name="rulesetname")

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

Inputs must explicitly bind to rulesets. If they don't, the default
ruleset is bound.

This brings up the next question:

What does "To bind to a Ruleset" mean?
--------------------------------------

This term is used in the same sense as "to bind an IP address to an
interface": it means that a specific input, or part of an input (like a
tcp listener) will use a specific ruleset to "pass its messages to". So
when a new message arrives, it will be processed via the bound ruleset.
Rules from all other rulesets are irrelevant and will never be processed.

This makes multiple rulesets very handy to process local and remote
message via separate means: bind the respective receivers to different
rule sets, and you do not need to separate the messages by any other
method.

Binding to rulesets is input-specific. For imtcp, this is done via the

::

    input(type="imptcp" port="514" ruleset="rulesetname")

directive. Note that "rulesetname" must be the name of a ruleset that is
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
:doc:`$RulesetParser <../configuration/ruleset/rsconf1_rulesetparser>` configuration directive.

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

Rulesets and Queues
-------------------

By default, rulesets do not have their own queue. It must be activated
via the $RulesetCreateMainQueue directive, or if using rainerscript
format, by specifying queue parameters on the ruleset directive, e.g.

::

   ruleset(name="whatever" queue.type="fixedArray" queue. ...)

See :doc:`http://www.rsyslog.com/doc/master/rainerscript/queue\_parameters.html <../rainerscript/queue_parameters>`
for more details.

Please note that when a ruleset uses its own queue, processing of the ruleset
happens **asynchronously** to the rest of processing. As such, any modifications
made to the message object (e.g. message or local variables that are set) or
discarding of the message object **have no effect outside that ruleset**. So
if you want to modify the message object inside the ruleset, you **cannot**
define a queue for it. Most importantly, you cannot call it and expect the
modified properties to be present when the call returns. Even more so, the
call will most probably return before the message is even begun to be processed
by the ruleset in question.

Note that in RainerScript format specifying any "queue.\*" can cause the
creation of a dedicated queue and as such asynchronous processing. This is
because queue parameters cannot be specified without a queue. Note, though,
that the actual creation is **guaranteed** only if "queue.type" is specified
as above. So if you intentionally want to assign a separate queue to the
ruleset, do so as shown above.

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
    if $fromhost-ip == '192.0.2.1' then {
            action(type="omfile" file="/var/log/remotefile02")
            stop
        }


    # only messages not from 192.0.2.1 make it past this point

    # The authpriv file has restricted access.
    authpriv.*                            /var/log/secure
    # Log all the mail messages in one place.
    mail.*                                /var/log/maillog
    # Log cron stuff
    cron.*                                /var/log/cron
    # Everybody gets emergency messages
    *.emerg                               *
    ... more ...

Note that "stop" is the discard action!. Also note that we assume that
192.0.2.1 is the sole remote sender (to keep it simple).

With multiple rulesets, we can simply define a dedicated ruleset for the
remote reception case and bind it to the receiver. This may be written
as follows:

::

    # ... module loading ...
    # process remote messages
    # define new ruleset and add rules to it:
    ruleset(name="remote"){
        action(type="omfile" file="/var/log/remotefile")
    }
    # only messages not from 192.0.2.1 make it past this point

    # bind ruleset to tcp listener and activate it:
    input(type="imptcp" port="10514" ruleset="remote")

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
priority, shall be written into a specific file and **not** be written to
10516's general log file.

This is the config:

::

    # ... module loading ...
    # process remote messages

    ruleset(name="remote10514"){
        action(type="omfile" file="/var/log/remote10514")
    }

    ruleset(name="remote10515"){
        action(type="omfile" file="/var/log/remote10515")
    }

    ruleset(name="remote10516"){
        if prifilt("mail.*") then {
            /var/log/mail10516
            stop
            # note that the stop-command will prevent this message from 
            # being written to the remote10516 file - as usual...   
        }
        /var/log/remote10516
    }


    # and now define listeners bound to the relevant ruleset
    input(type="imptcp" port="10514" ruleset="remote10514")
    input(type="imptcp" port="10515" ruleset="remote10515")
    input(type="imptcp" port="10516" ruleset="remote10516")

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
:doc:`$RulesetCreateMainQueue <../configuration/ruleset/rsconf1_rulesetcreatemainqueue>`
directive.
