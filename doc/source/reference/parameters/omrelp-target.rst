.. _param-omrelp-target:
.. _omrelp.parameter.input.target:

Target
======

.. index::
   single: omrelp; Target
   single: Target

.. summary-start

Defines the remote server omrelp connects to.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: Target
:Scope: input
:Type: string
:Default: input=none
:Required?: yes
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
The target server to connect to.

Input usage
-----------
.. _param-omrelp-input-target:
.. _omrelp.parameter.input.target-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
