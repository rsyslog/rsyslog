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


.. _esVersion_major:

esVersion.major
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

ElasticSearch is notoriously bad at maintaining backwards compatibility. For this
reason, the setting can be used to configure the server's major version number (e.g. 7, 8, ...).
As far as we know breaking changes only happen with major version changes.
As of now, only value 8 triggers API changes. All other values select
pre-version-8 API usage.

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
to send your index to. Defaults to "events".
Setting this parameter to an empty string will cause the type to be omitted,
which is required since Elasticsearch 7.0. See
`Elasticsearch documentation <https://www.elastic.co/guide/en/elasticsearch/reference/current/removal-of-types.html>`_
for more information.


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
pipeline name to be included in the request. This allows pre processing
of events before indexing them. By default, events are not send to a pipeline.


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


.. _skipPipelineIfEmpty:

skipPipelineIfEmpty
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

When POST'ing a document, Elasticsearch does not allow an empty pipeline
parameter value. If boolean option skipPipelineIfEmpty is set to `"on"`, the
pipeline parameter won't be posted. Default is `"off"`.


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


.. _indextimeout:

indexTimeout
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "0", "no", "none"

.. versionadded:: 8.2204.0

Specifies the number of milliseconds to wait for a successful log indexing
request on a server_. By default there is no timeout.


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

    $template StdJSONFmt, "{\"message\":\"%msg:::json%\",\"fromhost\":\"%HOSTNAME:::json%\",\"facility\":\"%syslogfacility-text%\",\"priority\":\"%syslogpriority-text%\",\"timereported\":\"%timereported:::date-rfc3339%\",\"timegenerated\":\"%timegenerated:::date-rfc3339%\"}"

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

