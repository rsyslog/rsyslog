/** SPDX-License-Identifier: Apache-2.0
 * @file reload-candidate.h
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
/** @brief Opaque owner of private parser objects, never an active runtime. */
typedef struct rsReloadCandidate_s rsReloadCandidate_t;
/** @brief Inspect an immutable object in frontend order; never retain its pointers. */
typedef rsRetVal (*rsReloadCandidateObjectVisitorV1_t)(const struct cnfobj *object, size_t parseOrdinal, void *context);
/** @brief Visit borrowed immutable syntax in effective parse order.
 * Identity and fragment are valid only during the callback; neither may be
 * mutated or retained. A non-OK result stops traversal and propagates unchanged.
 * firstForCanonicalIdentity distinguishes repeated fragments of one ruleset;
 * isSyntheticDefault identifies top-level statements, not a named ruleset.
 */
typedef rsRetVal (*rsReloadCandidateRulesetFragmentVisitorV1_t)(const char *canonicalIdentity,
                                                                const struct cnfstmt *fragment,
                                                                int firstForCanonicalIdentity,
                                                                int isSyntheticDefault,
                                                                void *context);

/**
 * @brief Parse a regular RainerScript or YAML file without runtime dispatch.
 * @param path Borrowed filename; used only during this call.
 * @param ppCandidate Non-NULL output, initially NULL. Receives ownership only
 * on success; release with rsReloadCandidateDestruct().
 * @return RS_RET_OK, RS_RET_PARAM_ERROR for invalid arguments or nested capture,
 * a file/I/O error, RS_RET_OUT_OF_MEMORY, or RS_RET_CONF_PARSE_ERROR.
 *
 * Caller must serialize access to the process-global parser. Lexer state is
 * reset after parsing, including failure; fatal include errors propagate to
 * the caller rather than exiting the process. No candidate escapes on error.
 */
rsRetVal rsReloadCandidateParse(const char *path, rsReloadCandidate_t **ppCandidate);
/** @brief Free all owned syntax and clear the pointer; NULL inputs are safe. */
void rsReloadCandidateDestruct(rsReloadCandidate_t **ppCandidate);
/** @brief Return captured object count without transferring ownership; NULL yields zero. */
size_t rsReloadCandidateObjectCount(const rsReloadCandidate_t *candidate);
/** @brief Visit catalog objects in frontend parse order.
 * Objects are borrowed for the callback only and retain frontend parse order.
 * The visitor must not mutate, destruct, or retain them.
 * @param candidate Borrowed syntax capture or detached object catalog.
 * @param visitor Callback; its first non-OK result stops traversal.
 * @param context Caller-owned callback context, never retained.
 * @return RS_RET_OK, visitor error, or RS_RET_PARAM_ERROR for invalid arguments.
 */
rsRetVal rsReloadCandidateVisitObjectsV1(const rsReloadCandidate_t *candidate,
                                         rsReloadCandidateObjectVisitorV1_t visitor,
                                         void *context);
/** @brief Build an independent ordered catalog containing global, module, input and named rate-policy
 * declarations. This is the source material required by module-specific
 * effective-profile classifiers; unrelated scripts and objects are omitted.
 * Owned reference values retain non-imtcp policy usage after script disposal.
 * Ordered global blocks retain startup's last-write parameter semantics.
 * @param candidate Borrowed syntax capture, not consumed.
 * @param ppCatalog Non-NULL address initially containing NULL; output is owned
 * by the caller and released with rsReloadCandidateDestruct().
 * @return RS_RET_OK on success, or an allocation/clone error.
 */
rsRetVal rsReloadCandidateBuildObjectCatalogV1(const rsReloadCandidate_t *candidate, rsReloadCandidate_t **ppCatalog);
/** @brief Traverse candidate ruleset fragments without consuming the candidate.
 * @param candidate Immutable candidate, kept alive by the caller throughout.
 * @param visitor Callback with borrowed syntax; must not alter candidate state.
 * @param context Caller-owned callback context.
 * @return RS_RET_OK, or the first callback/argument error. No internal lock is
 * taken; callers must serialize destruction and mutation against traversal.
 */
rsRetVal rsReloadCandidateVisitRulesetFragmentsV1(const rsReloadCandidate_t *candidate,
                                                  rsReloadCandidateRulesetFragmentVisitorV1_t visitor,
                                                  void *context);

