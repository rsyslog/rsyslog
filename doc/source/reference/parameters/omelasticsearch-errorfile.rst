.. _param-omelasticsearch-errorfile:
.. _omelasticsearch.parameter.module.errorfile:

errorFile
=========

.. index::
   single: omelasticsearch; errorFile
   single: errorFile

.. summary-start

File that receives records rejected during bulk mode.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: errorFile
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Failed bulk records are appended to this file with error details for later inspection or resubmission.

Action usage
------------
.. _param-omelasticsearch-action-errorfile:
.. _omelasticsearch.parameter.action.errorfile:
.. code-block:: rsyslog

   action(type="omelasticsearch" errorFile="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
