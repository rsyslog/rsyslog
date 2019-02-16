# omrabbitmq: RabbitMQ integration output module

| **Module Name:** | **omrabbitmq** |
|---------------|--------------|
| **Authors:**  | Jean-Philippe Hilaire <jean-philippe.hilaire@pmu.fr> & Philippe Duveau <philippe.duveau@free.fr> |

## Purpose
This module sends syslog messages into RabbitMQ server.
Only v6 configuration syntax is supported.

**omrabbitmq is tested and is running in production with 8.x version of rsyslog.**

## Compile
To successfully compile omrabbitmq module you need [rabbitmq-c](https://github.com/alanxz/rabbitmq-c) library version >= 0.4.

    ./configure --enable-omrabbitmq ...

## Configuration Parameters / Action Parameters

### host
| Parameter | Type | Mandatory | Value | Default 
|---|---|---|---|---
|host|string|yes|"hostname\[:port\]\[ hostname2\[:port2\]\]"|  

rabbitmq server(s). See HA configuration

### port
| Parameter | Type | Mandatory | Value | Default 
|---|---|---|---|---
|port|string|no|"port"|"5672"

### virtual\_host
| Parameter | Type | Mandatory | Value | Default 
|---|---|---|---|---
|virtual\_host|string|yes|"path"| 

virtual message broker

### user
| Parameter | Type | Mandatory | Value | Default 
|---|---|---|---|---
|user|string|yes|"user"| 

user name

### password
| Parameter | Type | Mandatory | Value | Default 
|---|---|---|---|---
|password|string|yes|"password"| 

user password

### exchange
| Type | Mandatory | Value | Default 
|---|---|---|---
|string|no|"name"|

exchange name

### routing\_key
| Type | Mandatory | Value | Default 
|---|---|---|---
|string|no|"name"| 

value of routing key

### routing\_key\_template
| Type | Mandatory | Value | Default 
|---|---|---|---
|string|no|"template"| 

template used to compute the routing key

### body\_template
| Type | Mandatory | Value | Default 
|---|---|---|---
|string|no|"template"|StdJSONFmt

template used to compute the message body. If the template is an empty string the sent message will be %rawmsg%

### delivery\_mode
| Type | Mandatory | Value | Default 
|---|---|---|---
|string|no|"TRANSIENT\|PERSISTANT"|"TRANSIENT"

persistance of the message in the broker

### expiration
| Type | Mandatory | Value | Default 
|---|---|---|---
|string|no|"milliseconds"| no expiration

ttl of the amqp message

### populate\_properties
| Type | Mandatory | Value | Default 
|---|---|---|---
|populate_properties|binary|no||off

fill timestamp, appid, msgid, hostname (cutsom header) with message informations

### content\_type
| Type | Mandatory | Value | Default 
|---|---|---|---
|string|no|"value"| 

content type as a MIME value

### recover\_policy
| Type | Mandatory | Value | Default 
|---|---|---|---
|string|no|"check\_interval;short\_failure_interval; short\_failure\_nb\_max;graceful\_interval"|"60;6;3;600"

See HA configuration

## HA configuration
The module can use two rabbitmq server in a fail-over mode. To configure this mode, the host parameter has to reference the two rabbitmq servers separated by space.
Each server can be optionaly completed with the port useful when they are differents.
One of the servers is choosen as a preferred one. The module connects to this server with a fail-over policy which can be defined through the action parameter "recover_policy".

The module launch a back-ground thread to monitor the to connection. As soon as the connection fails, the thread establish a connection to the back-up server to recover the service. When connected to backup server, the thread tries to reconnect the module to the preferred server using a "recover_policy".

The recover policy is based on 4 parameters :
- `check_interval` is the base duration between connection retries (default is 60 seconds)
- `short_failure_interval` is a duration under which two successive failures are considered as abnormal for the rabbitmq server (default is `check_interval/10`)
- `short_failure_nb_max` is the number of successive short failure are detected before to apply the graceful interval (default is 3)
- `graceful_interval` is a longer duration used if the rabbitmq server is unstable (default is `check_interval*10`).

## Samples
**Sample 1 :**

    module(load='omrabbitmq')

    action(type="omrabbitmq" 
           host="localhost"
           virtual_host="/"
           user="guest"
           password="guest"
           exchange="syslog"
           routing_key="syslog.all"
           template_body="RSYSLOG_ForwardFormat"
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
