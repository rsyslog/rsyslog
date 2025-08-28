.. _param-imuxsock-parsehostname:
.. _imuxsock.parameter.input.parsehostname:

ParseHostname
=============

.. index::
   single: imuxsock; ParseHostname
   single: ParseHostname

.. summary-start

Expects hostnames in messages when the special parser is disabled.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: ParseHostname
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: 8.9.0

Description
-----------
Equivalent to the ``SysSock.ParseHostname`` module parameter, but applies
to the input that is being defined.

.. versionadded:: 8.9.0

Input usage
-----------
.. _param-imuxsock-input-parsehostname:
.. _imuxsock.parameter.input.parsehostname-usage:

.. code-block:: rsyslog

   input(type="imuxsock" parseHostname="on")

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
