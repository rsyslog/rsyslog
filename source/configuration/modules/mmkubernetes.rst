*****************************************
Kubernetes Metadata Module (mmkubernetes)
*****************************************

===========================  ===========================================================================
**Module Name:**             **mmkubernetes**
**Author:**                  `Tomáš Heinrich`
                             `Rich Megginson` <rmeggins@redhat.com>
===========================  ===========================================================================

Purpose
=======

This module is used to add `Kubernetes <https://kubernetes.io/>`
metadata to log messages logged by containers running in Kubernetes.
It will add the namespace uuid, pod uuid, pod and namespace labels and
annotations, and other metadata associated with the pod and
namespace.

.. note::

   This **only** works with log files in `/var/log/containers/*.log` (docker
   `--log-driver=json-file`, or CRI-O log files), or with journald entries with
   message properties `CONTAINER_NAME` and `CONTAINER_ID_FULL` (docker
   `--log-driver=journald`), and when the application running inside the
   container writes logs to `stdout`/`stderr`.  This **does not** currently
   work with other log drivers.

For json-file and CRI-O logs, you must use the `imfile` module with the
`addmetadata="on"` parameter, and the filename must match the
liblognorm rules specified by the `filenamerules`
(:ref:`filenamerules`) or `filenamerulebase` (:ref:`filenamerulebase`)
parameter values.

For journald logs, there must be a message property `CONTAINER_NAME`
which matches the liblognorm rules specified by the `containerrules`
(:ref:`containerrules`) or `containerrulebase`
(:ref:`containerrulebase`) parameter values. The record must also have
the message property `CONTAINER_ID_FULL`.

This module is implemented via the output module interface. This means
that mmkubernetes should be called just like an action. After it has
been called, there will be two new message properties: `kubernetes`
and `docker`.  There will be subfields of each one for the various
metadata items: `$!kubernetes!namespace_name`
`$!kubernetes!labels!this-is-my-label`, etc.  There is currently only
1 docker subfield: `$!docker!container_id`.  See
https://github.com/ViaQ/elasticsearch-templates/blob/master/namespaces/kubernetes.yml
and
https://github.com/ViaQ/elasticsearch-templates/blob/master/namespaces/docker.yml
for more details.

Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.

Module Parameters and Action Parameters
---------------------------------------

.. _kubernetesurl:

KubernetesURL
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "https://kubernetes.default.svc.cluster.local:443", "no", "none"

The URL of the Kubernetes API server.  Example: `https://localhost:8443`.

.. _mmkubernetes-tls.cacert:

tls.cacert
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Full path and file name of file containing the CA cert of the
Kubernetes API server cert issuer.  Example: `/etc/rsyslog.d/mmk8s-ca.crt`.
This parameter is not mandatory if using an `http` scheme instead of `https` in
`kubernetesurl`, or if using `allowunsignedcerts="yes"`.

.. _mmkubernetes-tls.mycert:

tls.mycert
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

This is the full path and file name of the file containing the client cert for
doing client cert auth against Kubernetes.  This file is in PEM format.  For
example: `/etc/rsyslog.d/k8s-client-cert.pem`

.. _mmkubernetes-tls.myprivkey:

tls.myprivkey
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

This is the full path and file name of the file containing the private key
corresponding to the cert `tls.mycert` used for doing client cert auth against
Kubernetes.  This file is in PEM format, and must be unencrypted, so take
care to secure it properly.  For example: `/etc/rsyslog.d/k8s-client-key.pem`

.. _tokenfile:

tokenfile
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The file containing the token to use to authenticate to the Kubernetes API
server.  One of `tokenfile` or `token` is required if Kubernetes is configured
with access control.  Example: `/etc/rsyslog.d/mmk8s.token`

.. _token:

token
^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The token to use to authenticate to the Kubernetes API server.  One of `token`
or `tokenfile` is required if Kubernetes is configured with access control.
Example: `UxMU46ptoEWOSqLNa1bFmH`

.. _annotation_match:

annotation_match
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "none", "no", "none"

