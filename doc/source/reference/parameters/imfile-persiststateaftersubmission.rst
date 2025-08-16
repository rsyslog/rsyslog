.. _param-imfile-persiststateaftersubmission:
.. _imfile.parameter.input.persiststateaftersubmission:
.. _imfile.parameter.persiststateaftersubmission:

persistStateAfterSubmission
===========================

.. index::
   single: imfile; persistStateAfterSubmission
   single: persistStateAfterSubmission

.. summary-start

Persists state file information after each batch submission for robustness.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: persistStateAfterSubmission
:Scope: input
:Type: boolean
:Default: off
:Required?: no
:Introduced: 8.2006.0

Description
-----------
When switched ``on``, imfile persists state file information after a batch of
messages has been submitted. This enhances robustness against unclean
shutdowns, but may cause frequent state file writes and degrade performance,
depending on overall rsyslog configuration.

Input usage
-----------
.. _param-imfile-input-persiststateaftersubmission:
.. _imfile.parameter.input.persiststateaftersubmission-usage:

.. code-block:: rsyslog

   input(type="imfile" persistStateAfterSubmission="on")

Notes
-----
- Legacy documentation used the term ``binary`` for the type. It is treated as boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.
