$RulesetParser
--------------

**Type:** ruleset-specific configuration directive

**Parameter Values:** string

**Available since:** 5.3.4+

**Default:** rsyslog.rfc5424 followed by rsyslog.rfc3164

**Description:**

This directive permits to specify which `message
parsers <../../concepts/messageparser.html>`_ should be used for the ruleset in
question. It no ruleset is explicitly specified, the default ruleset is
used. Message parsers are contained in (loadable) parser modules with
the most common cases (RFC3164 and RFC5424) being build-in into
rsyslogd.

When this directive is specified the first time for a ruleset, it will
not only add the parser to the ruleset's parser chain, it will also wipe
out the default parser chain. So if you need to have them in addition to
the custom parser, you need to specify those as well.

Order of directives is important. Parsers are tried one after another,
in the order they are specified inside the config. As soon as a parser
is able to parse the message, it will do so and no other parsers will be
executed. If no matching parser can be found, the message will be
discarded and a warning message be issued (but only for the first 1,000
instances of this problem, to prevent message generation loops).

Note that the rfc3164 parser will **always** be able to parse a message
- it may just not be the format that you like. This has two important
implications: 1) always place that parser at the END of the parser list,
or the other parsers after it will never be tried and 2) if you would
like to make sure no message is lost, placing the rfc3164 parser at the
end of the parser list ensures that.

Multiple parser modules are very useful if you have various devices that
emit messages that are malformed in various ways. The route to take then
is

-  make sure you find a custom parser for that device; if there is no
   one, you may consider writing one yourself (it is not that hard) or
   getting one written as part of `Adiscon's professional services for
   rsyslog <http://www.rsyslog.com/professional-services>`_.
-  load your custom parsers via $ModLoad
-  create a ruleset for each malformed format; assign the custom parser
   to it
-  create a specific listening port for all devices that emit the same
   malformed format
-  bind the listener to the ruleset with the required parser

Note that it may be cumbersome to add all rules to all rulesets. To
avoid this, you can either use $Include or `omruleset <omruleset.html>`_
(what probably provides the best solution).

More information about rulesets in general can be found in
:doc:`multi-ruleset support in rsyslog <../../concepts/multi_ruleset>`.

**Caveats:**

currently none known

**Example:**

This example assumes there are two devices emitting malformed messages
via UDP. We have two custom parsers for them, named "device1.parser" and
"device2.parser". In addition to that, we have a number of other devices
sending well-formed messages, also via UDP.

The solution is to listen for data from the two devices on two special
ports (10514 and 10515 in this example), create a ruleset for each and
assign the custom parsers to them. The rest of the messages are received
via port 514 using the regular parsers. Processing shall be equal for
all messages. So we simply forward the malformed messages to the regular
queue once they are parsed (keep in mind that a message is never again
parsed once any parser properly processed it).

::

  $ModLoad imudp
  $ModLoad pmdevice1 # load parser "device1.parser" for device 1
  $ModLoad pmdevice2 # load parser "device2.parser" for device 2

  # define ruleset for the first device sending malformed data
  $Ruleset maldev1
  $RulesetCreateMainQueue on # create ruleset-specific queue
  $RulesetParser "device1.parser" # note: this deactivates the default parsers
  # forward all messages to default ruleset:
  $ActionOmrulesetRulesetName RSYSLOG\_DefaultRuleset
  \*.\* :omruleset:

  # define ruleset for the second device sending malformed data
  $Ruleset maldev2 $RulesetCreateMainQueue on # create ruleset-specific queue
  $RulesetParser "device2.parser" # note: this deactivates the default parsers
  # forward all messages to default ruleset:
  $ActionOmrulesetRulesetName RSYSLOG\_DefaultRuleset
  \*.\* :omruleset:

  # switch back to default ruleset
  $Ruleset RSYSLOG\_DefaultRuleset
  \*.\* /path/to/file
  auth.info @authlogger.example.net
  # whatever else you usually do...

  # now define the inputs and bind them to the rulesets
  # first the default listener (utilizing the default ruleset)
  $UDPServerRun 514

  # now the one with the parser for device type 1:
  $InputUDPServerBindRuleset maldev1
  $UDPServerRun 10514

  # and finally the one for device type 2:
  $InputUDPServerBindRuleset maldev2
  $UDPServerRun 10515

For an example of how multiple parser can be chained (and an actual use
case), please see the example section on the
`pmlastmsg <pmlastmsg.html>`_ parser module.

Note the positions of the directives. With the current config language,
**sequence of statements is very important**. This is ugly, but
unfortunately the way it currently works.

