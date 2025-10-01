.. _param-mmjsontransform-input:
.. _mmjsontransform.parameter.input.input:

input
=====

.. index::
   single: mmjsontransform; input
   single: input

.. summary-start

Names the JSON property that provides the object to transform.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmjsontransform`.

:Name: input
:Scope: input
:Type: string
:Default: none
:Required?: yes
:Introduced: 8.2410.0

Description
-----------

Specifies which message property contains the JSON object that will be
rewritten. The value must be a property reference that begins with ``$`` (for
example ``$!raw`` or ``$.normalized``). ``mmjsontransform`` validates the
expression during configuration and rejects other prefixes.

If the referenced property is missing at runtime, the action simply returns
without altering the message. When the property exists but is not a JSON
object, the module reports an error and the action fails, leaving the output
property untouched.

Input usage
-----------
.. _param-mmjsontransform-input-usage:
.. _mmjsontransform.parameter.input.input-usage:

.. code-block:: rsyslog

   action(type="mmjsontransform" input="$!raw" output="$!normalized")

See also
--------
See also the :doc:`main mmjsontransform module documentation
<../../configuration/modules/mmjsontransform>`.
