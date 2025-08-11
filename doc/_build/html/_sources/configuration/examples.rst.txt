Examples
--------

Below are examples for templates and rule definitions using RainerScript.

Templates
~~~~~~~~~

Templates define how messages are formatted before being written to a file,
forwarded, or otherwise processed.

**Traditional syslogd-like format:**

.. code-block:: rsyslog

   template(name="TraditionalFormat" type="list") {
       property(name="timegenerated")         # Add message timestamp
       constant(value=" ")                    # Add a space
       property(name="hostname")              # Add the hostname
       property(name="syslogtag")             # Add the syslog tag
       property(name="msg" droplastlf="on")   # Add the message, removing the trailing LF
       constant(value="\n")                   # End with a newline
   }

**A template with additional details:**

.. code-block:: rsyslog

   template(name="PreciseFormat" type="list") {
       property(name="syslogpriority")        # Log priority
       constant(value=",")
       property(name="syslogfacility")        # Facility
       constant(value=",")
       property(name="timegenerated")         # Timestamp
       constant(value=",")
       property(name="hostname")              # Hostname
       constant(value=",")
       property(name="syslogtag")             # Syslog tag
       constant(value=",")
       property(name="msg")                   # Message content
       constant(value="\n")
   }

**RFC 3164 format:**

.. code-block:: rsyslog

   template(name="RFC3164fmt" type="list") {
       constant(value="<")
       property(name="pri")                   # Priority
       constant(value=">")
       property(name="timestamp")             # Timestamp (RFC 3164 format)
       constant(value=" ")
       property(name="hostname")              # Hostname
       constant(value=" ")
       property(name="syslogtag")             # Syslog tag
       property(name="msg")                   # Message content
   }

**Database insert format (with SQL option):**

.. code-block:: rsyslog

   template(name="MySQLInsert" type="list" option.sql="on") {
       constant(value="insert iut, message, receivedat values ('")
       property(name="iut")                   # Insert iut field
       constant(value="', '")
       property(name="msg" caseconversion="upper")  # Insert message, converted to upper-case
       constant(value="', '")
       property(name="timegenerated" dateformat="mysql")  # Insert timestamp in MySQL format
       constant(value="') into systemevents\n")
   }

Rule Examples
~~~~~~~~~~~~~

The following examples demonstrate how to apply filters and actions
with modern RainerScript syntax.

**Store critical messages in a dedicated file:**

.. code-block:: rsyslog

   if prifilt("*.crit") and not prifilt("kern.*") then {
       action(type="omfile" file="/var/adm/critical")   # Save critical messages except kernel to /var/adm/critical
   }

**Store all kernel messages in a file:**

.. code-block:: rsyslog

   if prifilt("kern.*") then {
       action(type="omfile" file="/var/adm/kernel")     # Log all kernel facility messages
   }

**Forward critical kernel messages (TCP) to a remote server:**

.. code-block:: rsyslog

   if prifilt("kern.crit") then {
       action(
           type="omfwd"
           target="server.example.net"                  # Destination server
           protocol="tcp"                               # Use TCP forwarding
           port="514"                                   # Standard syslog port
           template="RFC3164fmt"                        # Use RFC 3164 format
       )
   }

**Send emergency messages to all logged-in users:**

.. code-block:: rsyslog

   if prifilt("*.emerg") then {
       action(type="omusrmsg" users="*")                # Send emergencies to all users
   }

**Forward all logs to a remote server (TCP):**

.. code-block:: rsyslog

   action(
       type="omfwd"
       target="server.example.net"                      # Destination server
       protocol="tcp"                                   # Use TCP
       port="514"                                       # Standard syslog port
   )

**Filter messages containing “error” and forward them:**

.. code-block:: rsyslog

   if ($msg contains "error") then {
       action(
           type="omfwd"
           target="server.example.net"                  # Destination server
           protocol="udp"                               # Use UDP
       )
   }

