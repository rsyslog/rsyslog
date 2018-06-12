********************************************
omelasticsearch: Elasticsearch Output Module
********************************************

===========================  ===========================================================================
**Module Name:**             **omelasticsearch**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

This module provides native support for logging to
`Elasticsearch <http://www.elasticsearch.org/>`_.


Notable Features
================

- :ref:`omelasticsearch-statistic-counter`


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Action Parameters
-----------------

Server
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "none", "no", "none"

An array of Elasticsearch servers in the specified format. If no scheme
is specified, it will be chosen according to usehttps_. If no port is
specified, serverport_ will be used. Defaults to "localhost".

Requests to Elasticsearch will be load-balanced between all servers in
round-robin fashion.

.. code-block:: none

  Examples:
       server="localhost:9200"
       server=["elasticsearch1", "elasticsearch2"]


.. _serverport:

Serverport
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "9200", "no", "none"

Default HTTP port to use to connect to Elasticsearch if none is specified
on a server_. Defaults to 9200


.. _healthchecktimeout:

HealthCheckTimeout
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "3500", "no", "none"

Specifies the number of milliseconds to wait for a successful health check
on a server_. Before trying to submit events to Elasticsearch, rsyslog will
execute an *HTTP HEAD* to ``/_cat/health`` and expect an *HTTP OK* within
this timeframe. Defaults to 3500.

*Note, the health check is verifying connectivity only, not the state of
the Elasticsearch cluster.*


.. _searchIndex:

searchIndex
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

`Elasticsearch
index <http://www.elasticsearch.org/guide/appendix/glossary.html#index>`_
to send your logs to. Defaults to "system"


.. _dynSearchIndex:

dynSearchIndex
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

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

searchType
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

`Elasticsearch
type <http://www.elasticsearch.org/guide/appendix/glossary.html#type>`_
to send your index to. Defaults to "events"


.. _dynSearchType:

dynSearchType
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Like dynSearchIndex_, it allows you to specify a
`rsyslog template <http://www.rsyslog.com/doc/rsyslog_conf_templates.html>`_
for searchType_, instead of a static string.


.. _pipelineName:

pipelineName
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The `ingest node <https://www.elastic.co/guide/en/elasticsearch/reference/current/ingest.html>`_
pipeline name to be inclued in the request. This allows pre processing
of events bevor indexing them. By default, events are not send to a pipeline.


.. _dynPipelineName:

dynPipelineName
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Like dynSearchIndex_, it allows you to specify a
`rsyslog template <http://www.rsyslog.com/doc/rsyslog_conf_templates.html>`_
for pipelineName_, instead of a static string.


.. _asyncrepl:

asyncrepl
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

No longer supported as ElasticSearch no longer supports it.


.. _usehttps:

usehttps
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Default scheme to use when sending events to Elasticsearch if none is
specified on a  server_. Good for when you have
Elasticsearch behind Apache or something else that can add HTTPS.
Note that if you have a self-signed certificate, you'd need to install
it first. This is done by copying the certificate to a trusted path
and then running *update-ca-certificates*. That trusted path is
typically */usr/local/share/ca-certificates* but check the man page of
*update-ca-certificates* for the default path of your distro


.. _timeout:

timeout
^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "1m", "no", "none"

How long Elasticsearch will wait for a primary shard to be available
for indexing your log before sending back an error. Defaults to "1m".


.. _template:

template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "see below", "no", "none"

This is the JSON document that will be indexed in Elasticsearch. The
resulting string needs to be a valid JSON, otherwise Elasticsearch
will return an error. Defaults to:

.. code-block:: none

    $template JSONDefault, "{\"message\":\"%msg:::json%\",\"fromhost\":\"%HOSTNAME:::json%\",\"facility\":\"%syslogfacility-text%\",\"priority\":\"%syslogpriority-text%\",\"timereported\":\"%timereported:::date-rfc3339%\",\"timegenerated\":\"%timegenerated:::date-rfc3339%\"}"

Which will produce this sort of documents (pretty-printed here for
readability):

.. code-block:: none

    {
        "message": " this is a test message",
        "fromhost": "test-host",
        "facility": "user",
        "priority": "info",
        "timereported": "2013-03-12T18:05:01.344864+02:00",
        "timegenerated": "2013-03-12T18:05:01.344864+02:00"
    }


