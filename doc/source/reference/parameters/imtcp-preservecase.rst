.. _param-imtcp-preservecase:
.. _imtcp.parameter.module.preservecase:
.. _imtcp.parameter.input.preservecase:

PreserveCase
============

.. index::
   single: imtcp; PreserveCase
   single: PreserveCase

.. summary-start

Controls whether the case of the ``fromhost`` property is preserved.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: PreserveCase
:Scope: module, input
:Type: boolean
:Default: ``on`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: 8.37.0 (module), 8.2106.0 (input)

Description
-----------
This parameter controls the case of the ``fromhost`` property. If ``preserveCase`` is set to "off", the case in ``fromhost`` is not preserved, and the hostname will be converted to lowercase. For example, if a message is received from 'Host1.Example.Org', ``fromhost`` will be 'host1.example.org'. The default is "on" for backward compatibility.

This can be set at the module level to define a global default for all ``imtcp`` listeners. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.preservecase-usage:

.. code-block:: rsyslog

   module(load="imtcp" preserveCase="off")

Input usage
-----------
.. _imtcp.parameter.input.preservecase-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" preserveCase="off")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

This parameter has no legacy name.

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
