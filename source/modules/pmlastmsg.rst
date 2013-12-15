`rsyslog module reference <rsyslog_conf_modules.html>`_

parser module for "last message repeated n times" (pmlastmsg)
=============================================================

**Module Name:    pmlastmsg**

**Module Type:    parser module**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Available Since**: 5.5.6

**Description**:

Some syslogds are known to emit severily malformed messages with content
"last message repeated n times". These messages can mess up message
reception, as they lead to wrong interpretation with the standard
RFC3164 parser. Rather than trying to fix this issue in pmrfc3164, we
have created a new parser module specifically for these messages. The
reason is that some processing overhead is involved in processing these
messages (they must be recognized) and we would not like to place this
toll on every user but only on those actually in need of the feature.
Note that the performance toll is not large -- but if you expect a very
high message rate with tenthousands of messages per second, you will
notice a difference.

This module should be loaded first inside `rsyslog's parser
chain <messageparser.html>`_. It processes all those messages that
contain a PRI, then none or some spaces and then the exact text
(case-insensitive) "last message repeated n times" where n must be an
integer. All other messages are left untouched.

**Please note:** this parser module makes it possible that these
messages are properly detected. It does **not** drop them. If you intend
to drop those messages, you need to use the usual filter logic in
combination with the discard action. As a side-note, please keep on your
mind that the sender discarded messages when the "last message repeated
n times" message is emited. You want to consider if that really is what
you intend to happen. If not, go change the sender.

**Configuration Directives**:

There do not currently exist any configuration directives for this
module.

**Examples:**

This example is the typical use case, where some systems emit malformed
"repeated msg" messages. Other than that, the default RFC5424 and
RFC3164 parsers should be used. Note that when a parser is specified,
the default parser chain is removed, so we need to specify all three
parsers. We use this together with the default ruleset.

$ModLoad pmlastmsg # this parser is NOT a built-in module # note that
parser are tried in the # order they appear in rsyslog.conf, so put
pmlastmsg first $RulesetParser rsyslog.lastline # as we have removed the
default parser chain, we # need to add the default parsers as well.
$RulesetParser rsyslog.rfc5424 $RulesetParser rsyslog.rfc3164 # now come
the typical rules, like... \*.\* /path/to/file.log

**Caveats/Known Bugs:**

currently none

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2010 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
