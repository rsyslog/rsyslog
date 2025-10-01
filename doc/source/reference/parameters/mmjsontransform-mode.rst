.. _param-mmjsontransform-mode:
.. _mmjsontransform.parameter.input.mode:

mode
====

.. index::
   single: mmjsontransform; mode
   single: mode

.. summary-start

Chooses whether dotted keys are expanded or flattened during processing.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmjsontransform`.

:Name: mode
:Scope: input
:Type: string
:Default: unflatten
:Required?: no
:Introduced: 8.2410.0

Description
-----------

Controls the transformation direction used by ``mmjsontransform``:

``unflatten`` (default)
   Expands dotted keys into nested containers. See
   :ref:`mmjsontransform-mode-unflatten` for details.

``flatten``
   Collapses nested containers back into dotted keys. See
   :ref:`mmjsontransform-mode-flatten` for the full description.

The comparison is case-insensitive. Omitting the parameter or providing an
empty string selects ``unflatten``. Any other value triggers a configuration
error.

Input usage
-----------
.. _param-mmjsontransform-mode-usage:
.. _mmjsontransform.parameter.input.mode-usage:

.. code-block:: rsyslog

   action(type="mmjsontransform" mode="flatten"
          input="$!normalized" output="$!normalized_flat")

See also
--------
See also the :doc:`main mmjsontransform module documentation
<../../configuration/modules/mmjsontransform>`.
