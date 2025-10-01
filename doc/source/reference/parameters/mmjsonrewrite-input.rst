.. _param-mmjsonrewrite-input:
.. _mmjsonrewrite.parameter.input.input:

input
=====

.. index::
   single: mmjsonrewrite; input
   single: input

.. summary-start

Names the JSON property that supplies the tree to rewrite.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmjsonrewrite`.

:Name: input
:Scope: input
:Type: string
:Default: none
:Required?: yes
:Introduced: 8.2410.0

Description
-----------

Specifies which message property contains the JSON object that will be
normalized. The value must be a property reference that begins with ``$`` (for
example ``$!raw`` or ``$.structured``). ``mmjsonrewrite`` validates the
expression at load time and rejects other prefixes.

If the property is absent at runtime, the action returns without altering the
message. When the property exists but does not resolve to a JSON object, the
module logs an error and the action fails without creating the output tree.

Input usage
-----------
.. _param-mmjsonrewrite-input-usage:
.. _mmjsonrewrite.parameter.input.input-usage:

.. code-block:: rsyslog

   action(type="mmjsonrewrite" input="$!raw" output="$!structured")

See also
--------
See also the :doc:`main mmjsonrewrite module documentation
<../../configuration/modules/mmjsonrewrite>`.
