.. index:: ! output plugins development

Developing Output Plugins
=========================

This document describes how to develop output plugins for rsyslog.

Overview
--------

Output plugins (om* modules) are used to send messages to various destinations.

Plugin Structure
----------------

Output plugins typically consist of:

- Module initialization
- Action configuration
- Message processing
- Cleanup functions

Example Plugin
--------------

Here's a basic example of an output plugin:

.. code-block:: c

   #include "rsyslog.h"
   #include "module-template.h"
   
   MODULE_TYPE_OUTPUT
   MODULE_TYPE_NOKEEP
   
   BEGINbeginCnf
   ENDbeginCnf
   
   BEGINendCnf
   ENDendCnf
   
   BEGINcreateInstance
   ENDcreateInstance
   
   BEGINisCompatibleWithFeature
   ENDisCompatibleWithFeature
   
   BEGINfreeInstance
   ENDfreeInstance
   
   BEGINwillRollback
   ENDwillRollback
   
   BEGINdoAction
   ENDdoAction
   
   BEGINparseSelectorAct
   ENDparseSelectorAct
   
   BEGINmodExit
   ENDmodExit
   
   BEGINqueryEtryPt
   ENDqueryEtryPt
   
   rsRetVal modInit(instanceConf_t *pModConf)
   {
       return RS_RET_OK;
   } 