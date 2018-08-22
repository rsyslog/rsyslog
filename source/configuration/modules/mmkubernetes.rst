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
