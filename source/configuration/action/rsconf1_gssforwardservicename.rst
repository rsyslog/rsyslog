$GssForwardServiceName
----------------------

**Type:** global configuration directive

**Default:** host

**Provided by:** *omgssapi*

**Description:**

Specifies the service name used by the client when forwarding GSS-API
wrapped messages.

The GSS-API service names are constructed by appending '@' and a
hostname following "@@" in each selector.

**Sample:**

``$GssForwardServiceName rsyslog``

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2007 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 2 or higher.
