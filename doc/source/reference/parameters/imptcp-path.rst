.. _param-imptcp-path:
.. _imptcp.parameter.input.path:

Path
====

.. index::
   single: imptcp; Path
   single: Path

.. summary-start

Specifies a Unix-domain socket path for the listener.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: Path
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
A path on the filesystem for a unix domain socket. It is an error to specify
both ``path`` and ``port``.

Input usage
-----------
.. _param-imptcp-input-path:
.. _imptcp.parameter.input.path-usage:

.. code-block:: rsyslog

   input(type="imptcp" path="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
