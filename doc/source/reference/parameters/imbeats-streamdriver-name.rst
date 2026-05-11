.. _param-imbeats-streamdriver-name:
.. _imbeats.parameter.input.streamdriver-name:

StreamDriver.Name
=================

.. meta::
   :description: Stream driver backend for imbeats.
   :keywords: rsyslog, imbeats, streamdriver name

.. index::
   single: imbeats; StreamDriver.Name
   single: StreamDriver.Name

.. summary-start

Select the netstrm backend used by imbeats, for example ``ptcp``, ``gtls``, or ``ossl``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: StreamDriver.Name
:Scope: input
:Type: string
:Default: input=ptcp
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Select the netstrm backend used by imbeats, for example ``ptcp``, ``gtls``, or ``ossl``.

Input usage
-----------
.. _param-imbeats-input-streamdriver-name:
.. _imbeats.parameter.input.streamdriver-name-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" streamDriver.name="ossl")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
