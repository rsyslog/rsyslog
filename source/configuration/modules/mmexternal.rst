Support module for external message modification modules
========================================================

**Module Name:    mmexternal**

**Available since:   ** 8.3.0

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

This module permits to integrate external message modification plugins
into rsyslog.

For details on the interface specification, see rsyslog's source in the
./plugins/external/INTERFACE.md.
 

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

The following config file snippet is used to write execute an external
message modification module "mmexternal.py". Note that the path to the
module is specified here. This is necessary if the module is not in the
default search path.

::

  module (load="mmexternal") # needs to be done only once inside the config

  action(type="mmexternal" binary="/path/to/mmexternal.py")
