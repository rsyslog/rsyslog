.. _param-mmfields-separator:
.. _mmfields.parameter.input.separator:

separator
=========

.. index::
   single: mmfields; separator
   single: separator

.. summary-start

Sets the character that separates fields when mmfields extracts them.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmfields`.

:Name: separator
:Scope: input
:Type: char
:Default: ``,``
:Required?: no
:Introduced: 7.5.1

Description
-----------
This parameter specifies the character used to separate fields. The module
currently accepts only a single character. Multi-character separators are not
supported by ``mmfields`` but can be implemented via the RainerScript method.
CEF does not require multi-character separators, so this limitation is usually
acceptable. If you need multi-character separators in ``mmfields``, please
request this feature on the rsyslog mailing list and describe the use case;
support can be added when there is a documented requirement.

The fields are named f\ *nbr*, where *nbr* is the field number starting with
one and being incremented for each field.

Input usage
-----------
.. _param-mmfields-input-separator-usage:
.. _mmfields.parameter.input.separator-usage:

.. code-block:: rsyslog

   module(load="mmfields")
   action(type="mmfields" separator=",")

See also
--------
This parameter is part of the :doc:`../../configuration/modules/mmfields` module.
