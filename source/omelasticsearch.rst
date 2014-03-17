`back <rsyslog_conf_modules.html>`_

Elasticsearch Output Module
===========================

**Module Name:    omelasticsearch**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Available since:**\ 6.4.0+

**Description**:

This module provides native support for logging to
`Elasticsearch <http://www.elasticsearch.org/>`_.

**Action Parameters**:

-  **server**
    Host name or IP address of the Elasticsearch server. Defaults to
   "localhost"
-  **serverport**
    HTTP port to connect to Elasticsearch. Defaults to 9200
-  **searchIndex**
    `Elasticsearch
   index <http://www.elasticsearch.org/guide/appendix/glossary.html#index>`_
   to send your logs to. Defaults to "system"
-  **dynSearchIndex**\ <on/**off**>
    Whether the string provided for **searchIndex** should be taken as a
   `template <http://www.rsyslog.com/doc/rsyslog_conf_templates.html>`_.
   Defaults to "off", which means the index name will be taken
   literally. Otherwise, it will look for a template with that name, and
   the resulting string will be the index name. For example, let's
   assume you define a template named "date-days" containing
   "%timereported:1:10:date-rfc3339%". Then, with dynSearchIndex="on",
   if you say searchIndex="date-days", each log will be sent to and
   index named after the first 10 characters of the timestamp, like
   "2013-03-22".
-  **searchType**
    `Elasticsearch
   type <http://www.elasticsearch.org/guide/appendix/glossary.html#type>`_
   to send your index to. Defaults to "events"
-  **dynSearchType** <on/**off**>
    Like **dynSearchIndex**, it allows you to specify a
   `template <http://www.rsyslog.com/doc/rsyslog_conf_templates.html>`_
   for **searchType**, instead of a static string.
-  **asyncrepl**\ <on/**off**>
    By default, an indexing operation returns after all `replica
   shards <http://www.elasticsearch.org/guide/appendix/glossary.html#replica_shard>`_
   have indexed the document. With asyncrepl="on" it will return after
   it was indexed on the `primary
   shard <http://www.elasticsearch.org/guide/appendix/glossary.html#primary_shard>`_
   only - thus trading some consistency for speed.
-  **usehttps**\ <on/**off**>
    Send events over HTTPS instead of HTTP. Good for when you have
   Elasticsearch behind Apache or something else that can add HTTPS.
-  **timeout**
    How long Elasticsearch will wait for a primary shard to be available
   for indexing your log before sending back an error. Defaults to "1m".
-  **template**
    This is the JSON document that will be indexed in Elasticsearch. The
   resulting string needs to be a valid JSON, otherwise Elasticsearch
   will return an error. Defaults to:

::

    $template JSONDefault, "{\"message\":\"%msg:::json%\",\"fromhost\":\"%HOSTNAME:::json%\",\"facility\":\"%syslogfacility-text%\",\"priority\":\"%syslogpriority-text%\",\"timereported\":\"%timereported:::date-rfc3339%\",\"timegenerated\":\"%timegenerated:::date-rfc3339%\"}"

Which will produce this sort of documents (pretty-printed here for
readability):

::

    {
        "message": " this is a test message",
        "fromhost": "test-host",
        "facility": "user",
        "priority": "info",
        "timereported": "2013-03-12T18:05:01.344864+02:00",
        "timegenerated": "2013-03-12T18:05:01.344864+02:00"
    }

-  **bulkmode**\ <on/**off**>
    The default "off" setting means logs are shipped one by one. Each in
   its own HTTP request, using the `Index
   API <http://www.elasticsearch.org/guide/reference/api/index_.html>`_.
   Set it to "on" and it will use Elasticsearch's `Bulk
   API <http://www.elasticsearch.org/guide/reference/api/bulk.html>`_ to
   send multiple logs in the same request. The maximum number of logs
   sent in a single bulk request depends on your queue settings -
   usually limited by the `dequeue batch
   size <http://www.rsyslog.com/doc/node35.html>`_. More information
   about queues can be found
   `here <http://www.rsyslog.com/doc/node32.html>`_.
-  **parent**
    Specifying a string here will index your logs with that string the
   parent ID of those logs. Please note that you need to define the
   `parent
   field <http://www.elasticsearch.org/guide/reference/mapping/parent-field.html>`_
   in your
   `mapping <http://www.elasticsearch.org/guide/reference/mapping/>`_
   for that to work. By default, logs are indexed without a parent.
-  **dynParent**\ <on/**off**>
    Using the same parent for all the logs sent in the same action is
   quite unlikely. So you'd probably want to turn this "on" and specify
   a
   `template <http://www.rsyslog.com/doc/rsyslog_conf_templates.html>`_
   that will provide meaningful parent IDs for your logs.
-  **uid**
    If you have basic HTTP authentication deployed (eg: through the
   `elasticsearch-basic
   plugin <https://github.com/Asquera/elasticsearch-http-basic>`_), you
   can specify your user-name here.
-  **pwd**
    Password for basic authentication.

**Samples:**

The following sample does the following:

-  loads the omelasticsearch module
-  outputs all logs to Elasticsearch using the default settings

::

    module(load="omelasticsearch")
    *.*     action(type="omelasticsearch")

The following sample does the following:

-  loads the omelasticsearch module
-  defines a template that will make the JSON contain the following
   properties (more info about what properties you can use
   `here <http://www.rsyslog.com/doc/property_replacer.html>`_):

   -  RFC-3339 timestamp when the event was generated
   -  the message part of the event
   -  hostname of the system that generated the message
   -  severity of the event, as a string
   -  facility, as a string
   -  the tag of the event

-  outputs to Elasticsearch with the following settings

   -  host name of the server is myserver.local
   -  port is 9200
   -  JSON docs will look as defined in the template above
   -  index will be "test-index"
   -  type will be "test-type"
   -  activate bulk mode. For that to work effectively, we use an
      in-memory queue that can hold up to 5000 events. The maximum bulk
      size will be 300
   -  retry indefinitely if the HTTP request failed (eg: if the target
      server is down)

::

    module(load="omelasticsearch")
    template(name="testTemplate"
             type="list"
             option.json="on") {
               constant(value="{")
                 constant(value="\"timestamp\":\"")      property(name="timereported" dateFormat="rfc3339")
                 constant(value="\",\"message\":\"")     property(name="msg")
                 constant(value="\",\"host\":\"")        property(name="hostname")
                 constant(value="\",\"severity\":\"")    property(name="syslogseverity-text")
                 constant(value="\",\"facility\":\"")    property(name="syslogfacility-text")
                 constant(value="\",\"syslogtag\":\"")   property(name="syslogtag")
               constant(value="\"}")
             }
    *.* action(type="omelasticsearch"
               server="myserver.local"
               serverport="9200"
               template="testTemplate"
               searchIndex="test-index"
               searchType="test-type"
               bulkmode="on"
               queue.type="linkedlist"
               queue.size="5000"
               queue.dequeuebatchsize="300"
               action.resumeretrycount="-1")

 

::

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2008-2012 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the ASL 2.0.
