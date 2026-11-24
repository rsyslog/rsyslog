.. SPDX-License-Identifier: Apache-2.0

.. _param-imfile-persiststateaftersubmission:
.. _imfile.parameter.input.persiststateaftersubmission:
.. _imfile.parameter.persiststateaftersubmission:

persistStateAfterSubmission
===========================

.. index::
   single: imfile; persistStateAfterSubmission
   single: persistStateAfterSubmission

.. summary-start

Persists state after imfile flushes submissions at the end of a read pass.

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
When switched ``on``, imfile writes state after flushing its pending messages
at the end of a file read pass. The write follows imfile's submission to the
processing queue; it does not confirm downstream processing or durable output
delivery. This enhances robustness against unclean shutdowns, but may cause
frequent state file writes and degrade performance, depending on overall
rsyslog configuration.

This setting is independent of :ref:`PersistStateInterval
<param-imfile-persiststateinterval>`: it can write state before the configured
line count is reached. It does not turn ``PersistStateInterval`` into a time
interval.

Input usage
-----------
.. _param-imfile-input-persiststateaftersubmission:
.. _imfile.parameter.input.persiststateaftersubmission-usage:

.. code-block:: rsyslog

   input(type="imfile"
         File="/var/log/example.log"
         Tag="example"
         persistStateAfterSubmission="on")

Notes
-----
- Legacy documentation used the term ``binary`` for the type. It is treated as boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.
