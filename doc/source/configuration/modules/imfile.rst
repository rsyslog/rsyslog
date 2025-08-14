******************************
imfile: Text File Input Module
******************************

.. index:: ! imfile

===========================  ===========================================================================
**Module Name:**             **imfile**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================

Purpose
=======

This module provides the ability to convert any standard text file
into a syslog
message. A standard text file is a file consisting of printable
characters with lines being delimited by LF.

The file is read line-by-line and any line read is passed to rsyslog's
rule engine. The rule engine applies filter conditions and selects which
actions needs to be carried out. Empty lines are **not** processed, as
they would result in empty syslog records. They are simply ignored.

As new lines are written they are taken from the file and processed.
Depending on the selected mode, this happens via inotify or based on
a polling interval. Especially in polling mode, file reading doesn't
happen immediately. But there are also slight delays (due to process
scheduling and internal processing) in inotify mode.

The file monitor supports file rotation. To fully work,
rsyslogd must run while the file is rotated. Then, any remaining lines
from the old file are read and processed and when done with that, the
new file is being processed from the beginning. If rsyslogd is stopped
during rotation, the new file is read, but any not-yet-reported lines
from the previous file can no longer be obtained.

When rsyslogd is stopped while monitoring a text file, it records the
last processed location and continues to work from there upon restart.
So no data is lost during a restart (except, as noted above, if the file
is rotated just in this very moment).

Notable Features
================

- :ref:`Metadata`
- :ref:`State-Files`
- :ref:`WildCards`
- presentation on `using wildcards with imfile <http://www.slideshare.net/rainergerhards1/using-wildcards-with-rsyslogs-file-monitor-imfile>`_


Configuration
=============

.. note::
   Parameter names are case-insensitive; CamelCase is recommended for readability.


Module Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imfile-mode`
     - .. include:: ../../reference/parameters/imfile-mode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-readtimeout`
     - .. include:: ../../reference/parameters/imfile-readtimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-deletestateonfilemove`
     - .. include:: ../../reference/parameters/imfile-deletestateonfilemove.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-timeoutgranularity`
     - .. include:: ../../reference/parameters/imfile-timeoutgranularity.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-sortfiles`
     - .. include:: ../../reference/parameters/imfile-sortfiles.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-pollinginterval`
     - .. include:: ../../reference/parameters/imfile-pollinginterval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-statefile-directory`
     - .. include:: ../../reference/parameters/imfile-statefile-directory.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

Input Parameters
----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imfile-file`
     - .. include:: ../../reference/parameters/imfile-file.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-tag`
     - .. include:: ../../reference/parameters/imfile-tag.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-facility`
     - .. include:: ../../reference/parameters/imfile-facility.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-severity`
     - .. include:: ../../reference/parameters/imfile-severity.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-persiststateinterval`
     - .. include:: ../../reference/parameters/imfile-persiststateinterval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-startmsg-regex`
     - .. include:: ../../reference/parameters/imfile-startmsg-regex.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-endmsg-regex`
     - .. include:: ../../reference/parameters/imfile-endmsg-regex.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-readtimeout`
     - .. include:: ../../reference/parameters/imfile-readtimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-readmode`
     - .. include:: ../../reference/parameters/imfile-readmode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-escapelf`
     - .. include:: ../../reference/parameters/imfile-escapelf.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-escapelf-replacement`
     - .. include:: ../../reference/parameters/imfile-escapelf-replacement.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-maxlinesatonce`
     - .. include:: ../../reference/parameters/imfile-maxlinesatonce.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-maxsubmitatonce`
     - .. include:: ../../reference/parameters/imfile-maxsubmitatonce.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-deletestateonfiledelete`
     - .. include:: ../../reference/parameters/imfile-deletestateonfiledelete.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-deletestateonfilemove`
     - .. include:: ../../reference/parameters/imfile-deletestateonfilemove.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-ruleset`
     - .. include:: ../../reference/parameters/imfile-ruleset.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-addmetadata`
     - .. include:: ../../reference/parameters/imfile-addmetadata.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-addceetag`
     - .. include:: ../../reference/parameters/imfile-addceetag.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-reopenontruncate`
     - .. include:: ../../reference/parameters/imfile-reopenontruncate.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-maxlinesperminute`
     - .. include:: ../../reference/parameters/imfile-maxlinesperminute.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-maxbytesperminute`
     - .. include:: ../../reference/parameters/imfile-maxbytesperminute.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-trimlineoverbytes`
     - .. include:: ../../reference/parameters/imfile-trimlineoverbytes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-freshstarttail`
     - .. include:: ../../reference/parameters/imfile-freshstarttail.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-discardtruncatedmsg`
     - .. include:: ../../reference/parameters/imfile-discardtruncatedmsg.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-msgdiscardingerror`
     - .. include:: ../../reference/parameters/imfile-msgdiscardingerror.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-needparse`
     - .. include:: ../../reference/parameters/imfile-needparse.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-persiststateaftersubmission`
     - .. include:: ../../reference/parameters/imfile-persiststateaftersubmission.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imfile-ignoreolderthan`
     - .. include:: ../../reference/parameters/imfile-ignoreolderthan.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/imfile-mode
   ../../reference/parameters/imfile-readtimeout
   ../../reference/parameters/imfile-deletestateonfilemove
   ../../reference/parameters/imfile-timeoutgranularity
   ../../reference/parameters/imfile-sortfiles
   ../../reference/parameters/imfile-pollinginterval
   ../../reference/parameters/imfile-statefile-directory
   ../../reference/parameters/imfile-file
   ../../reference/parameters/imfile-tag
   ../../reference/parameters/imfile-facility
   ../../reference/parameters/imfile-severity
   ../../reference/parameters/imfile-persiststateinterval
   ../../reference/parameters/imfile-startmsg-regex
   ../../reference/parameters/imfile-endmsg-regex
   ../../reference/parameters/imfile-readmode
   ../../reference/parameters/imfile-escapelf
   ../../reference/parameters/imfile-escapelf-replacement
   ../../reference/parameters/imfile-maxlinesatonce
   ../../reference/parameters/imfile-maxsubmitatonce
   ../../reference/parameters/imfile-deletestateonfiledelete
   ../../reference/parameters/imfile-ruleset
   ../../reference/parameters/imfile-addmetadata
   ../../reference/parameters/imfile-addceetag
   ../../reference/parameters/imfile-reopenontruncate
   ../../reference/parameters/imfile-maxlinesperminute
   ../../reference/parameters/imfile-maxbytesperminute
   ../../reference/parameters/imfile-trimlineoverbytes
   ../../reference/parameters/imfile-freshstarttail
   ../../reference/parameters/imfile-discardtruncatedmsg
   ../../reference/parameters/imfile-msgdiscardingerror
   ../../reference/parameters/imfile-needparse
   ../../reference/parameters/imfile-persiststateaftersubmission
   ../../reference/parameters/imfile-ignoreolderthan
