.. meta::
   :description: The mmjsontransform policyWatch parameter enables automatic reloads for watched policy files.
   :keywords: rsyslog, mmjsontransform, policyWatch, watched reload, yaml policy

.. _param-mmjsontransform-policywatch:
.. _mmjsontransform.parameter.input.policywatch:

policyWatch
===========

.. index::
   single: mmjsontransform; policyWatch
   single: policyWatch

.. summary-start

Enables automatic reload of the configured policy file when it changes.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmjsontransform`.

:Name: policyWatch
:Scope: input
:Type: boolean
:Default: off
:Required?: no
:Introduced: 8.2604.0

Description
-----------

When enabled together with :ref:`policy <param-mmjsontransform-policy>`,
``mmjsontransform`` watches the configured YAML policy file and reloads it
after the configured debounce interval. This is useful for targeted policy
updates that should not wait for a full ``HUP`` cycle.

The watcher monitors the file's parent directory and matches the configured
basename, so common write-temp-and-rename editor save patterns also trigger a
reload.

If watch support is unavailable in the current build or runtime environment,
rsyslog logs a warning and continues with ``HUP``-only reload behavior.

If the updated policy is invalid, the reload is rejected and the previous
in-memory policy remains active.

Input usage
-----------
.. _param-mmjsontransform-policywatch-usage:
.. _mmjsontransform.parameter.input.policywatch-usage:

.. code-block:: rsyslog

   action(type="mmjsontransform"
          policy="/etc/rsyslog/mmjsontransform-policy.yaml"
          policyWatch="on"
          input="$!raw" output="$!normalized")

See also
--------
See also the :doc:`main mmjsontransform module documentation
<../../configuration/modules/mmjsontransform>`.
