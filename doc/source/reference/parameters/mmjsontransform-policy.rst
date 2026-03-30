.. _param-mmjsontransform-policy:
.. _mmjsontransform-policy:
.. _mmjsontransform.parameter.input.policy:

policy
======

.. index::
   single: mmjsontransform; policy
   single: policy

.. summary-start

Loads a YAML policy file that can select the transformation mode and
rename or drop keys before processing.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmjsontransform`.

:Name: policy
:Scope: input
:Type: string
:Default: unset
:Required?: no
:Introduced: 8.2602.0

Description
-----------

When set, ``mmjsontransform`` loads a YAML policy file during startup and
re-checks it on ``HUP``. If the file's mtime changed, the module reloads it;
when reload fails, the previous in-memory policy is kept. The current
implementation supports these sections:

``mode``
   Selects ``unflatten`` or ``flatten`` from the policy file. When present,
   this overrides the action parameter on startup and after a successful
   ``HUP`` reload.

``map.rename``
   Mapping of source keys to destination keys. Both keys are dotted JSON paths.

``map.drop``
   Sequence of dotted JSON paths that should be removed before transformation.

The policy is applied before the selected transformation mode runs. This makes
it possible to drop noisy fields, normalize names, and switch between
``flatten`` and ``unflatten`` from one reloadable file.

``input`` and ``output`` remain action parameters because they define which
message properties are read and written; the policy file only controls the
transformation behavior.

If rsyslog is compiled without libyaml support, setting ``policy`` produces a
configuration error.

Input usage
-----------
.. _param-mmjsontransform-policy-usage:
.. _mmjsontransform.parameter.input.policy-usage:

.. code-block:: rsyslog

   action(type="mmjsontransform"
          policy="/etc/rsyslog/mmjsontransform-policy.yaml"
          input="$!raw" output="$!normalized")

Example policy file
-------------------

.. code-block:: yaml

   version: 1
   mode: unflatten
   map:
     rename:
       "usr": "user.name"
       "ctx.old": "ctx.new"
     drop:
       - "debug"

See also
--------
See also the :doc:`main mmjsontransform module documentation
<../../configuration/modules/mmjsontransform>`.
