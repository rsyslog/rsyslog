.. _param-imfile-persiststateaftersubmission:
.. _imfile.parameter.module.persiststateaftersubmission:

persistStateAfterSubmission
===========================

.. index::
   single: imfile; persistStateAfterSubmission
   single: persistStateAfterSubmission

.. summary-start

Ensures state file information is written after each batch of messages has been submitted for improved robustness.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: persistStateAfterSubmission
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: 8.2006.0

Description
-----------
.. versionadded:: 8.2006.0

This setting makes imfile persist state file information after a batch of
messages has been submitted. It can be activated (switched to "on") in order
to provide enhanced robustness against unclean shutdowns. Depending on the
configuration of the rest of rsyslog (most importantly queues), persisting
the state file after each message submission prevents message loss
when reading files and the system is shutdown in an unclean way (e.g.
loss of power).

Please note that this setting may cause frequent state file writes and
as such may cause some performance degradation.

Input usage
-----------
.. _param-imfile-input-persiststateaftersubmission:
.. _imfile.parameter.input.persiststateaftersubmission:
.. code-block:: rsyslog

   input(type="imfile" persistStateAfterSubmission="...")

Notes
-----
- Legacy docs describe this as a binary option; it is a boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.
