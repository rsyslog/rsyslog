.. _param-mmjsonrewrite-output:
.. _mmjsonrewrite.parameter.input.output:

output
======

.. index::
   single: mmjsonrewrite; output
   single: output

.. summary-start

Sets the destination property that receives the rewritten JSON tree.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmjsonrewrite`.

:Name: output
:Scope: input
:Type: string
:Default: none
:Required?: yes
:Introduced: 8.2410.0

Description
-----------

Defines where the normalized JSON object is stored on the message. The property
must use a JSON-capable prefix (``$!``, ``$.`` or ``$/``). When the
configuration includes a leading ``$`` symbol it is removed automatically before
validation. Other prefixes are rejected during configuration.

At runtime ``mmjsonrewrite`` verifies that the destination property does not
already exist. Attempting to overwrite an existing value triggers an error and
the action aborts without modifying the message.

Output usage
------------
.. _param-mmjsonrewrite-output-usage:
.. _mmjsonrewrite.parameter.input.output-usage:

.. code-block:: rsyslog

   action(type="mmjsonrewrite" input="$!raw" output="$!structured")

See also
--------
See also the :doc:`main mmjsonrewrite module documentation
<../../configuration/modules/mmjsonrewrite>`.
