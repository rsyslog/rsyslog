.. _param-mmnormalize-allow-regex:
.. _mmnormalize.parameter.module.allow-regex:

allow_regex
===========

.. index::
   single: mmnormalize; allow_regex
   single: allow_regex

.. summary-start

Enables support for liblognorm regex field types despite higher overhead.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmnormalize`.

:Name: allow_regex
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
.. _param-mmnormalize-module-allow-regex:
.. _mmnormalize.parameter.module.allow-regex-usage:

.. code-block:: rsyslog

   module(load="mmnormalize" allowRegex="on")

See also
--------
See also :doc:`../../configuration/modules/mmnormalize`.
