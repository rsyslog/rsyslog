omhiredis: Redis Output Module
==============================

**Module Name:** omhiredis

**Original Author:** Brian Knox <bknox@digitalocean.com>

**Description**

This module provides native support for writing to Redis, 
using the hiredis client library.

**Action Parameters**

- **server**
  Name or address of the Redis server

- **serverport**
  Port of the Redis server if the server is not listening on the default port.
  
- **serverpassword**
  Password to support authenticated redis database server to push messages 
  across networks and datacenters. Parameter is optional if not provided 
  AUTH command wont be sent to the server.

- **mode**
  Mode to run the output action in: "queue" or "publish". If not supplied, the
  original "template" mode is used. Note due to a config parsing bug in 8.13, 
  explicitly setting this to "template" mode will result in a config parsing
  error.

- **template**
  Template is required if using "template" mode. 

- **key**
  Key is required if using "publish" or "queue" mode.


**Examples**

*Mode: template*

In "template" mode, the string constructed by the template is sent
to Redis as a command. Note this mode has problems with strings
with spaces in them - full message won't work correctly. In this
mode, the template argument is required, and the key argument
is meaningless.

::
  module(load="omhiredis")

  template(
    name="program_count_tmpl"
    type="string"
    string="HINCRBY progcount %programname% 1")

  action(
    name="count_programs"
    server="my-redis-server.example.com"
    serverpassword="my-redis-password"
    port="6379"
    type="omhiredis"
    mode="template"
    template="program_count_tmpl")

Here's an example redis-cli session where we HGETALL the counts:

::
  > redis-cli 
  127.0.0.1:6379> HGETALL progcount
  1) "rsyslogd"
  2) "35"
  3) "rsyslogd-pstats"
  4) "4302"

*Mode: queue*

In "queue" mode, the syslog message is pushed into a Redis list
at "key", using the LPUSH command. If a template is not supplied,
the plugin will default to the RSYSLOG_ForwardFormat template.

::
  module(load="omhiredis")

  action(
    name="push_redis"
    server="my-redis-server.example.com"
    serverpassword="my-redis-password"
    port="6379"
    type="omhiredis"
    mode="queue"
    key="my_queue")

Here's an example redis-cli session where we RPOP from the queue:

::
  > redis-cli 
  127.0.0.1:6379> RPOP my_queue

  "<46>2015-09-17T10:54:50.080252-04:00 myhost rsyslogd: [origin software=\"rsyslogd\" swVersion=\"8.13.0.master\" x-pid=\"6452\" x-info=\"http://www.rsyslog.com\"] start"

  127.0.0.1:6379> 

*Mode: publish*

In "publish" mode, the syslog message is published to a Redis
topic set by "key".  If a template is not supplied, the plugin
will default to the RSYSLOG_ForwardFormat template.

::
  module(load="omhiredis")

  action(
    name="publish_redis"
    server="my-redis-server.example.com"
    serverpassword="my-redis-password"
    port="6379"
    type="omhiredis"
    mode="publish"
    key="my_channel")

Here's an example redis-cli session where we SUBSCRIBE to the topic:

::
  > redis-cli 

  127.0.0.1:6379> subscribe my_channel

  Reading messages... (press Ctrl-C to quit)

  1) "subscribe"

  2) "my_channel"

  3) (integer) 1

  1) "message"

  2) "my_channel"

  3) "<46>2015-09-17T10:55:44.486416-04:00 myhost rsyslogd-pstats: {\"name\":\"imuxsock\",\"origin\":\"imuxsock\",\"submitted\":0,\"ratelimit.discarded\":0,\"ratelimit.numratelimiters\":0}"

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.

Copyright © 2008-2015 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
