.. _param-imudp-name:
.. _imudp.parameter.input.name:

Name
====

.. index::
   single: imudp; Name
   single: Name

.. summary-start

Value assigned to ``inputname`` property for this listener.

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
Specifies the value of the ``inputname`` property. In older versions, this was
always ``imudp`` for all listeners, which still is the default. Starting with
7.3.9 it can be set to different values for each listener. Note that when a
single input statement defines multiple listener ports, the ``inputname`` will be
the same for all of them. If you want to differentiate in that case, use
``name.appendPort`` to make them unique. Note that the ``name`` parameter can be
an empty string. In that case, the corresponding ``inputname`` property will
obviously also be the empty string. This is primarily meant to be used together
with ``name.appendPort`` to set the ``inputname`` equal to the port.

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
.. _param-imudp-input-name:
.. _imudp.parameter.input.name-usage:

.. code-block:: rsyslog

   input(type="imudp" Name="...")

See also
--------
See also :doc:`../../configuration/modules/imudp`.
