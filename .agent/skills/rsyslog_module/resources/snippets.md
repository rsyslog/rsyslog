# rsyslog Module Snippets

Use these common boilerplate patterns to accelerate module development and ensure consistency with the v8 worker model.

## 1. Concurrency & Locking Header
Place this at the top of your module file (e.g., `ommodule.c`) to signal locking intention to AI agents and maintainers.

```c
/* Concurrency & Locking Checklist:
 * - pData (per-action) is shared across workers.
 * - Mutable state in pData MUST be guarded by a mutex.
 * - WID (wrkrInstanceData_t) is per-worker; no locking needed for WID internal state.
 * - Always use the "Belt and Suspenders" rule: assert(ptr != NULL); if(ptr == NULL)...
 */
```

## 2. Mandatory Entry Points (modInit)
Modularize global initialization.

```c
BEGINmodInit
CODESTARTmodInit
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* 2.2.1 is current */
CODEOKORPFAILmodInit
```

## 3. Transaction Skeletons (Output Modules)
For high-performance batching.

```c
/* Preferred Transaction Pattern */
static rsRetVal
beginTransaction(void *pWID) {
    DEFiRet;
    // Setup batch buffer or remote connection
    RETiRet;
}

static rsRetVal
commitTransaction(void *pWID) {
    DEFiRet;
    // Flush batch, handle remote acknowledgement
    RETiRet;
}
```

## 4. Parameter Definition
Using the modern `rsconf` interface.

```c
static struct c_option pblk[] = {
    { "server", eCmdHdlrGetWord, 0 },
    { "port", eCmdHdlrInt, 0 },
    { "template", eCmdHdlrGetWord, 0 }
};
```

## 5. Metadata Template (MODULE_METADATA.yaml)
Every module directory MUST contain this.

```yaml
support_status: code-supported
maturity_level: fully-mature
primary_contact: "rsyslog mailing list <https://lists.adiscon.net/mailman/listinfo/rsyslog>"
last_reviewed: 2026-01-23
```
