RFC5424 structured data parsing module (mmpstrucdata)
=====================================================

**Module Name:    mmpstrucdata**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Available since**: 7.5.4

**Description**:

The mmpstrucdata parses RFC5424 structured data into the message json
variable tree. The data parsed, if available, is stored under
"jsonRoot!rfc5424-sd!...".

 

**Module Configuration Parameters**:

Currently none.

 

**Action Confguration Parameters**:

-  **jsonRoot** - default "!"
    Specifies into which json container the data shall be parsed to.

**See Also**

-  `Howto anonymize messages that go to specific
   files <http://www.rsyslog.com/howto-anonymize-messages-that-go-to-specific-files/>`_

**Caveats/Known Bugs:**

-  this module is currently experimental; feedback is appreciated
-  property names are treated case-insensitive in rsyslog. As such,
   RFC5424 names are treated case-insensitive as well. If such names
   only differ in case (what is not recommended anyways), problems will
   occur.
-  structured data with duplicate SD-IDs and SD-PARAMS is not properly
   processed

**Samples:**

In this snippet, we parse the message and emit all json variable to a
file with the message anonymized. Note that once mmpstrucdata has run,
access to the original message is no longer possible (execept if stored
in user variables before anonymization).

module(load="mmpstrucdata") action(type="mmpstrucdata")
template(name="jsondump" type="string" string="%msg%: %$!%\\n")
action(type="omfile" file="/path/to/log" template="jsondump")

**A more practical one:**

Take this example message (inspired by RFC5424 sample;)):

``<34>1 2003-10-11T22:14:15.003Z mymachine.example.com su - ID47 [exampleSDID@32473 iut="3" eventSource="Application" eventID="1011"][id@2 test="tast"] BOM'su root' failed for lonvick on /dev/pts/8``

We apply this configuration:

module(load="mmpstrucdata") action(type="mmpstrucdata")
template(name="sample2" type="string" string="ALL: %$!%\\nSD:
%$!RFC5424-SD%\\nIUT:%$!rfc5424-sd!exampleSDID@32473!iut%\\nRAWMSG:
%rawmsg%\\n\\n") action(type="omfile" file="/path/to/log"
template="sample2")

This will output:

``ALL: { "rfc5424-sd": { "examplesdid@32473": { "iut": "3", "eventsource": "Application", "eventid": "1011" }, "id@2": { "test": "tast" } } } SD: { "examplesdid@32473": { "iut": "3", "eventsource": "Application", "eventid": "1011" }, "id@2": { "test": "tast" } } IUT:3 RAWMSG: <34>1 2003-10-11T22:14:15.003Z mymachine.example.com su - ID47 [exampleSDID@32473 iut="3" eventSource="Application" eventID="1011"][id@2 test="tast"] BOM'su root' failed for lonvick on /dev/pts/8``

As you can seem, you can address each of the individual items. Note that
the case of the RFC5424 parameter names has been converted to lower
case.

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2013 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
