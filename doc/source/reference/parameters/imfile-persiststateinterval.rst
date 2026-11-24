.. SPDX-License-Identifier: Apache-2.0

.. _param-imfile-persiststateinterval:
.. _imfile.parameter.input.persiststateinterval:
.. _imfile.parameter.persiststateinterval:

PersistStateInterval
====================

.. index::
   single: imfile; PersistStateInterval
   single: PersistStateInterval

.. summary-start

Controls how often the state file is written based on processed line count.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: PersistStateInterval
:Scope: input
:Type: integer
:Default: 0
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Specifies the number of processed lines between state file writes for this
input. A value of ``0`` disables line-count-based writes; the state is still
written when the monitored file is closed, typically at rsyslog shutdown.
A positive value ``n`` triggers a write after ``n`` processed lines. This
parameter does not measure elapsed time or start a timer while the file is
idle. If rsyslog stops unexpectedly before the next state write, it resumes
from the last saved position and may read and submit those lines again. A
smaller interval limits this replay window when state writes succeed, at the
cost of more state file writes. Rsyslog may also write state at other points.

With :ref:`persistStateAfterSubmission <param-imfile-persiststateaftersubmission>`
enabled, imfile also writes state after flushing a submission batch. That
setting is independent of the line-count interval and can cause writes before
the configured number of lines has been reached.

An unset or zero interval disables periodic line-count checkpoints, but does
not prevent state files from being written at other points, such as when the
monitored file is closed.

Input usage
-----------
.. _param-imfile-input-persiststateinterval:
.. _imfile.parameter.input.persiststateinterval-usage:

.. code-block:: rsyslog

   input(type="imfile"
         File="/var/log/example.log"
         Tag="example"
         PersistStateInterval="0")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imfile.parameter.legacy.inputfilepersiststateinterval:

- ``$InputFilePersistStateInterval`` — maps to PersistStateInterval (status: legacy)

.. index::
   single: imfile; $InputFilePersistStateInterval
   single: $InputFilePersistStateInterval

See also
--------
See also :doc:`../../configuration/modules/imfile`.
