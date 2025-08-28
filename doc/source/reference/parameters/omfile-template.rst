.. _param-omfile-template:
.. _omfile.parameter.module.template:

Template
========

.. index::
   single: omfile; Template
   single: Template

.. summary-start

Sets the template to be used for this action.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: Template
:Scope: module, action
:Type: word
:Default: module=RSYSLOG_FileFormat; action=inherits module
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------

Sets the template to be used for this action.

When set on the module, the value becomes the default for actions.

Module usage
------------

.. _param-omfile-module-template:
.. _omfile.parameter.module.template-usage:
.. code-block:: rsyslog

   module(load="builtin:omfile" Template="...")

Action usage
------------

.. _param-omfile-action-template:
.. _omfile.parameter.action.template:
.. code-block:: rsyslog

   action(type="omfile" Template="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.actionfiledefaulttemplate:

- $ActionFileDefaultTemplate — maps to Template (status: legacy)

.. index::
   single: omfile; $ActionFileDefaultTemplate
   single: $ActionFileDefaultTemplate

See also
--------

See also :doc:`../../configuration/modules/omfile`.
