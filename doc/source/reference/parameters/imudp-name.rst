.. _param-imudp-name:
.. _imudp.parameter.module.name:

Name
====

.. index::
   single: imudp; Name
   single: Name

.. summary-start

Sets the ``inputname`` property for messages received by this listener.

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
Defines the value of the ``inputname`` property. By default all listeners report
``imudp``. When multiple ports are configured within a single input, all share
the same name unless ``name.appendPort`` is also set. The name may be an empty
string, which is mainly useful together with ``name.appendPort`` to use the port
number itself.

Input usage
-----------
.. _param-imudp-input-name:
.. _imudp.parameter.input.name:
.. code-block:: rsyslog

   input(type="imudp" Name="...")

Notes
-----
- Earlier examples referred to this parameter as ``inputname``.

See also
--------
See also :doc:`../../configuration/modules/imudp`.

