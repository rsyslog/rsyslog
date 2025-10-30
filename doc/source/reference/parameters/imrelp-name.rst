.. _param-imrelp-name:
.. _imrelp.parameter.input.name:

name
====

.. index::
   single: imrelp; name
   single: name

.. summary-start

Sets a human-readable name for the RELP listener instance.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: name
:Scope: input
:Type: string
:Default: input=imrelp
:Required?: no
:Introduced: Not documented

Description
-----------
Assigns a name to the listener. If not specified, the default "imrelp" is used.

Input usage
-----------
.. _param-imrelp-input-name-usage:
.. _imrelp.parameter.input.name-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" name="relpServer")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
