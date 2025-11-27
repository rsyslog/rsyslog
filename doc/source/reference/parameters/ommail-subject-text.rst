.. _param-ommail-subject-text:
.. _ommail.parameter.input.subject-text:

Subject.Text
============

.. index::
   single: ommail; Subject.Text
   single: Subject.Text

.. summary-start

Sets a fixed subject line instead of generating one from a template.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommail`.

:Name: Subject.Text
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: 8.5.0

Description
-----------
Use this parameter to set a **constant** subject text. Choose
:ref:`param-ommail-subject-template` when the subject should be generated from a
template. The ``subject.template`` and ``subject.text`` parameters cannot both
be configured within a single action.

Input usage
------------
.. _ommail.parameter.input.subject-text-usage:

.. code-block:: rsyslog

   module(load="ommail")
   action(type="ommail" subjectText="rsyslog detected disk problem")

See also
--------
See also :doc:`../../configuration/modules/ommail`.
