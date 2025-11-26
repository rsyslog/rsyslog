.. _param-ommail-subject-text:
.. _ommail.parameter.module.subject-text:

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
:Scope: module
:Type: string
:Default: module=none
:Required?: no
:Introduced: 8.5.0

Description
-----------
Use this parameter to set a **constant** subject text. Choose :ref:`param-ommail-subject-template` when the subject should be generated from a template. The *subject.template* and *subject.text* parameters cannot both be configured within a single action.

Module usage
------------
.. _ommail.parameter.module.subject-text-usage:

.. code-block:: rsyslog

   module(load="ommail")
   action(type="ommail" subject.text="rsyslog detected disk problem")

See also
--------
See also :doc:`../../configuration/modules/ommail`.
