.. _ref-templates-type-list:

List template type
==================

.. summary-start

List templates build output from a sequence of constant and property statements.
They suit complex substitutions and structure-aware outputs.
.. summary-end

In list templates, the template is defined by a series of constant and
property statements enclosed in curly braces. This format works well for
structure-aware outputs such as ommongodb
while also supporting text-based outputs. It is recommended when complex
property substitutions are required because the syntax is clearer than a
single string template.

An example list template:

.. code-block:: none

   template(name="tpl1" type="list") {
        constant(value="Syslog MSG is: '")
        property(name="msg")
        constant(value="', ")
        property(name="timereported" dateFormat="rfc3339" caseConversion="lower")
        constant(value="\n")
   }

Subsections
-----------

Constant statement
  See :ref:`ref-templates-statement-constant` for full details.

Property statement
  See :ref:`ref-templates-statement-property` for full details.

