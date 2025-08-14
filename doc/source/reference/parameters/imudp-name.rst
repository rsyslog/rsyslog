.. _param-imudp-name:
.. _imudp.parameter.module.name:

Name
====

.. index::
   single: imudp; Name
   single: Name

.. summary-start

Sets the `inputname` property used to tag messages from this listener.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: Name
:Scope: input
:Type: word
:Default: input=imudp
:Required?: no
:Introduced: 8.3.3

Description
-----------
Provides a custom identifier for messages from this input; when multiple ports share a statement, all listeners share the same name unless altered.

Input usage
-----------
.. _param-imudp-input-name:
.. _imudp.parameter.input.name:
.. code-block:: rsyslog

   input(type="imudp" Name="...")

Notes
-----
.. versionadded:: 8.3.3

See also
--------
See also :doc:`../../configuration/modules/imudp`.
