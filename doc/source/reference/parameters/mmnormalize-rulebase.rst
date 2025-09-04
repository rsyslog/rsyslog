.. _param-mmnormalize-rulebase:
.. _mmnormalize.parameter.action.rulebase:

ruleBase
========

.. index::
   single: mmnormalize; ruleBase
   single: ruleBase

.. summary-start

Sets the rulebase file used for normalization.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmnormalize`.

:Name: ruleBase
:Scope: action
:Type: word
:Default: none
:Required?: yes
:Introduced: at least 6.1.2, possibly earlier

Description
-----------
Specifies which rulebase file is to use. If there are multiple mmnormalize
instances, each one can use a different file. However, a single instance can
use only a single file. This parameter or ``rule`` MUST be given, because
normalization can only happen based on a rulebase. It is recommended that an
absolute path name is given. Information on how to create the rulebase can
be found in the `liblognorm manual <https://www.liblognorm.com/manual/>`_.

Action usage
-------------
.. _param-mmnormalize-action-rulebase:
.. _mmnormalize.parameter.action.rulebase-usage:

.. code-block:: rsyslog

   action(type="mmnormalize" ruleBase="/path/to/rulebase.rb")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _mmnormalize.parameter.legacy.mmnormalizerulebase:

- $mmnormalizeRuleBase — maps to ruleBase (status: legacy)

.. index::
   single: mmnormalize; $mmnormalizeRuleBase
   single: $mmnormalizeRuleBase

See also
--------
See also :doc:`../../configuration/modules/mmnormalize`.
