.. _param-imudp-name-appendport:
.. _imudp.parameter.input.name-appendport:

Name.appendPort
===============

.. index::
   single: imudp; Name.appendPort
   single: Name.appendPort

.. summary-start

Appends listener port number to ``inputname``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: Name.appendPort
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: 7.3.9

Description
-----------
Appends the port to the ``inputname`` property. Note that when no ``name`` is
specified, the default of ``imudp`` is used and the port is appended to that
default. So, for example, a listener port of 514 in that case will lead to an
``inputname`` of ``imudp514``. The ability to append a port is most useful when
multiple ports are defined for a single input and each of the ``inputname``
values shall be unique. Note that there currently is no differentiation between
IPv4/v6 listeners on the same port.

Examples:

.. code-block:: rsyslog

   module(load="imudp")
   input(type="imudp" port=["10514","10515","10516"]
         name="udp" name.appendPort="on")

.. code-block:: rsyslog

   module(load="imudp")
   input(type="imudp" port=["10514","10515","10516"]
         name="" name.appendPort="on")

Input usage
-----------
.. _param-imudp-input-name-appendport:
.. _imudp.parameter.input.name-appendport-usage:

.. code-block:: rsyslog

   input(type="imudp" Name.appendPort="...")

Notes
-----
- Earlier documentation described the type as ``binary``; this maps to boolean.

See also
--------
See also :doc:`../../configuration/modules/imudp`.