/**
 * @brief Build a structural graph from a borrowed, successfully parsed candidate.
 * Convert the private, frontend-neutral cnfobj/nvlst capture into an owned
 * normalized graph.  This is deliberately a control-path API: it retains no
 * runtime object or module pointers and reads the shared capture objects
 * produced by either frontend. Cross-frontend semantic equivalence still
 * requires integration-level parity tests before activation. The graph is
 * structural evidence only, not semantic validation or an activation
 * decision. Explicit named rulesets emit separate program and metadata rows;
 * metadata records immutable attributes using the case-folded registry name.
 * @param candidate Borrowed capture, unchanged by this call.
 * @param ppBuilder Non-NULL output, initially NULL; receives caller-owned
 * builder on success, to free with rsReloadNormalizedGraphBuilderV1Destruct().
 * @return RS_RET_OK; RS_RET_PARAM_ERROR for invalid arguments or dirty capture;
 * otherwise an allocation, normalization, or unsupported-syntax error. With a
 * valid output slot, failure leaves it NULL and releases partial builder state;
 * a non-NULL existing output is preserved and rejected as a parameter error.
 */
rsRetVal rsReloadCandidateBuildNormalizedGraphV1(const rsReloadCandidate_t *candidate,
                                                 rsReloadNormalizedGraphBuilderV1_t **ppBuilder);
/** @brief Serialize and hash action source parameters without instantiation.
 * @param syntax Borrowed immutable parameter list.
 * @param ownedFingerprint Initially NULL output; caller frees the returned hash.
 * @return RS_RET_OK or a serialization/allocation error; output stays NULL on
 * failure. Caller serializes access to syntax; no runtime/module locks are used.
 */
rsRetVal rsReloadActionSyntaxFingerprintV1(const struct nvlst *syntax, char **ownedFingerprint);
/** @brief Copy an immutable object's canonical fingerprint into caller-owned storage. */
rsRetVal rsReloadObjectSyntaxFingerprintV1(const struct cnfobj *object, char **ownedFingerprint);
/** @brief Borrow an explicit action name from immutable source parameters.
 * @param syntax Borrowed parameter list, which must outlive the returned bytes.
 * @param borrowedName Output bytes, not necessarily NUL terminated.
 * @param nameLength Output byte length.
 * @return RS_RET_NOT_IMPLEMENTED for an unnamed action, otherwise argument status
 * or RS_RET_OK. No allocation, ownership transfer, or internal locking occurs.
 */
rsRetVal rsReloadActionSyntaxNameV1(const struct nvlst *syntax, const char **borrowedName, size_t *nameLength);
/** @brief Return an owned global string and fingerprint excluding only that parameter.
 * Omitted values are NULL. Ordered global blocks use startup's last-write map.
 * Both output slots must initially be NULL; caller frees successful outputs.
 */
rsRetVal rsReloadCandidateGlobalStringProfileV1(const rsReloadCandidate_t *candidate,
                                                const char *parameterName,
                                                char **ownedValue,
                                                char **ownedOtherFingerprint);

/**
 * @brief Check a borrowed V1 report against the initial ruleset-only scope.
 * Conservative scope gate for the first live-ruleset milestone.
 * It only accepts a report whose changed nodes are existing rulesets; action
 * nodes must be byte-for-byte unchanged. It builds no runtime objects; a
 * later private compiler still has to validate and prepare the accepted AST.
 * An unchanged report also passes; this never authorizes activation.
 * @param report Borrowed, well-formed V1 report with contiguous V1 entries.
 * @return RS_RET_OK for allowed scope, RS_RET_NOT_IMPLEMENTED for unsupported
 * changes, or RS_RET_PARAM_ERROR for a NULL/incompatible report header.
 */
rsRetVal rsReloadCandidateCheckRulesetOnlyReportV1(const rsReloadReportV1_t *report);
/** @brief Admit retained imtcp changes and exclusively owned named-policy mutations.
 * @param activeSourceCatalog Borrowed active catalog with owned non-imtcp references;
 * required for any named-policy mutation, otherwise NULL is accepted.
 * @param candidate Borrowed complete candidate syntax.
 * @param report Borrowed structurally valid report; no ownership is transferred.
 * @return RS_RET_OK for the bounded scope; parameter or unsupported-scope error otherwise.
 * Source authorization is not semantic validation or permission to bypass
 * topology, local/mixed queue, module classification or preparation guards.
 */
rsRetVal rsReloadCandidateCheckRulesetImtcpReportV1(const rsReloadCandidate_t *activeSourceCatalog,
                                                    const rsReloadCandidate_t *candidate,
                                                    const rsReloadReportV1_t *report);
