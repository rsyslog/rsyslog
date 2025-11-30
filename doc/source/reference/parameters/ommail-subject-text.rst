.. _param-ommail-subject-text:
.. _ommail.parameter.input.subject-text:

subjectText
===========

.. index::
   single: ommail; subjectText
   single: subjectText

.. summary-start

Sets a fixed subject line instead of generating one from a template.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommail`.

:Name: subjectText
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: 8.5.0

Description
-----------
Use this parameter to set a **constant** subject text. Choose
:ref:`param-ommail-subject-template` when the subject should be generated from a
template. The ``subjectTemplate`` and ``subjectText`` parameters cannot both
be configured within a single action. If neither is specified, a default,
non-configurable subject line will be generated.

Input usage
------------
.. _ommail.parameter.input.subject-text-usage:

.. code-block:: rsyslog

   module(load="ommail")
   action(
       type="ommail"
       server="mail.example.net"
       port="25"
       mailFrom="rsyslog@example.net"
       mailTo="operator@example.net"
       subjectText="rsyslog detected disk problem"
   )

See also
--------
See also :doc:`../../configuration/modules/ommail`.
