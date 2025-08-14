.. _param-imfile-needparse:
.. _imfile.parameter.module.needparse:

needParse
=========

.. index::
   single: imfile; needParse
   single: needParse

.. summary-start

.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: needParse
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: 8.1903.0

Description
-----------
.. versionadded:: 8.1903.0

By default, read message are sent to output modules without passing through
parsers. This parameter informs rsyslog to use also defined parser module(s).

Input usage
-----------
.. _param-imfile-input-needparse:
.. _imfile.parameter.input.needparse:
.. code-block:: rsyslog

   input(type="imfile" needParse="...")

Notes
-----
- Legacy docs describe this as a binary option; it is a boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.
