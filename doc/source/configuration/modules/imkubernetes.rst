.. _ref-imkubernetes:

.. meta::
   :description: rsyslog imkubernetes input module for Kubernetes pod and container logs.
   :keywords: rsyslog, imkubernetes, Kubernetes, CRI, container logs, input module

***************************************
imkubernetes: Kubernetes input module
***************************************

===========================  ==========================================================================
**Module Name:**             **imkubernetes**
**Author:**                  `rsyslog project <https://github.com/rsyslog/rsyslog>`_
**Available since:**         8.2604.0
===========================  ==========================================================================

.. summary-start

``imkubernetes`` is a Kubernetes-first input module that tails node-local pod
log files, parses CRI or Docker ``json-file`` records, preserves stream and
timestamp metadata, and can enrich each record with pod metadata from the
Kubernetes API.

.. summary-end

Purpose
=======

``imkubernetes`` is designed for node-agent deployments, typically as part of a
DaemonSet. It reads the host's Kubernetes container log files directly instead
of talking to a Docker daemon, which makes it fit current Kubernetes runtimes
better than ``imdocker``.

The module supports two common file layouts:

* pod log files such as ``/var/log/pods/<namespace>_<pod>_<uid>/<container>/<restart>.log``
* container symlink paths such as ``/var/log/containers/<pod>_<namespace>_<container>-<containerid>.log``

For CRI logs, ``imkubernetes`` merges partial records before submission. For
Docker ``json-file`` logs, it extracts the embedded timestamp, message payload,
and stream.

Message metadata
================

``imkubernetes`` stores parsed fields under ``$!kubernetes`` and, when present,
container IDs under ``$!docker``.

Common ``$!kubernetes`` properties include:

* ``namespace_name``
* ``pod_name``
* ``pod_uid``
* ``container_name``
* ``restart_count`` for pod log paths
* ``stream`` (``stdout`` or ``stderr``)
* ``log_format`` (``cri`` or ``docker_json``)
* ``log_file``

When :ref:`EnrichKubernetes <param-imkubernetes-enrichkubernetes>` is enabled,
the module also adds selected pod metadata from the Kubernetes API, including
pod UID, labels, annotations, owner references, pod IP, and host IP.

Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive. CamelCase is recommended for
   readability.

.. toctree::
   :hidden:

   ../../reference/parameters/imkubernetes-allowunsignedcerts
   ../../reference/parameters/imkubernetes-cacheentryttl
   ../../reference/parameters/imkubernetes-defaultfacility
   ../../reference/parameters/imkubernetes-defaultseverity
   ../../reference/parameters/imkubernetes-enrichkubernetes
   ../../reference/parameters/imkubernetes-escapelf
   ../../reference/parameters/imkubernetes-freshstarttail
   ../../reference/parameters/imkubernetes-kubernetesurl
   ../../reference/parameters/imkubernetes-logfileglob
   ../../reference/parameters/imkubernetes-pollinginterval
   ../../reference/parameters/imkubernetes-skipverifyhost
   ../../reference/parameters/imkubernetes-tls-cacert
   ../../reference/parameters/imkubernetes-token
   ../../reference/parameters/imkubernetes-tokenfile

