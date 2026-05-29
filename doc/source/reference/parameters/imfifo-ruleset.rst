.. _param-imfifo-ruleset:
.. _imfifo.parameter.input.ruleset:

ruleset
=======

.. meta::
   :description: Bind the imfifo input instance to a specific ruleset.
   :keywords: rsyslog, imfifo, ruleset

.. index::
   single: imfifo; ruleset
   single: ruleset

.. summary-start

Binds this imfifo input instance to a specific ruleset.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfifo`.

:Name: ruleset
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: none
:Required?: no
:Introduced: 8.2606.0

Description
-----------
Binds the input instance to a specific ruleset. If specified, messages read from this FIFO will be processed by the ruleset instead of the default ruleset.

Input usage
-----------
.. _param-imfifo-input-ruleset:
.. _imfifo.parameter.input.ruleset-usage:

.. code-block:: rsyslog

   input(type="imfifo"
         file="/var/log/myfifo"
         tag="fifo-logs:"
         ruleset="my_custom_ruleset")

See also
--------
See also :doc:`../../configuration/modules/imfifo`.
