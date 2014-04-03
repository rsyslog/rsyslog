Failover Syslog Server
======================

There are often situations where syslog data from the local system should be 
sent to a central syslogd (for consolidation, archival and whatever other 
reasons). A common problem is that messages are lost when the central syslogd 
goes down.
Rsyslog has the capability to work with failover servers to prevent message 
loss. A perquisite is that TCP based syslog forwarding is used to sent to the 
central server. The reason is that with UDP there is no reliable way to detect the remote system has gone away.
Let's assume you have a primary and two secondary central servers. Then, you 
can use the following config file excerpt to send data to them:
rsyslog.conf:

::

  *.* @@primary-syslog.example.com
  $ActionExecOnlyWhenPreviousIsSuspended on
  & @@secondary-1-syslog.example.com
  & @@secondary-2-syslog.example.com
  & /var/log/localbuffer
  $ActionExecOnlyWhenPreviousIsSuspended off

This selector processes all messages it receives (*.*). It tries to forward 
every message to primary-syslog.example.com (via tcp). If it can not reach that
server, it tries secondary-1-syslog.example.com, if that fails too, it tries 
secondary-2-syslog.example.com. If neither of these servers can be connected, 
the data is stored in /var/log/localbuffer. Please note that the secondaries 
and the local log buffer are only used if the one before them does not work. 
So ideally, /var/log/localbuffer will never receive a message. If one of the 
servers resumes operation, it automatically takes over processing again.
