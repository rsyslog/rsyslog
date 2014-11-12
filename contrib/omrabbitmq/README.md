
# rsyslog output module for RabbitMQ

This module sends syslog messages into RabbitMQ server.

Only v6 configuration syntax is supported.

**omrabbitmq is tested only with 6.6.0 version of rsyslog.**


## Compile
To successfully compile omrabbitmq module you need [rabbitmq-c](https://github.com/alanxz/rabbitmq-c) library.

    ./configure --enable-omrabbitmq ...


----
## Configure

omrabbitmq output module supports only v6 configuration syntax.

Parameters:

* host=&lt;hostname&gt; &#8211; server
* virtual_host=&lt;virtual\_host&gt; &#8211; virtual  message  broker
* user=&lt;user&gt; &#8211; user name
* password=&lt;password&gt; &#8211; password
* exchange=&lt;name&gt; &#8211; exchange name
* routing_key=&lt;name&gt; &#8211; name of routing key


Example:

        $ModLoad omrabbitmq

        *.*    action(type="omrabbitmq" 
                 host="localhost"
                 virtual_host="/"
                 user="guest"
                 password="guest"
                 exchange="syslog"
                 routing_key="syslog.all"
                 template="RSYSLOG_ForwardFormat"
                 queue.type="linkedlist"
                 queue.timeoutenqueue="0"
                 queue.filename="rabbitmq"
                 queue.highwatermark="500000"
                 queue.lowwatermark="400000"
                 queue.discardmark="5000000"
                 queue.timeoutenqueue="0"
                 queue.maxdiskspace="5g"
                 queue.size="2000000"
                 queue.saveonshutdown="on"
                 action.resumeretrycount="-1")


