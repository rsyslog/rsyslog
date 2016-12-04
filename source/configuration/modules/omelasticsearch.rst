omelasticsearch: Elasticsearch Output Module
============================================

**Module Name:    omelasticsearch**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Available since:**\ 6.4.0+

**Description**:

This module provides native support for logging to
`Elasticsearch <http://www.elasticsearch.org/>`_.

**Action Parameters**:

.. _server:

-  **server** [ ``http://`` | ``https://`` |  ] <*hostname* | *ip*> [ ``:`` <*port*> ]
   An array of Elasticsearch servers in the specified format. If no scheme is specified, 
   it will be chosen according to usehttps_. If no port is specified, 
   serverport_ will be used. Defaults to "localhost". 

   Requests to Elasticsearch will be load-balanced between all servers in round-robin fashion.

::
  
  Examples:
       server="localhost:9200"
       server=["elasticsearch1", "elasticsearch2"]

.. _serverport:

-  **serverport**
   Default HTTP port to use to connect to Elasticsearch if none is specified 
   on a server_. Defaults to 9200

.. _healthchecktimeout:

-  **healthchecktimeout**
   Specifies the number of milliseconds to wait for a successful health check on a server_. 
   Before trying to submit events to Elasticsearch, rsyslog will execute an *HTTP HEAD* to 
   ``/_cat/health`` and expect an *HTTP OK* within this timeframe. Defaults to 3500.

   *Note, the health check is verifying connectivity only, not the state of the Elasticsearch cluster.*

.. _searchIndex:

-  **searchIndex**
   `Elasticsearch
   index <http://www.elasticsearch.org/guide/appendix/glossary.html#index>`_
   to send your logs to. Defaults to "system"

.. _dynSearchIndex:

-  **dynSearchIndex**\ <on/**off**>
   Whether the string provided for searchIndex_ should be taken as a
   `rsyslog template <http://www.rsyslog.com/doc/rsyslog_conf_templates.html>`_.
   Defaults to "off", which means the index name will be taken
   literally. Otherwise, it will look for a template with that name, and
   the resulting string will be the index name. For example, let's
   assume you define a template named "date-days" containing
   "%timereported:1:10:date-rfc3339%". Then, with dynSearchIndex="on",
   if you say searchIndex="date-days", each log will be sent to and
   index named after the first 10 characters of the timestamp, like
   "2013-03-22".

.. _searchType:

-  **searchType**
   `Elasticsearch
   type <http://www.elasticsearch.org/guide/appendix/glossary.html#type>`_
   to send your index to. Defaults to "events"

.. _dynSearchType:

-  **dynSearchType** <on/**off**>
   Like dynSearchIndex_, it allows you to specify a
   `rsyslog template <http://www.rsyslog.com/doc/rsyslog_conf_templates.html>`_
   for searchType_, instead of a static string.

.. _asyncrepl:

-  **asyncrepl**\ <on/**off**>
   No longer supported as ElasticSearch no longer supports it.

.. _usehttps:

-  **usehttps**\ <on/**off**>
   Default scheme to use when sending events to Elasticsearch if none is
   specified on a  server_. Good for when you have
   Elasticsearch behind Apache or something else that can add HTTPS.
   Note that if you have a self-signed certificate, you'd need to install
   it first. This is done by copying the certificate to a trusted path
   and then running *update-ca-certificates*. That trusted path is
   typically */usr/local/share/ca-certificates* but check the man page of
   *update-ca-certificates* for the default path of your distro

.. _timeout:

-  **timeout**
   How long Elasticsearch will wait for a primary shard to be available
   for indexing your log before sending back an error. Defaults to "1m".

.. _template:

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

.. _bulkmode:

-  **bulkmode**\ <on/**off**>
   The default "off" setting means logs are shipped one by one. Each in
   its own HTTP request, using the `Index
   API <http://www.elasticsearch.org/guide/reference/api/index_.html>`_.
   Set it to "on" and it will use Elasticsearch's `Bulk
   API <http://www.elasticsearch.org/guide/reference/api/bulk.html>`_ to
   send multiple logs in the same request. The maximum number of logs
   sent in a single bulk request depends on your maxbytes_  
   and queue settings -
   usually limited by the `dequeue batch
   size <http://www.rsyslog.com/doc/node35.html>`_. More information
   about queues can be found
   `here <http://www.rsyslog.com/doc/node32.html>`_.

.. _maxbytes:

-  **maxbytes** *(since v8.23.0)*
   When shipping logs with bulkmode_ **on**, maxbytes specifies the maximum
   size of the request body sent to Elasticsearch. Logs are batched until 
   either the buffer reaches maxbytes or the the `dequeue batch
   size <http://www.rsyslog.com/doc/node35.html>`_ is reached. In order to
   ensure Elasticsearch does not reject requests due to content length, verify
   this value is set accoring to the `http.max_content_length 
   <https://www.elastic.co/guide/en/elasticsearch/reference/current/modules-http.html>`_
   setting in Elasticsearch. Defaults to 100m. 

.. _parent:

-  **parent**
   Specifying a string here will index your logs with that string the
   parent ID of those logs. Please note that you need to define the
   `parent
   field <http://www.elasticsearch.org/guide/reference/mapping/parent-field.html>`_
   in your
   `mapping <http://www.elasticsearch.org/guide/reference/mapping/>`_
   for that to work. By default, logs are indexed without a parent.

.. _dynParent:

-  **dynParent**\ <on/**off**>
   Using the same parent for all the logs sent in the same action is
   quite unlikely. So you'd probably want to turn this "on" and specify
   a
   `rsyslog template <http://www.rsyslog.com/doc/rsyslog_conf_templates.html>`_
   that will provide meaningful parent IDs for your logs.

.. _uid:

-  **uid**
   If you have basic HTTP authentication deployed (eg through the
   `elasticsearch-basic
   plugin <https://github.com/Asquera/elasticsearch-http-basic>`_), you
   can specify your user-name here.

.. _pwd:

-  **pwd**
   Password for basic authentication.

.. _errorfile:

- **errorfile** <filename> (optional)

  If specified, records failed in bulk mode are written to this file, including
  their error cause. Rsyslog itself does not process the file any more, but the
  idea behind that mechanism is that the user can create a script to periodically
  inspect the error file and react appropriately. As the complete request is
  included, it is possible to simply resubmit messages from that script.

  *Please note:* when rsyslog has problems connecting to elasticsearch, a general
  error is assumed and the submit is retried. However, if we receive negative
  responses during batch processing, we assume an error in the data itself
  (like a mandatory field is not filled in, a format error or something along
  those lines). Such errors cannot be solved by simpy resubmitting the record.
  As such, they are written to the error file so that the user (script) can
  examine them and act appropriately. Note that e.g. after search index
  reconfiguration (e.g. dropping the mandatory attribute) a resubmit may
  be succesful.

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
   properties

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
    action(type="omelasticsearch"
           server="myserver.local"
           serverport="9200"
           template="testTemplate"
           searchIndex="test-index"
           searchType="test-type"
           bulkmode="on"
           maxbytes="100m"
           queue.type="linkedlist"
           queue.size="5000"
           queue.dequeuebatchsize="300"
           action.resumeretrycount="-1")


This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2008-2016 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the ASL 2.0.
