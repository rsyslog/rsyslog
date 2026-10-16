/* SPDX-License-Identifier: Apache-2.0
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
/** @brief Opaque owner of private parser objects, never an active runtime. */
typedef struct rsReloadCandidate_s rsReloadCandidate_t;

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

/**
 * @brief Build a structural graph from a borrowed, successfully parsed candidate.
 * Convert the private, frontend-neutral cnfobj/nvlst capture into an owned
 * normalized graph.  This is deliberately a control-path API: it retains no
 * runtime object or module pointers and reads the shared capture objects
 * produced by either frontend. Cross-frontend semantic equivalence still
 * requires integration-level parity tests before activation. The graph is
 * structural evidence only, not semantic validation or an activation
 * decision.
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
 * @return RS_RET_OK; RS_RET_PARAM_ERROR for invalid output/inactive capture;
 * RS_RET_NOT_IMPLEMENTED for a dirty baseline; otherwise allocation/graph errors.
 * Valid-output failures discard capture state and leave the output NULL.
 */
rsRetVal rsReloadCandidateSourceFinish(rsReloadNormalizedGraphBuilderV1_t **ppBuilder);
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