.. _bulkmode:

bulkmode
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

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

maxbytes
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "100m", "no", "none"

.. versionadded:: 8.23.0

When shipping logs with bulkmode_ **on**, maxbytes specifies the maximum
size of the request body sent to Elasticsearch. Logs are batched until
either the buffer reaches maxbytes or the the `dequeue batch
size <http://www.rsyslog.com/doc/node35.html>`_ is reached. In order to
ensure Elasticsearch does not reject requests due to content length, verify
this value is set accoring to the `http.max_content_length
<https://www.elastic.co/guide/en/elasticsearch/reference/current/modules-http.html>`_
setting in Elasticsearch. Defaults to 100m.


.. _parent:

parent
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Specifying a string here will index your logs with that string the
parent ID of those logs. Please note that you need to define the
`parent
field <http://www.elasticsearch.org/guide/reference/mapping/parent-field.html>`_
in your
`mapping <http://www.elasticsearch.org/guide/reference/mapping/>`_
for that to work. By default, logs are indexed without a parent.


.. _dynParent:

dynParent
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Using the same parent for all the logs sent in the same action is
quite unlikely. So you'd probably want to turn this "on" and specify
a
`rsyslog template <http://www.rsyslog.com/doc/rsyslog_conf_templates.html>`_
that will provide meaningful parent IDs for your logs.


.. _uid:

uid
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

If you have basic HTTP authentication deployed (eg through the
`elasticsearch-basic
plugin <https://github.com/Asquera/elasticsearch-http-basic>`_), you
can specify your user-name here.


.. _pwd:

pwd
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Password for basic authentication.


.. _errorfile:

errorFile
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

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

.. _omelasticsearch-tls.cacert:

tls.cacert
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

This is the full path and file name of the file containing the CA cert for the
CA that issued the Elasticsearch server cert.  This file is in PEM format.  For
example: `/etc/rsyslog.d/es-ca.crt`

.. _tls.mycert:

tls.mycert
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

This is the full path and file name of the file containing the client cert for
doing client cert auth against Elasticsearch.  This file is in PEM format.  For
example: `/etc/rsyslog.d/es-client-cert.pem`

.. _tls.myprivkey:

tls.myprivkey
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

This is the full path and file name of the file containing the private key
corresponding to the cert `tls.mycert` used for doing client cert auth against
Elasticsearch.  This file is in PEM format, and must be unencrypted, so take
care to secure it properly.  For example: `/etc/rsyslog.d/es-client-key.pem`

.. _omelasticsearch-statistic-counter:

Statistic Counter
=================

This plugin maintains global :doc:`statistics <../rsyslog_statistic_counter>`,
which accumulate all action instances. The statistic is named "omelasticsearch".
Parameters are:

-  **submitted** - number of messages submitted for processing (with both
   success and error result)

-  **fail.httprequests** - the number of times a http request failed. Note
   that a single http request may be used to submit multiple messages, so this
   number may be (much) lower than fail.http.

-  **fail.http** - number of message failures due to connection like-problems
   (things like remote server down, broken link etc)

-  **fail.es** - number of failures due to elasticsearch error reply; Note that
   this counter does NOT count the number of failed messages but the number of
   times a failure occured (a potentially much smaller number). Counting messages
   would be quite performance-intense and is thus not done.

**The fail.httprequests and fail.http counters reflect only failures that
omelasticsearch detected.** Once it detects problems, it (usually, depends on
circumstances) tell the rsyslog core that it wants to be suspended until the
situation clears (this is a requirement for rsyslog output modules). Once it is
suspended, it does NOT receive any further messages. Depending on the user
configuration, messages will be lost during this period. Those lost messages will
NOT be counted by impstats (as it does not see them).

Note that some previous (pre 7.4.5) versions of this plugin had different counters.
These were experimental and confusing. The only ones really used were "submits",
which were the number of successfully processed messages and "connfail" which were
equivalent to "failed.http".


Examples
========

Example 1
---------

The following sample does the following:

-  loads the omelasticsearch module
-  outputs all logs to Elasticsearch using the default settings

.. code-block:: none

    module(load="omelasticsearch")
    *.*     action(type="omelasticsearch")


Example 2
---------

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

.. code-block:: none

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


