.. _param-imuxsock-usespecialparser:
.. _imuxsock.parameter.input.usespecialparser:

UseSpecialParser
================

.. index::
   single: imuxsock; UseSpecialParser
   single: UseSpecialParser

.. summary-start

Selects the special parser tailored for default UNIX socket format.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: UseSpecialParser
:Scope: input
:Type: boolean
:Default: input=on
:Required?: no
:Introduced: 8.9.0

Description
-----------
Equivalent to the ``SysSock.UseSpecialParser`` module parameter, but applies
to the input that is being defined.

.. versionadded:: 8.9.0
   The setting was previously hard-coded "on"

Input usage
-----------
.. _param-imuxsock-input-usespecialparser:
.. _imuxsock.parameter.input.usespecialparser-usage:

.. code-block:: rsyslog

   input(type="imuxsock" useSpecialParser="off")

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
