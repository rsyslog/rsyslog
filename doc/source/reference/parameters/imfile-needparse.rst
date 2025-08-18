.. _param-imfile-needparse:
.. _imfile.parameter.input.needparse:
.. _imfile.parameter.needparse:

needParse
=========

.. index::
   single: imfile; needParse
   single: needParse

.. summary-start

Forces messages to pass through configured parser modules.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: needParse
:Scope: input
:Type: boolean
:Default: off
:Required?: no
:Introduced: 8.1903.0

Description
-----------
By default, read messages are sent to output modules without passing through
parsers. Setting ``needParse`` to ``on`` makes rsyslog apply any defined
parser modules to these messages.

Input usage
-----------
.. _param-imfile-input-needparse:
.. _imfile.parameter.input.needparse-usage:

.. code-block:: rsyslog

   input(type="imfile" needParse="on")

Notes
-----
- Legacy documentation used the term ``binary`` for the type. It is treated as boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.