/** @brief Private preparations that authorize otherwise unsupported report kinds. */
typedef enum rsReloadCandidateAuthorizationV1_e {
    RS_RELOAD_AUTHORIZE_IMTCP_V1 = 1U << 0, /**< Classified existing-listener/named-policy transition. */
    RS_RELOAD_AUTHORIZE_BASE_V1 = 1U << 1, /**< Independently validated single global scalar. */
    RS_RELOAD_AUTHORIZE_RULESET_ADDITIONS_V1 = 1U << 2 /**< Complete private shells, name-only metadata and fence. */
} rsReloadCandidateAuthorizationV1_t;
/* Ruleset modifications remain the baseline capability. Additional report
 * object kinds are accepted only when the caller has already prepared and
 * authorized the corresponding private runtime transition. */
rsRetVal rsReloadCandidateCheckAuthorizedReportV1(const rsReloadCandidate_t *activeSourceCatalog,
                                                  const rsReloadCandidate_t *candidate,
                                                  const rsReloadReportV1_t *report,
                                                  unsigned authorizations);

/**
 * @brief Begin single-threaded startup observation, discarding any prior capture.
 * Normal startup observes the already-parsed source once, before regular
 * cnfobj dispatch consumes it. The finished builder is owned by the caller;
 * this records no runtime pointers and never reads the configuration again.
 * @return RS_RET_OK or an allocation failure. Later capture failures are latched
 * and reported by rsReloadCandidateSourceFinish(), not by process termination.
 */
rsRetVal rsReloadCandidateSourceBegin(void);
/** @brief Return nonzero only while a clean startup source capture is active. */
int rsReloadCandidateSourceActive(void);
/** @brief Observe a borrowed object before dispatch consumes it; retain no AST pointers. */
void rsReloadCandidateSourceCaptureObject(const struct cnfobj *object);
/** @brief Observe a borrowed default script; serialization failures dirty the capture. */
void rsReloadCandidateSourceCaptureDefaultScript(const struct cnfstmt *script);
/**
 * @brief Finish startup observation and transfer the owned structural builder.
 * @param ppBuilder Non-NULL output, initially NULL; caller frees a successful
 * result with rsReloadNormalizedGraphBuilderV1Destruct().
 * @param ppObjectCatalog Non-NULL initially NULL output; receives an owned
 * source catalog on success, released with rsReloadCandidateDestruct().
 * @return RS_RET_OK; RS_RET_PARAM_ERROR for invalid output/inactive capture;
 * RS_RET_NOT_IMPLEMENTED for a dirty baseline; otherwise allocation/graph errors.
 * Valid-output failures discard capture state and leave the output NULL.
 */
rsRetVal rsReloadCandidateSourceFinish(rsReloadNormalizedGraphBuilderV1_t **ppBuilder,
                                       rsReloadCandidate_t **ppObjectCatalog);
/** @brief Release unfinished startup observation; safe when no capture exists. */
void rsReloadCandidateSourceAbort(void);
/** @brief Mark startup observation unusable; a no-op outside source capture. */
void rsReloadCandidateSourceNoteError(void);

/** @brief Test whether rsReloadCandidateParse() owns the single-threaded parser. */
int rsReloadCandidateCaptureActive(void);
/**
 * @brief Append an object to the active private capture.
 * @param object Ownership transfers only on RS_RET_OK, including its script.
 * @return RS_RET_OK, RS_RET_PARAM_ERROR for no active capture/NULL object, or
 * RS_RET_OUT_OF_MEMORY. Caller retains and must free the object on failure.
 */
rsRetVal rsReloadCandidateTakeObject(struct cnfobj *object);
/**
 * @brief Wrap a default script in a privately owned ruleset object.
 * @param script Ownership transfers only on RS_RET_OK; caller retains it on error.
 * @return RS_RET_OK, RS_RET_PARAM_ERROR for no active capture/NULL script, or
 * RS_RET_OUT_OF_MEMORY. Only for the serialized candidate parser path.
 */
rsRetVal rsReloadCandidateTakeDefaultScript(struct cnfstmt *script);
/** @brief Latch syntax failure for the active candidate; otherwise a no-op. */
void rsReloadCandidateNoteParseError(void);
/**
 * @brief Latch an error for return by rsReloadCandidateParse(), never exiting.
 * @param error Error to retain; resource errors take precedence over file/I/O
 * errors, which take precedence over other errors. RS_RET_OK is ignored.
 * No-op when no candidate capture is active; parser access must be serialized.
 */
void rsReloadCandidateNoteError(rsRetVal error);

#endif /* RELOAD_CANDIDATE_H_INCLUDED */
