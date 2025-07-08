# Message Modification Module Native Interface

This document summarizes the C interface used by *mm* plugins inside the
`plugins/` directory. No standalone documentation exists; existing
modules reference the macros from `runtime/module-template.h`.

Message modification modules are implemented as output modules.  Each
plugin declares

```
MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("module_name")
```

and uses the helper macros from `module-template.h` to define entry
points.  Typical modules implement `doAction()` for per-message
processing and do **not** implement the transaction interface.

## Calling Convention

A worker instance receives a single `smsg_t *` via `doAction()`.  The
function is declared using `BEGINdoAction_NoStrings`:

```c
BEGINdoAction_NoStrings
smsg_t **ppMsg = (smsg_t **) pMsgData;
smsg_t *pMsg = ppMsg[0];
/* modify pMsg as needed */
CODESTARTdoAction
ENDdoAction
```
```
This interface processes one message at a time. Existing modules such as
`mmjsonparse` and `mmnormalize` all use this form.

While macros exist for transactional interfaces (`beginTransaction`,
`commitTransaction`, `endTransaction`) they are unused by mm modules.

## Minimal Module Skeleton

Below is a minimal skeleton that compiles against the native interface.
It omits error checking and configuration for brevity.

```c
#include "config.h"
#include "rsyslog.h"
#include "module-template.h"

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("mmskeleton")

DEF_OMOD_STATIC_DATA

typedef struct instanceData {
/* module parameters */
} instanceData;

typedef struct wrkrInstanceData {
instanceData *pData;
} wrkrInstanceData_t;

BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance

BEGINfreeInstance
CODESTARTfreeInstance
ENDfreeInstance

BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
ENDcreateWrkrInstance

BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance
ENDfreeWrkrInstance

BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
if(strncmp((char*)p, ":mmskeleton:", sizeof(":mmskeleton:") - 1))
ALIZE(RS_RET_CONFLINE_UNPROCESSED);
p += sizeof(":mmskeleton:") - 1;
CHKiRet(createInstance(&pData));
CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_TPL_AS_MSG, (uchar*)"RSYSLOG_FileFormat"));
CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct

BEGINnewActInst
CODESTARTnewActInst
CHKiRet(createInstance(&pData));
CODE_STD_STRING_REQUESTnewActInst(1)
CODE_STD_FINALIZERnewActInst
ENDnewActInst

BEGINdoAction_NoStrings
smsg_t **ppMsg = (smsg_t **)pMsgData;
smsg_t *pMsg = ppMsg[0];
(void)pWrkrData;
(void)pMsg; /* modify message here */
CODESTARTdoAction
ENDdoAction

NO_LEGACY_CONF_parseSelectorAct

BEGINmodExit
CODESTARTmodExit
ENDmodExit

BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
CODEqueryEtryPt_STD_OMOD8_QUERIES
CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
ENDqueryEtryPt

BEGINmodInit()
CODESTARTmodInit
*ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
ENDmodInit
```