Another template, FullJSONFmt, is available that includes more fields including programname, PROCID (usually the process ID), and MSGID.

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
either the buffer reaches maxbytes or the `dequeue batch
size <http://www.rsyslog.com/doc/node35.html>`_ is reached. In order to
ensure Elasticsearch does not reject requests due to content length, verify
this value is set according to the `http.max_content_length
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
those lines). Such errors cannot be solved by simply resubmitting the record.
As such, they are written to the error file so that the user (script) can
examine them and act appropriately. Note that e.g. after search index
reconfiguration (e.g. dropping the mandatory attribute) a resubmit may
be successful.

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

.. _omelasticsearch-allowunsignedcerts:

allowunsignedcerts
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "off", "no", "none"

If `"on"`, this will set the curl `CURLOPT_SSL_VERIFYPEER` option to
`0`.  You are strongly discouraged to set this to `"on"`.  It is
primarily useful only for debugging or testing.

.. _omelasticsearch-skipverifyhost:

skipverifyhost
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "off", "no", "none"

If `"on"`, this will set the curl `CURLOPT_SSL_VERIFYHOST` option to
`0`.  You are strongly discouraged to set this to `"on"`.  It is
primarily useful only for debugging or testing.

.. _omelasticsearch-bulkid:

bulkid
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

This is the unique id to assign to the record.  The `bulk` part is misleading - this
can be used in both bulk mode :ref:`bulkmode` or in index
(record at a time) mode.  Although you can specify a static value for this
parameter, you will almost always want to specify a *template* for the value of
this parameter, and set `dynbulkid="on"` :ref:`omelasticsearch-dynbulkid`.  NOTE:
you must use `bulkid` and `dynbulkid` in order to use `writeoperation="create"`
:ref:`omelasticsearch-writeoperation`.

.. _omelasticsearch-dynbulkid:

dynbulkid
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

If this parameter is set to `"on"`, then the `bulkid` parameter :ref:`omelasticsearch-bulkid`
specifies a *template* to use to generate the unique id value to assign to the record.  If
using `bulkid` you will almost always want to set this parameter to `"on"` to assign
a different unique id value to each record.  NOTE:
you must use `bulkid` and `dynbulkid` in order to use `writeoperation="create"`
:ref:`omelasticsearch-writeoperation`.

.. _omelasticsearch-writeoperation:

writeoperation
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "index", "no", "none"

The value of this parameter is either `"index"` (the default) or `"create"`.  If `"create"` is
used, this means the bulk action/operation will be `create` - create a document only if the
document does not already exist.  The record must have a unique id in order to use `create`.
See :ref:`omelasticsearch-bulkid` and :ref:`omelasticsearch-dynbulkid`.  See
:ref:`omelasticsearch-writeoperation-example` for an example.

.. _omelasticsearch-retryfailures:

retryfailures
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

If this parameter is set to `"on"`, then the module will look for an
`"errors":true` in the bulk index response.  If found, each element in the
response will be parsed to look for errors, since a bulk request may have some
records which are successful and some which are failures.  Failed requests will
be converted back into records and resubmitted back to rsyslog for
reprocessing.  Each failed request will be resubmitted with a local variable
called `$.omes`.  This is a hash consisting of the fields from the metadata
header in the original request, and the fields from the response.  If the same
field is in the request and response, the value from the field in the *request*
will be used, to facilitate retries that want to send the exact same request,
and want to know exactly what was sent.
See below :ref:`omelasticsearch-retry-example` for an example of how retry
processing works.
*NOTE* The retried record will be resubmitted at the "top" of your processing
pipeline.  If your processing pipeline is not idempotent (that is, your
processing pipeline expects "raw" records), then you can specify a ruleset to
redirect retries to.  See :ref:`omelasticsearch-retryruleset` below.

`$.omes` fields:

* writeoperation - the operation used to submit the request - for rsyslog
  omelasticsearch this currently means either `"index"` or `"create"`
* status - the HTTP status code - typically an error will have a `4xx` or `5xx`
  code - of particular note is `429` - this means Elasticsearch was unable to
  process this bulk record request due to a temporary condition e.g. the bulk
  index thread pool queue is full, and rsyslog should retry the operation.
* _index, _type, _id, pipeline, _parent - the metadata associated with the
  request - not all of these fields will be present with every request - for
  example, if you do not use `"pipelinename"` or `"dynpipelinename"`, there
  will be no `$.omes!pipeline` field.
* error - a hash containing one or more, possibly nested, fields containing
  more detailed information about a failure.  Typically there will be fields
  `$.omes!error!type` (a keyword) and `$.omes!error!reason` (a longer string)
  with more detailed information about the rejection.  NOTE: The format is
  apparently not described in great detail, so code must not make any
  assumption about the availability of `error` or any specific sub-field.

There may be other fields too - the code just copies everything in the
response.  Here is an example of a detailed error response, in JSON format, from
Elasticsearch 5.6.9:

.. code-block:: json

    {"omes":
      {"writeoperation": "create",
       "_index": "rsyslog_testbench",
       "_type": "test-type",
       "_id": "92BE7AF79CD44305914C7658AF846A08",
       "status": 400,
       "error":
         {"type": "mapper_parsing_exception",
          "reason": "failed to parse [msgnum]",
          "caused_by":
            {"type": "number_format_exception",
             "reason": "For input string: \"x00000025\""}}}}

Reference: https://www.elastic.co/guide/en/elasticsearch/guide/current/bulk.html#bulk

.. _omelasticsearch-retryruleset:

retryruleset
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "", "no", "none"

If `retryfailures` is not `"on"` (:ref:`omelasticsearch-retryfailures`) then
this parameter has no effect.  This parameter specifies the name of a ruleset
to use to route retries.  This is useful if you do not want retried messages to
be processed starting from the top of your processing pipeline, or if you have
multiple outputs but do not want to send retried Elasticsearch failures to all
of your outputs, and you do not want to clutter your processing pipeline with a
lot of conditionals.  See below :ref:`omelasticsearch-retry-example` for an
example of how retry processing works.

.. _omelasticsearch-ratelimit.interval:

ratelimit.interval
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "600", "no", "none"

If `retryfailures` is not `"on"` (:ref:`omelasticsearch-retryfailures`) then
this parameter has no effect.  Specifies the interval in seconds onto which
rate-limiting is to be applied. If more than ratelimit.burst messages are read
during that interval, further messages up to the end of the interval are
discarded. The number of messages discarded is emitted at the end of the
interval (if there were any discards).
Setting this to value zero turns off ratelimiting.

.. _omelasticsearch-ratelimit.burst:

ratelimit.burst
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "20000", "no", "none"

If `retryfailures` is not `"on"` (:ref:`omelasticsearch-retryfailures`) then
this parameter has no effect.  Specifies the maximum number of messages that
can be emitted within the ratelimit.interval interval. For further information,
see description there.

.. _omelasticsearch-rebindinterval:

rebindinterval
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "-1", "no", "none"

This parameter tells omelasticsearch to close the connection and reconnect
to Elasticsearch after this many operations have been submitted.  The default
value `-1` means that omelasticsearch will not reconnect.  A value greater
than `-1` tells omelasticsearch, after this many operations have been
submitted to Elasticsearch, to drop the connection and establish a new
connection.  This is useful when rsyslog connects to multiple Elasticsearch
nodes through a router or load balancer, and you need to periodically drop
and reestablish connections to help the router balance the connections.  Use
the counter `rebinds` to monitor the number of times this has happened.

.. _omelasticsearch-statistic-counter:

Statistic Counter
=================

This plugin maintains global :doc:`statistics <../rsyslog_statistic_counter>`,
which accumulate all action instances. The statistic is named "omelasticsearch".
Parameters are:

-  **submitted** - number of messages submitted for processing (with both
   success and error result)

-  **fail.httprequests** - the number of times an HTTP request failed. Note
   that a single HTTP request may be used to submit multiple messages, so this
   number may be (much) lower than fail.http.

-  **fail.http** - number of message failures due to connection like-problems
   (things like remote server down, broken link etc)

-  **fail.es** - number of failures due to elasticsearch error reply; Note that
   this counter does NOT count the number of failed messages but the number of
   times a failure occurred (a potentially much smaller number). Counting messages
   would be quite performance-intense and is thus not done.

The following counters are available when `retryfailures="on"` is used:

-  **response.success** - number of records successfully sent in bulk index
   requests - counts the number of successful responses

-  **response.bad** - number of times omelasticsearch received a response in a
   bulk index response that was unrecognized or unable to be parsed.  This may
   indicate that omelasticsearch is attempting to communicate with a version of
   Elasticsearch that is incompatible, or is otherwise sending back data in the
   response that cannot be handled

-  **response.duplicate** - number of records in the bulk index request that
   were duplicates of already existing records - this will only be reported if
   using `writeoperation="create"` and `bulkid` to assign each record a unique
   ID

-  **response.badargument** - number of times omelasticsearch received a
   response that had a status indicating omelasticsearch sent bad data to
   Elasticsearch.  For example, status `400` and an error message indicating
   omelasticsearch attempted to store a non-numeric string value in a numeric
   field.

-  **response.bulkrejection** - number of times omelasticsearch received a
   response that had a status indicating Elasticsearch was unable to process
   the record at this time - status `429`.  The record can be retried.

-  **response.other** - number of times omelasticsearch received a
   response not recognized as one of the above responses, typically some other
   `4xx` or `5xx` HTTP status.

-  **rebinds** - if using `rebindinterval` this will be the number of
   times omelasticsearch has reconnected to Elasticsearch

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

How Retries Are Handled
=======================

When using `retryfailures="on"` (:ref:`omelasticsearch-retryfailures`), the
original `Message` object (that is, the original `smsg_t *msg` object) **is not
available**.  This means none of the metadata associated with that object, such
as various timestamps, hosts/ip addresses, etc. are not available for the retry
operation.  The only thing available are the metadata header (_index, _type,
_id, pipeline, _parent) and original JSON string sent in the original request,
and whatever data is returned in the error response.  All of these are made
available in the `$.omes` fields.  If the same field name exists in the request
metadata and the response, the field from the request will be used, in order to
facilitate retrying the exact same request.  For the message to retry, the code
will take the original JSON string and parse it back into an internal `Message`
object.  This means you **may need to use a different template** to output
messages for your retry ruleset.  For example, if you used the following
template to format the Elasticsearch message for the initial submission:

.. code-block:: none

    template(name="es_output_template"
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

You would have to use a different template for the retry, since none of the
`timereported`, `msg`, etc. fields will have the same values for the retry as
for the initial try.

Same with the other omelasticsearch parameters which can be constructed with
templates, such as `"dynpipelinename"`, `"dynsearchindex"`, `"dynsearchtype"`,
`"dynparent"`, and `"dynbulkid"`.  For example, if you generate the `_id` to
use in the request, you will want to reuse the same `_id` for each subsequent
retry:

.. code-block:: none

    template(name="id-template" type="string" string="%$.es_msg_id%")
    if strlen($.omes!_id) > 0 then {
        set $.es_msg_id = $.omes!_id;
    } else {
        # NOTE: depends on rsyslog being compiled with --enable-uuid
        set $.es_msg_id = $uuid;
    }
    action(type="omelasticsearch" bulkid="id-template" ...)

That is, if this is a retry, `$.omes!_id` will be set, so use that value for
the bulk id for this record, otherwise, generate a new one with `$uuid`.  Note
that the template uses the temporary variable `$.es_msg_id` which must be set
each time, to either `$.omes!_id` or `$uuid`.

The `rawmsg` field is a special case.  If the original request had a field
called `message`, then when constructing the new message from the original to
retry, the `rawmsg` message property will be set to the value of the `message`
field.  Otherwise, the `rawmsg` property value will be set to the entire
original request - the data part, not the metadata.  In previous versions,
without the `message` field, the `rawmsg` property was set to the value of the
data plus the Elasticsearch metadata, which caused problems with retries.  See
`rsyslog issue 3573 <https://github.com/rsyslog/rsyslog/issues/3573>`_

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
-  outputs all logs to Elasticsearch using the full JSON logging template including program name

