.. _param-omlibdbi-template:
.. _omlibdbi.parameter.module.template:
.. _omlibdbi.parameter.input.template:

Template
========

.. index::
   single: omlibdbi; Template
   single: Template

.. summary-start

Defines the template used to render records, either globally for the
module or for a specific action.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omlibdbi`.

:Name: Template
:Scope: module, input
:Type: word
:Default: module=StdDBFmt; input=inherits module
:Required?: no
:Introduced: Not documented

Description
-----------
Set the default template that omlibdbi uses when writing to the database,
then optionally override it per action.

If no template is provided at the action or module level, omlibdbi uses
the built-in ``StdDBFmt`` template.

Module usage
------------
.. _param-omlibdbi-module-template-usage:
.. _omlibdbi.parameter.module.template-usage:

.. code-block:: rsyslog

   module(load="omlibdbi" template="dbTemplate")

Input usage
-----------
.. _param-omlibdbi-input-template-usage:
.. _omlibdbi.parameter.input.template-usage:

.. code-block:: rsyslog

   action(type="omlibdbi" driver="mysql" server="db.example.net" \
          uid="dbwriter" pwd="sup3rSecret" db="syslog" \
          template="structuredDb")

See also
--------
See also :doc:`../../configuration/modules/omlibdbi`.
