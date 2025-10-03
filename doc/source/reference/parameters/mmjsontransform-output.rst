.. _param-mmjsontransform-output:
.. _mmjsontransform.parameter.input.output:

output
======

.. index::
   single: mmjsontransform; output
   single: output

.. summary-start

Sets the destination property that receives the rewritten JSON tree.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmjsontransform`.

:Name: output
:Scope: input
:Type: string
:Default: none
:Required?: yes
:Introduced: 8.2410.0

Description
-----------

Defines where the transformed JSON object is stored on the message. The
property must use a JSON-capable prefix (``$!``, ``$.`` or ``$/``). When the
configuration uses a leading ``$`` symbol, it is stripped automatically before
validation. The module rejects other prefixes during configuration.

At runtime ``mmjsontransform`` verifies that the destination property does not
already exist. Attempting to overwrite an existing value raises an error and
the action aborts without modifying the message.

Input usage
-----------
.. _param-mmjsontransform-output-usage:
.. _mmjsontransform.parameter.input.output-usage:

.. code-block:: rsyslog

   action(type="mmjsontransform" input="$!raw" output="$!normalized")

See also
--------
See also the :doc:`main mmjsontransform module documentation
<../../configuration/modules/mmjsontransform>`.
