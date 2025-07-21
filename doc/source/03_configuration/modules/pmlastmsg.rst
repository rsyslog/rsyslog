****************************************
pmlastmsg: last message repeated n times
****************************************

===========================  ===========================================================================
**Module Name:**             **pmlastmsg**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
**Available Since:**         5.5.6
===========================  ===========================================================================


Purpose
=======

Some syslogds are known to emit severity malformed messages with content
"last message repeated n times". These messages can mess up message
reception, as they lead to wrong interpretation with the standard
RFC3164 parser. Rather than trying to fix this issue in pmrfc3164, we
have created a new parser module specifically for these messages. The
reason is that some processing overhead is involved in processing these
messages (they must be recognized) and we would not like to place this
toll on every user but only on those actually in need of the feature.
Note that the performance toll is not large -- but if you expect a very
high message rate with ten thousands of messages per second, you will
notice a difference.

This module should be loaded first inside :doc:`rsyslog's parser
chain </02_concepts/messageparser>`. It processes all those messages that
contain a PRI, then none or some spaces and then the exact text
(case-insensitive) "last message repeated n times" where n must be an
integer. All other messages are left untouched.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


There do not currently exist any configuration parameters for this
module.


Examples
========

Systems emitting malformed "repeated msg" messages
--------------------------------------------------

This example is the typical use case, where some systems emit malformed
"repeated msg" messages. Other than that, the default :rfc:`5424` and
:rfc:`3164` parsers should be used. Note that when a parser is specified,
the default parser chain is removed, so we need to specify all three
parsers. We use this together with the default ruleset.

.. code-block:: none

   module(load="pmlastmsg")

   parser(type="pmlastmsg" name="custom.pmlastmsg")

   ruleset(name="ruleset" parser=["custom.pmlastmsg", "rsyslog.rfc5424",
                                  "rsyslog.rfc3164"]) {
        ... do processing here ...
   }

