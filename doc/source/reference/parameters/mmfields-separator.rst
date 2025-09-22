.. _param-mmfields-separator:
.. _mmfields.parameter.module.separator:

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
:Scope: module
:Type: char
:Default: ,
:Required?: no
:Introduced: 7.5.1

Description
-----------
This is the character used to separate fields. Currently, only a single
character is permitted, while the RainerScript method permits to specify
multi-character separator strings. For CEF, this is not required. If there
is actual need to support multi-character separator strings, support can
relatively easy be added. It is suggested to request it on the rsyslog
mailing list, together with the use case - we intend to add functionality only
if there is a real use case behind the request (in the past we too-often
implemented things that actually never got used).

The fields are named f\ *nbr*, where *nbr* is the field number starting with
one and being incremented for each field.

Module usage
------------
.. _param-mmfields-module-separator-usage:
.. _mmfields.parameter.module.separator-usage:

.. code-block:: rsyslog

   module(load="mmfields")
   action(type="mmfields" separator=",")

See also
--------
See also :doc:`../../configuration/modules/mmfields`.