Module Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imkubernetes-logfileglob`
     - .. include:: ../../reference/parameters/imkubernetes-logfileglob.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imkubernetes-pollinginterval`
     - .. include:: ../../reference/parameters/imkubernetes-pollinginterval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imkubernetes-cacheentryttl`
     - .. include:: ../../reference/parameters/imkubernetes-cacheentryttl.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imkubernetes-freshstarttail`
     - .. include:: ../../reference/parameters/imkubernetes-freshstarttail.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imkubernetes-defaultseverity`
     - .. include:: ../../reference/parameters/imkubernetes-defaultseverity.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imkubernetes-defaultfacility`
     - .. include:: ../../reference/parameters/imkubernetes-defaultfacility.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imkubernetes-escapelf`
     - .. include:: ../../reference/parameters/imkubernetes-escapelf.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imkubernetes-enrichkubernetes`
     - .. include:: ../../reference/parameters/imkubernetes-enrichkubernetes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imkubernetes-kubernetesurl`
     - .. include:: ../../reference/parameters/imkubernetes-kubernetesurl.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imkubernetes-token`
     - .. include:: ../../reference/parameters/imkubernetes-token.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imkubernetes-tokenfile`
     - .. include:: ../../reference/parameters/imkubernetes-tokenfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imkubernetes-tls-cacert`
     - .. include:: ../../reference/parameters/imkubernetes-tls-cacert.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imkubernetes-allowunsignedcerts`
     - .. include:: ../../reference/parameters/imkubernetes-allowunsignedcerts.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imkubernetes-skipverifyhost`
     - .. include:: ../../reference/parameters/imkubernetes-skipverifyhost.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

Deployment notes
================

``imkubernetes`` is intended to run close to the node logs. Typical
deployments:

* mount ``/var/log/pods`` read-only and keep the default
  :ref:`LogFileGlob <param-imkubernetes-logfileglob>`
* mount ``/var/log/containers`` read-only and override
  :ref:`LogFileGlob <param-imkubernetes-logfileglob>` if you prefer the
  symlink view
* mount the service-account token and CA certificate when enrichment is enabled

If you only need file-derived metadata, set
:ref:`EnrichKubernetes <param-imkubernetes-enrichkubernetes>` to ``off`` and
the module will avoid Kubernetes API calls entirely.

Statistic Counter
=================

This plugin maintains per-module :doc:`statistics
<../rsyslog_statistic_counter>`. The statistic name is ``imkubernetes``.

The following counters are maintained:

* ``submitted`` - records submitted to the main queue
* ``parse.errors`` - lines that could not be parsed as CRI or Docker JSON and
  were submitted as raw payloads
* ``files.discovered`` - log files discovered by glob scans
* ``records.cri`` - CRI records parsed successfully
* ``records.dockerjson`` - Docker ``json-file`` records parsed successfully
* ``kube.cache_hits`` - pod metadata cache hits
* ``kube.cache_misses`` - pod metadata cache misses
* ``kube.api_errors`` - Kubernetes API request failures
* ``ratelimit.discarded`` - records dropped by rate limiting

Examples
========

Collect pod logs with in-cluster enrichment
-------------------------------------------

.. code-block:: rsyslog

   module(
     load="imkubernetes"
     LogFileGlob="/var/log/pods/*/*/*.log"
     PollingInterval="1"
     KubernetesUrl="https://kubernetes.default.svc.cluster.local:443"
     TokenFile="/var/run/secrets/kubernetes.io/serviceaccount/token"
     tls.caCert="/var/run/secrets/kubernetes.io/serviceaccount/ca.crt"
   )

   template(name="k8s-json" type="list" option.jsonf="on") {
     property(outname="msg" name="msg" format="jsonf")
     property(outname="namespace" name="$!kubernetes!namespace_name" format="jsonf")
     property(outname="pod" name="$!kubernetes!pod_name" format="jsonf")
     property(outname="container" name="$!kubernetes!container_name" format="jsonf")
     property(outname="stream" name="$!kubernetes!stream" format="jsonf")
     property(outname="pod_ip" name="$!kubernetes!pod_ip" format="jsonf")
   }

   action(type="omfile" file="/var/log/rsyslog-k8s.json" template="k8s-json")

Collect container symlink logs without API enrichment
-----------------------------------------------------

.. code-block:: rsyslog

   module(
     load="imkubernetes"
     LogFileGlob="/var/log/containers/*.log"
     EnrichKubernetes="off"
   )

See also
========

* :doc:`imdocker` for Docker-daemon based collection on non-Kubernetes hosts
* :doc:`mmkubernetes` for post-ingest enrichment of ``imfile`` or
  ``imjournal`` pipelines
