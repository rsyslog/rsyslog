.. _param-omstdout-template:
.. _omstdout.parameter.input.template:

template
========

.. index::
   single: omstdout; template
   single: template

.. summary-start

Selects the rsyslog template used to format messages written to stdout.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omstdout`.

:Name: template
:Scope: input
:Type: word
:Default: input=RSYSLOG_FileFormat
:Required?: no
:Introduced: 4.1.6

Description
-----------
Set the template that will be used for the output. If none is specified the
module uses the default template.

Input usage
-----------
.. _param-omstdout-input-template:
.. _omstdout.parameter.input.template-usage:

.. code-block:: rsyslog

   action(type="omstdout" template="outfmt")

See also
--------
See also :doc:`../../configuration/modules/omstdout`.
