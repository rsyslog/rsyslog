.. _param-immark-ruleset:
.. _immark.parameter.module.ruleset:

ruleset
=======

.. index::
   single: immark; ruleset
   single: ruleset

.. summary-start

Routes mark messages to the named ruleset instead of the default.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/immark`.

:Name: ruleset
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=none
:Required?: no
:Introduced: 8.2012.0

Description
-----------
Set this parameter to bind mark messages emitted by immark to a specific
ruleset. The module looks up the ruleset during configuration; if it is
not found, immark logs a warning and continues with the default ruleset.

If a ruleset is configured while
:ref:`use.syslogCall <param-immark-use-syslogcall>` is still enabled,
immark logs a notice and automatically switches to its internal message
submission path so the ruleset can be applied.

Module usage
------------
.. _immark.parameter.module.ruleset-usage:

.. code-block:: rsyslog

   module(load="immark"
          use.syslogCall="off"
          ruleset="markRouting")

See also
--------

* :ref:`param-immark-use-syslogcall`
* :doc:`../../configuration/modules/immark`
