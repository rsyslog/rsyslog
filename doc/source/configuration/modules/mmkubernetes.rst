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
(:ref:`param-mmkubernetes-filenamerules`) or `filenamerulebase` (:ref:`param-mmkubernetes-filenamerulebase`)
parameter values.

For journald logs, there must be a message property `CONTAINER_NAME`
which matches the liblognorm rules specified by the `containerrules`
(:ref:`param-mmkubernetes-containerrules`) or `containerrulebase`
(:ref:`param-mmkubernetes-containerrulebase`) parameter values. The record must also have
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

   Parameter names are case-insensitive; camelCase is recommended for readability.

.. toctree::
   :hidden:

   ../../reference/parameters/mmkubernetes-annotation-match
   ../../reference/parameters/mmkubernetes-allowunsignedcerts
   ../../reference/parameters/mmkubernetes-busyretryinterval
   ../../reference/parameters/mmkubernetes-cacheentryttl
   ../../reference/parameters/mmkubernetes-cacheexpireinterval
   ../../reference/parameters/mmkubernetes-containerrulebase
   ../../reference/parameters/mmkubernetes-containerrules
   ../../reference/parameters/mmkubernetes-de-dot
   ../../reference/parameters/mmkubernetes-de-dot-separator
   ../../reference/parameters/mmkubernetes-dstmetadatapath
   ../../reference/parameters/mmkubernetes-filenamerulebase
   ../../reference/parameters/mmkubernetes-filenamerules
   ../../reference/parameters/mmkubernetes-kubernetesurl
   ../../reference/parameters/mmkubernetes-skipverifyhost
   ../../reference/parameters/mmkubernetes-srcmetadatapath
   ../../reference/parameters/mmkubernetes-sslpartialchain
   ../../reference/parameters/mmkubernetes-tls-cacert
   ../../reference/parameters/mmkubernetes-tls-mycert
   ../../reference/parameters/mmkubernetes-tls-myprivkey
   ../../reference/parameters/mmkubernetes-token
   ../../reference/parameters/mmkubernetes-tokenfile

Action Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-mmkubernetes-annotation-match`
     - .. include:: ../../reference/parameters/mmkubernetes-annotation-match.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-allowunsignedcerts`
     - .. include:: ../../reference/parameters/mmkubernetes-allowunsignedcerts.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-busyretryinterval`
     - .. include:: ../../reference/parameters/mmkubernetes-busyretryinterval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-cacheentryttl`
     - .. include:: ../../reference/parameters/mmkubernetes-cacheentryttl.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-cacheexpireinterval`
     - .. include:: ../../reference/parameters/mmkubernetes-cacheexpireinterval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-containerrulebase`
     - .. include:: ../../reference/parameters/mmkubernetes-containerrulebase.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-containerrules`
     - .. include:: ../../reference/parameters/mmkubernetes-containerrules.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-de-dot`
     - .. include:: ../../reference/parameters/mmkubernetes-de-dot.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-de-dot-separator`
     - .. include:: ../../reference/parameters/mmkubernetes-de-dot-separator.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-dstmetadatapath`
     - .. include:: ../../reference/parameters/mmkubernetes-dstmetadatapath.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-filenamerulebase`
     - .. include:: ../../reference/parameters/mmkubernetes-filenamerulebase.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-filenamerules`
     - .. include:: ../../reference/parameters/mmkubernetes-filenamerules.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-kubernetesurl`
     - .. include:: ../../reference/parameters/mmkubernetes-kubernetesurl.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-skipverifyhost`
     - .. include:: ../../reference/parameters/mmkubernetes-skipverifyhost.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-srcmetadatapath`
     - .. include:: ../../reference/parameters/mmkubernetes-srcmetadatapath.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-sslpartialchain`
     - .. include:: ../../reference/parameters/mmkubernetes-sslpartialchain.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-tls-cacert`
     - .. include:: ../../reference/parameters/mmkubernetes-tls-cacert.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-tls-mycert`
     - .. include:: ../../reference/parameters/mmkubernetes-tls-mycert.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-tls-myprivkey`
     - .. include:: ../../reference/parameters/mmkubernetes-tls-myprivkey.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-token`
     - .. include:: ../../reference/parameters/mmkubernetes-token.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmkubernetes-tokenfile`
     - .. include:: ../../reference/parameters/mmkubernetes-tokenfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. _mmkubernetes-statistic-counter:

Statistic Counter
=================

This plugin maintains per-action :doc:`statistics
<../rsyslog_statistic_counter>`.  The statistic is named
"mmkubernetes($kubernetesurl)", where `$kubernetesurl` is the
:ref:`param-mmkubernetes-kubernetesurl` setting for the action.

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
records using the :ref:`param-mmkubernetes-busyretryinterval` setting.  If you want
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
