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
rsRetVal rsReloadActionSyntaxFingerprintV1(const struct nvlst *syntax, char **ownedFingerprint);
rsRetVal rsReloadActionSyntaxNameV1(const struct nvlst *syntax, const char **borrowedName, size_t *nameLength);

/*
 * Conservative scope gate for the first live-ruleset milestone.
 * It only accepts a report whose changed nodes are existing rulesets; action
 * nodes must be byte-for-byte unchanged. It builds no runtime objects; a
 * later private compiler still has to validate and prepare the accepted AST.
 */
rsRetVal rsReloadCandidateCheckRulesetOnlyReportV1(const rsReloadReportV1_t *report);

/*
 * Normal startup observes the already-parsed source once, before regular
 * cnfobj dispatch consumes it. The finished builder is owned by the caller;
 * this records no runtime pointers and never reads the configuration again.
 */
rsRetVal rsReloadCandidateSourceBegin(void);
int rsReloadCandidateSourceActive(void);
void rsReloadCandidateSourceCaptureObject(const struct cnfobj *object);
void rsReloadCandidateSourceCaptureDefaultScript(const struct cnfstmt *script);
rsRetVal rsReloadCandidateSourceFinish(rsReloadNormalizedGraphBuilderV1_t **ppBuilder);
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
