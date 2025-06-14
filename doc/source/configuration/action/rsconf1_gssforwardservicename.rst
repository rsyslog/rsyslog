$GssForwardServiceName
----------------------

**Type:** global configuration parameter

**Default:** host

**Provided by:** *omgssapi*

**Description:**

Specifies the service name used by the client when forwarding GSS-API
wrapped messages.

The GSS-API service names are constructed by appending '@' and a
hostname following "@@" in each selector.

**Sample:**

``$GssForwardServiceName rsyslog``

