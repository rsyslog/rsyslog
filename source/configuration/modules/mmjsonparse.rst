JSON/CEE Structured Content Extraction Module (mmjsonparse)
===========================================================

**Module Name:    mmjsonparse**

**Available since:**\ 6.6.0+

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

This module provides support for parsing structured log messages that
follow the CEE/lumberjack spec. The so-called "CEE cookie" is checked
and, if present, the JSON-encoded structured message content is parsed.
The properties are then available as original message properties.

The "CEE cookie" is the character squence "@cee:" which must prepend the
actual JSON. Note that the JSON must be valid and MUST NOT be followed
by any non-JSON message. If either of these conditions is not true,
mmjsonparse will **not** parse the associated JSON. This is based on the
cookie definition used in CEE/project lumberjack and is meant to aid
against an erroneous detection of a message as being CEE where it is
not.

This also means that mmjsonparse currently is NOT a generic JSON parser
that picks up JSON from whereever it may occur in the message. This is
intentional, but future versions may support config parameters to relax
the format requirements.

**Action specific Configuration Directives**:

- **cookie** [string] defaults to "@cee:"

  Permits to set the cookie that must be present in front of the
  JSON part of the message.

  Most importantly, this can be set to the empty string ("") in order
  to not require any cookie. In this case, leading spaces are permitted
  in front of the JSON. No non-whitespace characters are permitted
  after the JSON. If such is required, mmnormalize must be used.

**Sample:**

This activates the module and applies normalization to all messages::

  module(load="mmjsonparse")
  action(type="mmjsonparse")

To permit parsing messages without cookie, use this action statement::

  action(type="mmjsonparse" cookie="")

The same in legacy format::

  $ModLoad mmjsonparse 
  *.* :mmjsonparse:

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2012 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
