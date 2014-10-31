Log Message Normalization Module (mmjsonparse)
==============================================

**Module Name:    mmjsonparse**

**Available since:**\ 6.6.0+

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

This module provides support for parsing structured log messages that
follow the CEE/lumberjack spec. The so-called "CEE cookie" is checked
and, if present, the JSON-encoded structured message content is parsed.
The properties are than available as original message properties.

The "CEE cookie" is the character squence "@cee:" which must prepend the
actual JSON. Note that the JSON must be valid and MUST NOT be followed
by any non-JSON message. If either of these conditions is not true,
mmjsonparse will **not** parse the associated JSON. This is based on the
cookie definition used in CEE/project lumberjack and is meant to aid
against an errornous detection of a message as being CEE where it is
not.

This also means that mmjsonparse currently is NOT a generic JSON parser
that picks up JSON from whereever it may occur in the message. This is
intentional, but future versions may support config parameters to relax
the format requirements.

**Action specific Configuration Directives**:

currently none

**Sample:**

This activates the module and applies normalization to all messages:

module(load="mmjsonparse") action(type="mmjsonparse")

The same in legacy format:

$ModLoad mmjsonparse \*.\* :mmjsonparse:

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2012 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
