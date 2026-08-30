/* reload-candidate.h
 *
 * Side-effect-free syntax capture for a reload candidate. This is control
 * path code: it never publishes runtime objects or touches message flow.
 */
#ifndef RELOAD_CANDIDATE_H_INCLUDED
#define RELOAD_CANDIDATE_H_INCLUDED 1

#include <stddef.h>

#include "rsyslog.h"
#include "reload-normalized-graph.h"
#include "reload-report.h"

struct cnfobj;
struct cnfstmt;
struct nvlst;
typedef struct rsReloadCandidate_s rsReloadCandidate_t;
typedef rsRetVal (*rsReloadCandidateObjectVisitorV1_t)(const struct cnfobj *object, size_t parseOrdinal, void *context);
typedef rsRetVal (*rsReloadCandidateRulesetFragmentVisitorV1_t)(const char *canonicalIdentity,
                                                                const struct cnfstmt *fragment,
                                                                int firstForCanonicalIdentity,
                                                                int isSyntheticDefault,
                                                                void *context);

rsRetVal rsReloadCandidateParse(const char *path, rsReloadCandidate_t **ppCandidate);
void rsReloadCandidateDestruct(rsReloadCandidate_t **ppCandidate);
size_t rsReloadCandidateObjectCount(const rsReloadCandidate_t *candidate);
/* Objects are borrowed for the callback only and retain frontend parse order.
 * The visitor must not mutate, destruct, or retain them. */
rsRetVal rsReloadCandidateVisitObjectsV1(const rsReloadCandidate_t *candidate,
                                         rsReloadCandidateObjectVisitorV1_t visitor,
                                         void *context);
/* Build an independent, ordered catalog containing global, module, input, and
 * named rate-limit declarations. This is the source material required by base
 * and module-specific effective-profile classifiers; unrelated scripts and
 * objects are omitted. Sequential global blocks retain parse order so a
 * private lowerer can reproduce the startup last-write merge. Rate-limit
 * declarations remain syntax-only until a capability-specific Prepare path
 * validates and materializes them. */
rsRetVal rsReloadCandidateBuildObjectCatalogV1(const rsReloadCandidate_t *candidate, rsReloadCandidate_t **ppCatalog);
/* Identity and fragment are borrowed for the callback only. The visitor must
 * not mutate or retain them. Fragments are visited in effective parse order. */
rsRetVal rsReloadCandidateVisitRulesetFragmentsV1(const rsReloadCandidate_t *candidate,
                                                  rsReloadCandidateRulesetFragmentVisitorV1_t visitor,
                                                  void *context);

/*
 * Convert the private, frontend-neutral cnfobj/nvlst capture into an owned
 * normalized graph.  This is deliberately a control-path API: it retains no
 * runtime object or module pointers and consumes the shared capture objects
 * produced by either frontend. Cross-frontend semantic equivalence still
 * requires integration-level parity tests before activation. The graph is
 * structural evidence only, not semantic validation or an activation
 * decision.
 */
rsRetVal rsReloadCandidateBuildNormalizedGraphV1(const rsReloadCandidate_t *candidate,
                                                 rsReloadNormalizedGraphBuilderV1_t **ppBuilder);
rsRetVal rsReloadObjectSyntaxFingerprintV1(const struct cnfobj *object, char **ownedFingerprint);
rsRetVal rsReloadActionSyntaxFingerprintV1(const struct nvlst *syntax, char **ownedFingerprint);
rsRetVal rsReloadActionSyntaxNameV1(const struct nvlst *syntax, const char **borrowedName, size_t *nameLength);
/* Build a private, frontend-neutral view of one string-valued global
 * parameter. ownedValue is NULL when the parameter is omitted. The second
 * output fingerprints the effective last-write map of every other global
 * parameter, so callers can authorize one narrow base change without
 * accidentally accepting an unrelated global mutation. */
rsRetVal rsReloadCandidateGlobalStringProfileV1(const rsReloadCandidate_t *candidate,
                                                const char *parameterName,
                                                char **ownedValue,
                                                char **ownedOtherFingerprint);

/*
 * Conservative scope gate for the first live-ruleset milestone.
 * It only accepts a report whose changed nodes are existing rulesets; action
 * nodes must be byte-for-byte unchanged. It builds no runtime objects; a
 * later private compiler still has to validate and prepare the accepted AST.
 */
rsRetVal rsReloadCandidateCheckRulesetOnlyReportV1(const rsReloadReportV1_t *report);
rsRetVal rsReloadCandidateCheckRulesetImtcpReportV1(const rsReloadCandidate_t *activeSourceCatalog,
                                                    const rsReloadCandidate_t *candidate,
                                                    const rsReloadReportV1_t *report);
typedef enum rsReloadCandidateAuthorizationV1_e {
    RS_RELOAD_AUTHORIZE_IMTCP_V1 = 1U << 0,
    RS_RELOAD_AUTHORIZE_BASE_V1 = 1U << 1,
    RS_RELOAD_AUTHORIZE_RELOAD_MODE_V1 = RS_RELOAD_AUTHORIZE_BASE_V1
} rsReloadCandidateAuthorizationV1_t;
/* Ruleset modifications remain the baseline capability. Additional report
 * object kinds are accepted only when the caller has already prepared and
 * authorized the corresponding private runtime transition. */
rsRetVal rsReloadCandidateCheckAuthorizedReportV1(const rsReloadCandidate_t *activeSourceCatalog,
                                                  const rsReloadCandidate_t *candidate,
                                                  const rsReloadReportV1_t *report,
                                                  unsigned authorizations);

/*
 * Normal startup observes the already-parsed source once, before regular
 * cnfobj dispatch consumes it. The finished builder is owned by the caller;
 * this records no runtime pointers and never reads the configuration again.
 */
rsRetVal rsReloadCandidateSourceBegin(void);
int rsReloadCandidateSourceActive(void);
void rsReloadCandidateSourceCaptureObject(const struct cnfobj *object);
void rsReloadCandidateSourceCaptureDefaultScript(const struct cnfstmt *script);
rsRetVal rsReloadCandidateSourceFinish(rsReloadNormalizedGraphBuilderV1_t **ppBuilder,
                                       rsReloadCandidate_t **ppObjectCatalog);
void rsReloadCandidateSourceAbort(void);
void rsReloadCandidateSourceNoteError(void);

/* Parser integration. These functions are meaningful only while
 * rsReloadCandidateParse() owns the single-threaded parser. */
int rsReloadCandidateCaptureActive(void);
rsRetVal rsReloadCandidateTakeObject(struct cnfobj *object);
rsRetVal rsReloadCandidateTakeDefaultScript(struct cnfstmt *script);
void rsReloadCandidateNoteParseError(void);
void rsReloadCandidateNoteError(rsRetVal error);

#endif /* RELOAD_CANDIDATE_H_INCLUDED */
