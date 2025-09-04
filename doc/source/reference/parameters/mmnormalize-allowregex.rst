.. _param-mmnormalize-allowregex:
.. _mmnormalize.parameter.module.allowregex:

allowRegex
===========

.. index::
   single: mmnormalize; allowRegex
   single: allowRegex

.. summary-start

Enables support for liblognorm regex field types despite higher overhead.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmnormalize`.

:Name: allowRegex
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: at least 6.1.2, possibly earlier

Description
-----------
Specifies if regex field-type should be allowed. Regex field-type has significantly higher computational overhead compared to other fields, so it should be avoided when another field-type can achieve the desired effect. Needs to be "on" for regex field-type to work.

Module usage
------------
.. _param-mmnormalize-module-allowregex:
.. _mmnormalize.parameter.module.allowregex-usage:

.. code-block:: rsyslog

   module(load="mmnormalize" allowRegex="on")

See also
--------
See also :doc:`../../configuration/modules/mmnormalize`.
