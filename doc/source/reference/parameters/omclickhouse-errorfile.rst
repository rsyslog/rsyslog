.. _param-omclickhouse-errorfile:
.. _omclickhouse.parameter.input.errorfile:

errorFile
=========

.. index::
   single: omclickhouse; errorFile
   single: errorFile
   single: omclickhouse; errorfile
   single: errorfile

.. summary-start

Specifies a file that receives bulk-mode failures along with the ClickHouse error response.

.. summary-end

This parameter applies to :doc:`/configuration/modules/omclickhouse`.

:Name: errorFile
:Scope: input
:Type: word
:Default: none
:Required?: no
:Introduced: not specified

Description
-----------
If specified, records failed in bulk mode are written to this file, including their error cause. Rsyslog itself does not process the file any more, but the idea behind that mechanism is that the user can create a script to periodically inspect the error file and react appropriately. As the complete request is included, it is possible to simply resubmit messages from that script.

*Please note:* when rsyslog has problems connecting to ClickHouse, a general error is assumed. However, if we receive negative responses during batch processing, we assume an error in the data itself (like a mandatory field is not filled in, a format error or something along those lines). Such errors cannot be solved by simply resubmitting the record. As such, they are written to the error file so that the user (script) can examine them and act appropriately. Note that e.g. after search index reconfiguration (e.g. dropping the mandatory attribute) a resubmit may be successful.

Input usage
-----------
.. _omclickhouse.parameter.input.errorfile-usage:

.. code-block:: rsyslog

   module(load="omclickhouse")
   action(type="omclickhouse" errorFile="clickhouse-error.log")

See also
--------
See also :doc:`/configuration/modules/omclickhouse`.
