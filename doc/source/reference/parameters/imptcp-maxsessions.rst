.. _param-imptcp-maxsessions:
.. _imptcp.parameter.module.maxsessions:
.. _imptcp.parameter.input.maxsessions:

MaxSessions
===========

.. index::
   single: imptcp; MaxSessions
   single: MaxSessions

.. summary-start

Limits the number of open sessions.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: MaxSessions
:Scope: module, input
:Type: integer
:Default: module=0; input=0
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Maximum number of open sessions allowed. When used as a module parameter
this value becomes the default inherited by each ``input()`` instance but
is not a global maximum. When set inside an input, it applies to that
listener. A setting of zero or less than zero means no limit.

Module usage
------------
.. _param-imptcp-module-maxsessions:
.. _imptcp.parameter.module.maxsessions-usage:

.. code-block:: rsyslog

   module(load="imptcp" maxSessions="...")

Input usage
-----------
.. _param-imptcp-input-maxsessions:
.. _imptcp.parameter.input.maxsessions-usage:
.. code-block:: rsyslog

   input(type="imptcp" maxSessions="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