By default no pod or namespace annotations will be added to the
messages.  This parameter is an array of patterns to match the keys of
the `annotations` field in the pod and namespace metadata to include
in the `$!kubernetes!annotations` (for pod annotations) or the
`$!kubernetes!namespace_annotations` (for namespace annotations)
message properties.  Example: `["k8s.*master","k8s.*node"]`

.. _srcmetadatapath:

srcmetadatapath
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "$!metadata!filename", "no", "none"

When reading json-file logs, with `imfile` and `addmetadata="on"`,
this is the property where the filename is stored.

.. _dstmetadatapath:

dstmetadatapath
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "$!", "no", "none"

This is the where the `kubernetes` and `docker` properties will be
written.  By default, the module will add `$!kubernetes` and
`$!docker`.

.. _allowunsignedcerts:

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

.. _skipverifyhost:

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

.. _de_dot:

de_dot
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "on", "no", "none"

When processing labels and annotations, if this parameter is set to
`"on"`, the key strings will have their `.` characters replaced with
the string specified by the `de_dot_separator` parameter.

.. _de_dot_separator:

de_dot_separator
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "_", "no", "none"

When processing labels and annotations, if the `de_dot` parameter is
set to `"on"`, the key strings will have their `.` characters replaced
with the string specified by the string value of this parameter.

.. _filenamerules:

filenamerules
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "SEE BELOW", "no", "none"

.. note::

    This directive is not supported with liblognorm 2.0.2 and earlier.

When processing json-file logs, these are the lognorm rules to use to
match the filename and extract metadata.  The default value is::

    rule=:/var/log/containers/%pod_name:char-to:_%_%namespace_name:char-to:_%_%contai\
    ner_name_and_id:char-to:.%.log

.. note::

    In the above rules, the slashes ``\`` ending each line indicate
    line wrapping - they are not part of the rule.

.. _filenamerulebase:

filenamerulebase
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "/etc/rsyslog.d/k8s_filename.rulebase", "no", "none"

When processing json-file logs, this is the rulebase used to match the filename
and extract metadata.  For the actual rules, see :ref:`filenamerules`.

.. _containerrules:

containerrules
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "SEE BELOW", "no", "none"

.. note::

    This directive is not supported with liblognorm 2.0.2 and earlier.

For journald logs, there must be a message property `CONTAINER_NAME`
which has a value matching these rules specified by this parameter.
The default value is::

    rule=:%k8s_prefix:char-to:_%_%container_name:char-to:.%.%container_hash:char-to:\
    _%_%pod_name:char-to:_%_%namespace_name:char-to:_%_%not_used_1:char-to:_%_%not_u\
    sed_2:rest%
    rule=:%k8s_prefix:char-to:_%_%container_name:char-to:_%_%pod_name:char-to:_%_%na\
    mespace_name:char-to:_%_%not_used_1:char-to:_%_%not_used_2:rest%

.. note::

    In the above rules, the slashes ``\`` ending each line indicate
    line wrapping - they are not part of the rule.

There are two rules because the `container_hash` is optional.

.. _containerrulebase:

containerrulebase
^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "/etc/rsyslog.d/k8s_container_name.rulebase", "no", "none"

When processing json-file logs, this is the rulebase used to match the
CONTAINER_NAME property value and extract metadata.  For the actual rules, see
:ref:`containerrules`.

.. _mmkubernetes-busyretryinterval:

busyretryinterval
^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "5", "no", "none"

The number of seconds to wait before retrying operations to the Kubernetes API
server after receiving a `429 Busy` response.  The default `"5"` means that the
module will retry the connection every `5` seconds.  Records processed during
this time will _not_ have any additional metadata associated with them, so you
will need to handle cases where some of your records have all of the metadata
and some do not.

If you want to have rsyslog suspend the plugin until the Kubernetes API server
is available, set `busyretryinterval` to `"0"`.  This will cause the plugin to
return an error to rsyslog.

.. _mmkubernetes-sslpartialchain:

sslpartialchain
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "off", "no", "none"

