ommongodb: MongoDB Output Module
================================

**Module Name:    ommongodb**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

This module provides native support for logging to MongoDB.

**Action Parameters**:

Note: parameter names are case-insensitive.

-  **uristr**
   MongoDB connexion string, as defined by the MongoDB String URI Format (See: https://docs.mongodb.com/manual/reference/connection-string/). If uristr is defined, following directives will be ignored: server, serverport, uid, pwd.
-  **ssl_cert**
   Absolute path to the X509 certificate you want to use for TLS client authentication. This is optional.
-  **ssl_ca**
   Absolute path to the trusted X509 CA certificate that signed the mongoDB server certificate. This is optional.
-  **server** (deprecated, use "uristr" instead)
   Name or address of the MongoDB server
-  **serverport** (deprecated, use "uristr" instead)
   Permits to select a non-standard port for the MongoDB server. The
   default is 0, which means the system default port is used. There is
   no need to specify this parameter unless you know the server is
   running on a non-standard listen port.
-  **db**
   Database to use
-  **collection**
   Collection to use
-  **uid** (deprecated, use "uristr" instead)
   logon userid used to connect to server. Must have proper permissions.
-  **pwd** (decrecated, use "uristr" instead)
   the user's password
-  **template**
   Template to use when submitting messages.


Note rsyslog contains a canned default template to write to the MongoDB.
It will be used automatically if no other template is specified to be
used. This template is:

::

  template(name="BSON" type="string" string="\\"sys\\" : \\"%hostname%\\",
  \\"time\\" : \\"%timereported:::rfc3339%\\", \\"time\_rcvd\\" :
  \\"%timegenerated:::rfc3339%\\", \\"msg\\" : \\"%msg%\\",
  \\"syslog\_fac\\" : \\"%syslogfacility%\\", \\"syslog\_server\\" :
  \\"%syslogseverity%\\", \\"syslog\_tag\\" : \\"%syslogtag%\\",
  \\"procid\\" : \\"%programname%\\", \\"pid\\" : \\"%procid%\\",
  \\"level\\" : \\"%syslogpriority-text%\\"")

This creates the BSON document needed for MongoDB if no template is
specified. The default schema is aligned to CEE and project lumberjack.
As such, the field names are standard lumberjack field names, and
**not** `rsyslog property names <property_replacer.html>`_. When
specifying templates, be sure to use rsyslog property names as given in
the table. If you would like to use lumberjack-based field names inside
MongoDB (which probably is useful depending on the use case), you need
to select fields names based on the lumberjack schema. If you just want
to use a subset of the fields, but with lumberjack names, you can look
up the mapping in the default template. For example, the lumberjack
field "level" contains the rsyslog property "syslogpriority-text".

**Sample:**

The following sample writes all syslog messages to the database "syslog"
and into the collection "log" on mongoserver.example.com. The server is
being accessed under the account of "user" with password "pwd". Please note
that this syntax is deprecated by the "uristr" directive, as shown below.

::

  module(load="ommongodb")
  action(type="ommongodb"
         server="mongoserver.example.com" db="syslog" collection="log"
         uid="user" pwd="pwd")


Another sample that uses the new "uristr" directives to connect to a TLS mongoDB server with TLS and client authentication.

::

   module(load="ommongodb")
   action(type="ommongodb"
         uristr="mongodb://vulture:9091,vulture2:9091/?replicaset=Vulture&ssl=true"
         ssl_cert="/var/db/mongodb/mongod.pem"
         ssl_ca="/var/db/mongodb/ca.pem"
         db="logs" collection="syslog")