.. code-block:: none

    module(load="omelasticsearch")
    *.*     action(type="omelasticsearch" template="FullJSONFmt")


Example 3
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


.. _omelasticsearch-writeoperation-example:

Example 4
---------

The following sample shows how to use :ref:`omelasticsearch-writeoperation`
with :ref:`omelasticsearch-dynbulkid` and :ref:`omelasticsearch-bulkid`.  For
simplicity, it assumes rsyslog has been built with `--enable-libuuid` which
provides the `uuid` property for each record:

.. code-block:: none

    module(load="omelasticsearch")
    set $!es_record_id = $uuid;
    template(name="bulkid-template" type="list") { property(name="$!es_record_id") }
    action(type="omelasticsearch"
           ...
           bulkmode="on"
           bulkid="bulkid-template"
           dynbulkid="on"
           writeoperation="create")


.. _omelasticsearch-retry-example:

Example 5
---------

The following sample shows how to use :ref:`omelasticsearch-retryfailures` to
process, discard, or retry failed operations.  This uses
`writeoperation="create"` with a unique `bulkid` so that we can check for and
discard duplicate messages as successful.  The `try_es` ruleset is used both
for the initial attempt and any subsequent retries.  The code in the ruleset
assumes that if `$.omes!status` is set and is non-zero, this is a retry for a
previously failed operation.  If the status was successful, or Elasticsearch
said this was a duplicate, the record is already in Elasticsearch, so we can
drop the record.  If there was some error processing the response
e.g. Elasticsearch sent a response formatted in some way that we did not know
how to process, then submit the record to the `error_es` ruleset.  If the
response was a "hard" error like `400`, then submit the record to the
`error_es` ruleset.  In any other case, such as a status `429` or `5xx`, the
record will be resubmitted to Elasticsearch. In the example, the `error_es`
ruleset just dumps the records to a file.

