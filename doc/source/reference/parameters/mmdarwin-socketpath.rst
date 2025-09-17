.. _param-mmdarwin-socketpath:
.. _mmdarwin.parameter.input.socketpath:

socketpath
==========

.. index::
   single: mmdarwin; socketpath
   single: socketpath

.. summary-start

Specifies the Darwin filter socket path that mmdarwin connects to.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmdarwin`.

:Name: socketpath
:Scope: input
:Type: word
:Default: input=none
:Required?: yes
:Introduced: at least 8.x, possibly earlier

Description
-----------
The Darwin filter socket path to use.

Input usage
-----------
.. _param-mmdarwin-input-socketpath:
.. _mmdarwin.parameter.input.socketpath-usage:

.. code-block:: rsyslog

   action(type="mmdarwin" socketPath="/path/to/reputation_1.sock")

See also
--------
See also :doc:`../../configuration/modules/mmdarwin`.
