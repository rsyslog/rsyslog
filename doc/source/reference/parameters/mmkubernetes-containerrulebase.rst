.. _param-mmkubernetes-containerrulebase:
.. _mmkubernetes.parameter.action.containerrulebase:

containerrulebase
=================

.. index::
   single: mmkubernetes; containerrulebase
   single: containerrulebase

.. summary-start

Specifies the rulebase file used to parse ``CONTAINER_NAME`` values.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: containerrulebase
:Scope: action
:Type: word
:Default: action=/etc/rsyslog.d/k8s_container_name.rulebase
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
When processing journald logs, this is the rulebase used to match the
CONTAINER_NAME property value and extract metadata.  For the actual rules, see
:ref:`param-mmkubernetes-containerrules`.

Action usage
------------
.. _param-mmkubernetes-action-containerrulebase:
.. _mmkubernetes.parameter.action.containerrulebase-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" containerRuleBase="/etc/rsyslog.d/k8s_container_name.rulebase")

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.
