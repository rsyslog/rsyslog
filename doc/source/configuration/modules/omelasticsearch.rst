.. _ref-omelasticsearch:

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


Target platform detection
=========================

Starting with release 8.2510.0 the module probes the configured servers during
configuration processing to determine whether they are running Elasticsearch or
OpenSearch and to capture the version number that is exposed by the cluster.
The probe happens once at startup, before the action begins to process any
messages.  When the detection succeeds the module will automatically adapt
internal defaults (for example, legacy clusters continue to receive the
``system`` index) and it will override ``esVersion.major`` with the detected
major version.  The configured action keeps running even if the probe cannot
reach the servers; in that case rsyslog falls back to the provided
configuration values.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive; CamelCase is recommended for readability.

.. note::

   This module supports action parameters, only.

Action Parameters
-----------------
.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omelasticsearch-server`
     - .. include:: ../../reference/parameters/omelasticsearch-server.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-serverport`
     - .. include:: ../../reference/parameters/omelasticsearch-serverport.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-healthchecktimeout`
     - .. include:: ../../reference/parameters/omelasticsearch-healthchecktimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-esversion-major`
     - .. include:: ../../reference/parameters/omelasticsearch-esversion-major.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-searchindex`
     - .. include:: ../../reference/parameters/omelasticsearch-searchindex.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-dynsearchindex`
     - .. include:: ../../reference/parameters/omelasticsearch-dynsearchindex.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-searchtype`
     - .. include:: ../../reference/parameters/omelasticsearch-searchtype.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-dynsearchtype`
     - .. include:: ../../reference/parameters/omelasticsearch-dynsearchtype.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-pipelinename`
     - .. include:: ../../reference/parameters/omelasticsearch-pipelinename.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-dynpipelinename`
     - .. include:: ../../reference/parameters/omelasticsearch-dynpipelinename.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-skippipelineifempty`
     - .. include:: ../../reference/parameters/omelasticsearch-skippipelineifempty.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-asyncrepl`
     - .. include:: ../../reference/parameters/omelasticsearch-asyncrepl.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-usehttps`
     - .. include:: ../../reference/parameters/omelasticsearch-usehttps.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-timeout`
     - .. include:: ../../reference/parameters/omelasticsearch-timeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-indextimeout`
     - .. include:: ../../reference/parameters/omelasticsearch-indextimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-template`
     - .. include:: ../../reference/parameters/omelasticsearch-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-bulkmode`
     - .. include:: ../../reference/parameters/omelasticsearch-bulkmode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-maxbytes`
     - .. include:: ../../reference/parameters/omelasticsearch-maxbytes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-parent`
     - .. include:: ../../reference/parameters/omelasticsearch-parent.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-dynparent`
     - .. include:: ../../reference/parameters/omelasticsearch-dynparent.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-uid`
     - .. include:: ../../reference/parameters/omelasticsearch-uid.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-pwd`
     - .. include:: ../../reference/parameters/omelasticsearch-pwd.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-apikey`
     - .. include:: ../../reference/parameters/omelasticsearch-apikey.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-errorfile`
     - .. include:: ../../reference/parameters/omelasticsearch-errorfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-tls-cacert`
     - .. include:: ../../reference/parameters/omelasticsearch-tls-cacert.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-tls-mycert`
     - .. include:: ../../reference/parameters/omelasticsearch-tls-mycert.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-tls-myprivkey`
     - .. include:: ../../reference/parameters/omelasticsearch-tls-myprivkey.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-allowunsignedcerts`
     - .. include:: ../../reference/parameters/omelasticsearch-allowunsignedcerts.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-skipverifyhost`
     - .. include:: ../../reference/parameters/omelasticsearch-skipverifyhost.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-bulkid`
     - .. include:: ../../reference/parameters/omelasticsearch-bulkid.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-dynbulkid`
     - .. include:: ../../reference/parameters/omelasticsearch-dynbulkid.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-writeoperation`
     - .. include:: ../../reference/parameters/omelasticsearch-writeoperation.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-retryfailures`
     - .. include:: ../../reference/parameters/omelasticsearch-retryfailures.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-retryruleset`
     - .. include:: ../../reference/parameters/omelasticsearch-retryruleset.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-ratelimit-interval`
     - .. include:: ../../reference/parameters/omelasticsearch-ratelimit-interval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-ratelimit-burst`
     - .. include:: ../../reference/parameters/omelasticsearch-ratelimit-burst.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omelasticsearch-rebindinterval`
     - .. include:: ../../reference/parameters/omelasticsearch-rebindinterval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


.. toctree::
   :hidden:

   ../../reference/parameters/omelasticsearch-server
   ../../reference/parameters/omelasticsearch-serverport
   ../../reference/parameters/omelasticsearch-healthchecktimeout
   ../../reference/parameters/omelasticsearch-esversion-major
   ../../reference/parameters/omelasticsearch-searchindex
   ../../reference/parameters/omelasticsearch-dynsearchindex
   ../../reference/parameters/omelasticsearch-searchtype
   ../../reference/parameters/omelasticsearch-dynsearchtype
   ../../reference/parameters/omelasticsearch-pipelinename
   ../../reference/parameters/omelasticsearch-dynpipelinename
   ../../reference/parameters/omelasticsearch-skippipelineifempty
   ../../reference/parameters/omelasticsearch-asyncrepl
   ../../reference/parameters/omelasticsearch-usehttps
   ../../reference/parameters/omelasticsearch-timeout
   ../../reference/parameters/omelasticsearch-indextimeout
   ../../reference/parameters/omelasticsearch-template
   ../../reference/parameters/omelasticsearch-bulkmode
   ../../reference/parameters/omelasticsearch-maxbytes
   ../../reference/parameters/omelasticsearch-parent
   ../../reference/parameters/omelasticsearch-dynparent
   ../../reference/parameters/omelasticsearch-uid
   ../../reference/parameters/omelasticsearch-pwd
   ../../reference/parameters/omelasticsearch-apikey
   ../../reference/parameters/omelasticsearch-errorfile
   ../../reference/parameters/omelasticsearch-tls-cacert
   ../../reference/parameters/omelasticsearch-tls-mycert
   ../../reference/parameters/omelasticsearch-tls-myprivkey
   ../../reference/parameters/omelasticsearch-allowunsignedcerts
   ../../reference/parameters/omelasticsearch-skipverifyhost
   ../../reference/parameters/omelasticsearch-bulkid
   ../../reference/parameters/omelasticsearch-dynbulkid
   ../../reference/parameters/omelasticsearch-writeoperation
   ../../reference/parameters/omelasticsearch-retryfailures
   ../../reference/parameters/omelasticsearch-retryruleset
   ../../reference/parameters/omelasticsearch-ratelimit-interval
   ../../reference/parameters/omelasticsearch-ratelimit-burst
   ../../reference/parameters/omelasticsearch-rebindinterval

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

When using `retryfailures="on"` (:ref:`param-omelasticsearch-retryfailures`), the
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
    action(type="omelasticsearch")


Example 2
---------

The following sample does the following:

-  loads the omelasticsearch module
-  outputs all logs to Elasticsearch using the full JSON logging template including program name

.. code-block:: none

    module(load="omelasticsearch")
    action(type="omelasticsearch" template="FullJSONFmt")


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

The following sample shows how to use :ref:`param-omelasticsearch-writeoperation`
with :ref:`param-omelasticsearch-dynbulkid` and :ref:`param-omelasticsearch-bulkid`.  For
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

The following sample shows how to use :ref:`param-omelasticsearch-retryfailures` to
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
