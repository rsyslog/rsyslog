.. _param-mmdarwin-filtercode:
.. _mmdarwin.parameter.input.filtercode:

filtercode
==========

.. index::
   single: mmdarwin; filtercode
   single: filtercode

.. summary-start

Sets the legacy Darwin filter code expected by older Darwin filters.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmdarwin`.

:Name: filtercode
:Scope: input
:Type: word
:Default: input=0x00000000
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Each Darwin module has a unique filter code. For example, the code of the
hostlookup filter is :json:`"0x66726570"`.
This code was mandatory but is now obsolete. It can be omitted or left at its
default value.

Input usage
-----------
.. _param-mmdarwin-input-filtercode-usage:
.. _mmdarwin.parameter.input.filtercode-usage:

.. code-block:: rsyslog

   action(type="mmdarwin" filterCode="0x72657075")

See also
--------
See also :doc:`../../configuration/modules/mmdarwin`.
