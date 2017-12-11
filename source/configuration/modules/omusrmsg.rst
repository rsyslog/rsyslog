omusrmsg: notify users
======================

**Module Name:    omusrmsg**


**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

This module permits to send log messages to the user terminal.
 

**Module Parameters**:

Note: parameter names are case-insensitive.

none
 

**Action Parameters**:

Note: parameter names are case-insensitive.

-  **users**
   the name of the users to send data to.

-  **template**
   template to user for the message.

**Caveats/Known Bugs:**

-  None.

**Sample:**

The following command writes emergency messages to all users::

  action(type="omusrmsg" users="*")
