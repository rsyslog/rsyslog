.. SPDX-License-Identifier: Apache-2.0

.. meta::
   :description: Configure the built-in pmlastmsg parser for malformed repeated-message records.
   :keywords: rsyslog, pmlastmsg, rsyslog.lastline, parser chain, repeated messages

.. _ref-pmlastmsg:

*************************************************
pmlastmsg: "last message repeated n times" parser
*************************************************

===========================  ===========================================================================
**Module Name:**             **pmlastmsg**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
**Available Since:**         5.5.6
===========================  ===========================================================================


Purpose
=======

.. summary-start

``pmlastmsg`` provides the built-in parser ``rsyslog.lastline`` for malformed
"last message repeated n times" records.

.. summary-end

Some syslog daemons are known to emit severity malformed messages with content
"last message repeated n times". These messages can mess up message reception,
as they lead to wrong interpretation with the standard RFC3164 parser. Rather
than trying to fix this issue in pmrfc3164, we have created a new parser module
specifically for these messages. The reason is that some processing overhead is
involved in processing these messages (they must be recognized) and we would not
like to place this toll on every user but only on those actually in need of the
feature. Note that the performance toll is not large -- but if you expect a very
high message rate with ten thousands of messages per second, you will notice a
difference.

This module should be loaded first inside :doc:`rsyslog's parser
chain </concepts/messageparser>`. It processes all those messages that
contain a PRI, then none or some spaces and then the exact text
(case-insensitive) "last message repeated n times" where n must be an
integer. All other messages are left untouched.


Configuration Parameters
========================

Parser Parameters
-----------------

There are no configurable parser-instance parameters. Loading ``pmlastmsg``
registers the built-in parser ``rsyslog.lastline``. Reference that name directly
in the parser chain; custom ``parser(type="pmlastmsg" ...)`` instances are not
supported.


Examples
========

Systems emitting malformed "repeated msg" messages
--------------------------------------------------

This example is the typical use case, where some systems emit malformed
"repeated msg" messages. Other than that, the default :rfc:`5424` and
:rfc:`3164` parsers should be used. Note that when a parser is specified,
the default parser chain is removed, so we need to specify all three
parsers. This example accepts messages on loopback TCP port 10514 and binds
that input to the named ruleset ``repeated_messages``.

.. rsyslog-doc-sample: validate-config
   :require-plugin: pmlastmsg
   :require-plugin: imtcp

.. code-block:: rsyslog

   module(load="pmlastmsg")
   module(load="imtcp")

   ruleset(name="repeated_messages"
           parser=["rsyslog.lastline", "rsyslog.rfc5424", "rsyslog.rfc3164"]) {
       action(type="omfile" file="/var/log/remote.log")
   }

   input(type="imtcp" address="127.0.0.1" port="10514"
         ruleset="repeated_messages")

