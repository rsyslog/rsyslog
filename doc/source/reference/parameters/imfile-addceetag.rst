.. _param-imfile-addceetag:
.. _imfile.parameter.input.addceetag:
.. _imfile.parameter.addceetag:

addCeeTag
=========

.. index::
   single: imfile; addCeeTag
   single: addCeeTag

.. summary-start

Adds the ``@cee:`` cookie to the message when enabled.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: addCeeTag
:Scope: input
:Type: boolean
:Default: off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Turns on or off the addition of the ``@cee:`` cookie to the message
object.

Input usage
-----------
.. _param-imfile-input-addceetag:
.. _imfile.parameter.input.addceetag-usage:

.. code-block:: rsyslog

   input(type="imfile" addCeeTag="on")

Notes
-----
- Legacy documentation used the term ``binary`` for the type. It is treated as boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.
