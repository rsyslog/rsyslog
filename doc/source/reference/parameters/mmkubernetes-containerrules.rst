.. _param-mmkubernetes-containerrules:
.. _mmkubernetes.parameter.action.containerrules:

containerrules
==============

.. index::
   single: mmkubernetes; containerrules
   single: containerrules

.. summary-start

Defines lognorm rules to parse ``CONTAINER_NAME`` values for metadata.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: containerrules
:Scope: action
:Type: word
:Default: action=SEE BELOW
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
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

    In the above rules, the slashes ``\\`` ending each line indicate
    line wrapping - they are not part of the rule.

There are two rules because the `container_hash` is optional.

Action usage
------------
.. _param-mmkubernetes-action-containerrules:
.. _mmkubernetes.parameter.action.containerrules-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" containerRules="...")

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.
