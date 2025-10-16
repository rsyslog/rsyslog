.. _param-mmjsonparse-userawmsg:
.. _mmjsonparse.parameter.input.userawmsg:

useRawMsg
=========

.. index::
   single: mmjsonparse; useRawMsg
   single: useRawMsg
   single: mmjsonparse; userawmsg
   single: userawmsg

.. summary-start

Controls whether parsing operates on the raw message or only the MSG part.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmjsonparse`.

:Name: useRawMsg
:Scope: input
:Type: boolean
:Default: off
:Required?: no
:Introduced: 6.6.0

Description
-----------
Specifies if the raw message should be used for parsing (``on``) or just
the MSG part of the message (``off``).

Notes
-----
- Older documentation referred to this boolean setting as ``binary``.

Input usage
-----------
.. _mmjsonparse.parameter.input.userawmsg-usage:

.. code-block:: rsyslog

   action(type="mmjsonparse" useRawMsg="on")

See also
--------
See also the :doc:`main mmjsonparse module documentation
<../../configuration/modules/mmjsonparse>`.
