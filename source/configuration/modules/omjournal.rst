omjournal: Systemd Journal Output
==================================

**Module Name:    omjournal**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

This module provides native support for logging to the systemd journal.

**Action Parameters**:

-  **template**
   Template to use when submitting messages.

By default, rsyslog will use the incoming %msg% as the MESSAGE field
of the journald entry, and include the syslog tag and priority.

You can override the default formatting of the message, and include 
custom fields with a template. Complex fields in the template
(eg. json entries) will be added to the journal as json text. Other 
fields will be coerced to strings.

Journald requires that you include a template parameter named MESSAGE.

**Sample:**

The following sample writes all syslog messages to the journal with a
custom EVENT_TYPE field.

::

  module(load="omjournal")

  template(name="journal" type="list") {
    constant(value="Something happened" outname="MESSAGE")
    property(name="$!event-type" outname="EVENT_TYPE")
  }

  action(type="omjournal" template="journal")


This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2008-2014 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the ASL 2.0.