This option is only available if rsyslog was built with support for OpenSSL and
only if the `X509_V_FLAG_PARTIAL_CHAIN` flag is available.  If you attempt to
set this parameter on other platforms, you will get an `INFO` level log
message.  This was done so that you could use the same configuration on
different platforms.
If `"on"`, this will set the OpenSSL certificate store flag
`X509_V_FLAG_PARTIAL_CHAIN`.   This will allow you to verify the Kubernetes API
server cert with only an intermediate CA cert in your local trust store, rather
than having to have the entire intermediate CA + root CA chain in your local
trust store.  See also `man s_client` - the `-partial_chain` flag.
If you get errors like this, you probably need to set `sslpartialchain="on"`:

.. code-block:: none

    rsyslogd: mmkubernetes: failed to connect to [https://...url...] -
    60:Peer certificate cannot be authenticated with given CA certificates

.. _mmkubernetes-cacheexpireinterval:

cacheexpireinterval
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "-1", "no", "none"

This parameter allows you to expire entries from the metadata cache.  The
values are:

- -1 (default) - disables metadata cache expiration
- 0 - check cache for expired entries before every cache lookup
- 1 or higher - the number is a number of seconds - check the cache
  for expired entries every this many seconds, when processing an
  entry

The cache is only checked if processing a record from Kubernetes.  There
isn't some sort of housekeeping thread that continually runs cleaning up
the cache.  When an record from Kubernetes is processed:

If `cacheexpireinterval` is -1, then do not check for cache expiration.
If `cacheexpireinterval` is 0, then check for cache expiration.
If `cacheexpireinterval` is greater than 0, check for cache expiration
if the last time we checked was more than this many seconds ago.

When cache expiration is checked, it will delete all cache entries which
have a ttl less than or equal to the current time.  The cache entry ttl
is set using the `cacheentryttl`.

.. _mmkubernetes-cacheentryttl:

cacheentryttl
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "3600", "no", "none"

This parameter allows you to set the maximum age (time-to-live, or ttl) of
an entry in the metadata cache.  The value is in seconds.  The default value
is `3600` (one hour).  When cache expiration is checked, if a cache entry
has a ttl less than or equal to the current time, it will be removed from
the cache.

This option is only used if `cacheexpireinterval` is 0 or greater.

This value must be 0 or greater, otherwise, if `cacheexpireinterval` is 0
or greater, you will get an error.

.. _mmkubernetes-statistic-counter:

Statistic Counter
=================

This plugin maintains per-action :doc:`statistics
<../rsyslog_statistic_counter>`.  The statistic is named
"mmkubernetes($kubernetesurl)", where `$kubernetesurl` is the
:ref:`kubernetesurl` setting for the action.

Parameters are:

-  **recordseen** - number of messages seen by the action which the action has
   determined have Kubernetes metadata associated with them

-  **namespacemetadatasuccess** - the number of times a successful request was
   made to the Kubernetes API server for namespace metadata.

-  **namespacemetadatanotfound** - the number of times a request to the
   Kubernetes API server for namespace metadata was returned with a `404 Not
   Found` error code - the namespace did not exist at that time.

-  **namespacemetadatabusy** - the number of times a request to the Kubernetes
   API server for namespace metadata was returned with a `429 Busy` error
   code - the server was too busy to send a proper response.

-  **namespacemetadataerror** - the number of times a request to the Kubernetes
   API server for namespace metadata was returned with some other error code
   not handled above.  These are typically "hard" errors which require some
   sort of intervention to fix e.g. Kubernetes server down, credentials incorrect.

-  **podmetadatasuccess** - the number of times a successful request was made
   to the Kubernetes API server for pod metadata.

-  **podmetadatanotfound** - the number of times a request to the Kubernetes
   API server for pod metadata was returned with a `404 Not Found` error code -
   the pod did not exist at that time.

-  **podmetadatabusy** - the number of times a request to the Kubernetes API
   server for pod metadata was returned with a `429 Busy` error code - the
   server was too busy to send a proper response.

-  **podmetadataerror** - the number of times a request to the Kubernetes API
   server for pod metadata was returned with some other error code not handled
   above.  These are typically "hard" errors which require some sort of
   intervention to fix e.g. Kubernetes server down, credentials incorrect.

-  **podcachenumentries** - the number of entries in the pod metadata cache.

-  **namespacecachenumentries** - the number of entries in the namespace metadata
   cache.

-  **podcachehits** - the number of times a requested entry was found in the
   pod metadata cache.

-  **namespacecachehits** - the number of times a requested entry was found in the
   namespace metadata cache.

-  **podcachemisses** - the number of times a requested entry was not found in the
   pod metadata cache, and had to be requested from Kubernetes.

-  **namespacecachemisses** - the number of times a requested entry was not found
   in the namespace metadata cache, and had to be requested from Kubernetes.

Fields
------

These are the fields added from the metadata in the json-file filename, or from
the `CONTAINER_NAME` and `CONTAINER_ID_FULL` fields from the `imjournal` input:

`$!kubernetes!namespace_name`, `$!kubernetes!pod_name`,
`$!kubernetes!container_name`, `$!docker!id`, `$!kubernetes!master_url`.

If mmkubernetes can extract the above fields from the input, the following
fields will always be present.  If they are not present, mmkubernetes
failed to look up the namespace or pod in Kubernetes:

`$!kubernetes!namespace_id`, `$!kubernetes!pod_id`,
`$!kubernetes!creation_timestamp`, `$!kubernetes!host`

The following fields may be present, depending on how the namespace and pod are
defined in Kubernetes, and depending on the value of the directive
`annotation_match`:

`$!kubernetes!labels`, `$!kubernetes!annotations`, `$!kubernetes!namespace_labels`,
`$!kubernetes!namespace_annotations`

More fields may be added in the future.

Error Handling
--------------
If the plugin encounters a `404 Not Found` in response to a request for
namespace or pod metadata, that is, the pod or namespace is missing, the plugin
will cache that result, and no metadata will be available for that pod or
namespace forever.  If the pod or namespace is recreated, you will need to
restart rsyslog in order to clear the cache and allow it to find that metadata.

If the plugin gets a `429 Busy` response, the plugin will _not_ cache that
result, and will _not_ add the metadata to the record. This can happen in very
large Kubernetes clusters when you run into the upper limit on the number of
concurrent Kubernetes API service connections.  You may have to increase that
limit.  In the meantime, you can control what the plugin does with those
records using the :ref:`mmkubernetes-busyretryinterval` setting.  If you want
to continue to process the records, but with incomplete metadata, set
`busyretryinterval` to a non-zero value, which will be the number of seconds
after which mmkubernetes will retry the connection.  The default value is `5`,
so by default, the plugin will retry the connection every `5` seconds.  If the
`429` error condition in the Kubernetes API server is brief and transient, this
means you will have some (hopefully small) number of records without the
metadata such as the uuids, labels, and annotations, but your pipeline will not
stop.  If the `429` error condition in the Kubernetes API server is persistent,
it may require Kubernetes API server administrator intervention to address, and
you may want to use the `busyretryinterval` value of `"0"`.  This will cause
the module to return a "hard" error (see below).

For other errors, the plugin will assume they are "hard" errors requiring admin
intervention, return an error code, and rsyslog will suspend the plugin.  Use
the :ref:`mmkubernetes-statistic-counter` to monitor for problems getting data
from the Kubernetes API service.

Example
-------

Assuming you have an `imfile` input reading from docker json-file container
logs managed by Kubernetes, with `addmetadata="on"` so that mmkubernetes can
get the basic necessary Kubernetes metadata from the filename:

.. code-block:: none

    input(type="imfile" file="/var/log/containers/*.log"
          tag="kubernetes" addmetadata="on")

(Add `reopenOnTruncate="on"` if using Docker, not required by CRI-O).

and/or an `imjournal` input for docker journald container logs annotated by
Kubernetes:

.. code-block:: none

    input(type="imjournal")

Then mmkubernetes can be used to annotate log records like this:

.. code-block:: none

    module(load="mmkubernetes")

    action(type="mmkubernetes")

After this, you should have log records with fields described in the `Fields`
section above.

Credits
-------

This work is based on
https://github.com/fabric8io/fluent-plugin-kubernetes_metadata_filter
and has many of the same features.
