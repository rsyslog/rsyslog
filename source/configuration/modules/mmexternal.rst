Support module for external message modification modules
========================================================

**Module Name:    mmexternal**

**Available since:   ** 8.3.0

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

This module permits to integrate external message modification plugins
into rswyslog.
 

**Action Parameters**:

-  **binary**

   The name of the external message modification plugin to be called. This
   can be a full path name.

- **interface.input**

  This can either be "msg", "rawmsg" or "fulljson". In case of "fulljson", the
  message object is provided as a json object. Otherwise, the respective
  property is provided. This setting **must** match the external plugin's
  expectations. Check the external plugin documentation for what needs to be used.

- **output**
  
  This is a debug aid. If set, this is a filename where the plugins output
  is logged. Note that the output is also being processed as usual by rsyslog.
  Setting this parameter thus gives insight into the internal processing
  that happens between plugin and rsyslog core.

- **forcesingleinstance**

  This is an expert parameter, just like the equivalent *omprog* parameter.
  See the message modification plugin's documentation if it is needed.

**Caveats/Known Bugs:**

-  None.

**Sample:**

The following command writes all syslog messages into a file.

::

  module (load="mmexternal")
  action(type="mmexternal"
         binary="/pathto/mmexternal.py 

