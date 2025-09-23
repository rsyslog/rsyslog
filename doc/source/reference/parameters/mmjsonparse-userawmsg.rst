.. _param-mmjsonparse-userawmsg:
.. _mmjsonparse.parameter.input.userawmsg:

useRawMsg
=========

.. index::
   single: mmjsonparse; useRawMsg
   single: useRawMsg

.. summary-start

Controls whether parsing operates on the raw message instead of only the MSG part.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmjsonparse`.

:Name: useRawMsg
:Scope: input
:Type: boolean
:Default: off
:Required?: no
:Introduced: at least 6.6.0, possibly earlier

Description
-----------
Specifies if the raw message should be used for normalization (``on``) or just
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
See also :doc:`../../configuration/modules/mmjsonparse`.
