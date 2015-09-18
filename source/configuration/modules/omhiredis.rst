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

- **mode**
  Mode to run the output action in: "template", "queue" or "publish".

- **template**
  Template is required if using "template" mode. 

- **key**
  Key is required if using "publish" or "queue" mode.


**Examples**

*Mode: template*

In "template" mode, the string constructed by the template is sent
to Redis as a command. Note this mode has problems with strings
with spaces in them - full message won't work correctly.

::
  module(load="omhiredis")

  template(
    name="program_count_tmpl"
    type="string"
    string="HINCRBY progcount %programname% 1")

  action(
    name="count_programs"
    type="omhiredis"
    mode="template"
    template="program_count_tmpl")

*Mode: queue*

In "queue" mode, the syslog message is pushed into a Redis list
at "key", using the LPUSH command. If a template is not supplied,
the plugin will default to the RSYSLOG_ForwardFormat template.

::
  module(load="omhiredis")

  action(
    name="push_redis"
    type="omhiredis"
    mode="queue"
    key="my_queue")

*Mode: publish*

In "publish" mode, the syslog message is published to a Redis
topic set by "key".  If a template is not supplied, the plugin
will default to the RSYSLOG_ForwardFormat template.

::
  module(load="omhiredis")

  action(
    name="publish_redis"
    type="omhiredis"
    mode="publish"
    key="my_channel")


This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.

Copyright © 2008-2015 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