.. code-block:: none

    module(load="omelasticsearch")
    module(load="omfile")
    template(name="bulkid-template" type="list") { property(name="$.es_record_id") }

    ruleset(name="error_es") {
	    action(type="omfile" template="RSYSLOG_DebugFormat" file="es-bulk-errors.log")
    }

    ruleset(name="try_es") {
        if strlen($.omes!status) > 0 then {
            # retry case
            if ($.omes!status == 200) or ($.omes!status == 201) or (($.omes!status == 409) and ($.omes!writeoperation == "create")) then {
                stop # successful
            }
            if ($.omes!writeoperation == "unknown") or (strlen($.omes!error!type) == 0) or (strlen($.omes!error!reason) == 0) then {
                call error_es
                stop
            }
            if ($.omes!status == 400) or ($.omes!status < 200) then {
                call error_es
                stop
            }
            # else fall through to retry operation
        }
        if strlen($.omes!_id) > 0 then {
            set $.es_record_id = $.omes!_id;
        } else {
            # NOTE: depends on rsyslog being compiled with --enable-uuid
            set $.es_record_id = $uuid;
        }
        action(type="omelasticsearch"
                  ...
                  bulkmode="on"
                  bulkid="bulkid-template"
                  dynbulkid="on"
                  writeoperation="create"
                  retryfailures="on"
                  retryruleset="try_es")
    }
    call try_es
