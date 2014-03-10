imrelp: RELP Input Module
=========================

Provides the ability to receive syslog messages via the reliable RELP
protocol. This module requires `librelp <http://www.librelp.com>`_ to be
present on the system. From the user's point of view, imrelp works much
like imtcp or imgssapi, except that no message loss can occur.

Clients send messages to the RELP server via omrelp.

**Author:** Rainer Gerhards

Configuration Directives
------------------------

.. function:: InputRELPServerRun <port>

   Starts a RELP server on selected port

Caveats/Known Bugs
------------------

-  With the currently supported relp protocol version, a minor
   message duplication may occur if a network connection between the 
   relp client and relp server breaks after the client could 
   successfully send some messages but the server could not acknowledge 
   them. The window of opportunity is very slim, but in theory this is 
   possible. Future versions of RELP will prevent this.
-  rsyslogd may lose a few messages if rsyslog is shutdown while a 
   network conneciton to the server is broken and could not yet be 
   recovered. Future version of RELP support in rsyslog will prevent that.
-  Please note that both scenarios also exists with plain tcp syslog. 
   RELP, even with the small nits outlined above, is a much more reliable 
   solution than plain tcp syslog and so it is highly suggested to use 
   RELP instead of plain tcp.
-  To obtain the remote system's IP address, you need to have at least
   librelp 1.0.0 installed. Versions below it return the hostname
   instead of the IP address.

Example
-------

This sets up a RELP server on port 20514.

::

  $ModLoad imrelp # needs to be done just once
  $InputRELPServerRun 20514

