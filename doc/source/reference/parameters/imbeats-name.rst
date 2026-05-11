.. _param-imbeats-name:
.. _imbeats.parameter.input.name:

name
====

.. meta::
   :description: Input name for imbeats messages.
   :keywords: rsyslog, imbeats, name

.. index::
   single: imbeats; name
   single: name

.. summary-start

Set the ``inputname`` property used for messages received by this imbeats input.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: name
:Scope: input
:Type: string
:Default: input=imbeats
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Set the ``inputname`` property used for messages received by this imbeats input.

Input usage
-----------
.. _param-imbeats-input-name:
.. _imbeats.parameter.input.name-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" name="imbeats")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
