/* reload-candidate.h
 *
 * Side-effect-free syntax capture for a reload candidate. This is control
 * path code: it never publishes runtime objects or touches message flow.
 */
#ifndef RELOAD_CANDIDATE_H_INCLUDED
#define RELOAD_CANDIDATE_H_INCLUDED 1

#include <stddef.h>

#include "rsyslog.h"

struct cnfobj;
struct cnfstmt;
typedef struct rsReloadCandidate_s rsReloadCandidate_t;

rsRetVal rsReloadCandidateParse(const char *path, rsReloadCandidate_t **ppCandidate);
void rsReloadCandidateDestruct(rsReloadCandidate_t **ppCandidate);
size_t rsReloadCandidateObjectCount(const rsReloadCandidate_t *candidate);

/* Parser integration. These functions are meaningful only while
 * rsReloadCandidateParse() owns the single-threaded parser. */
int rsReloadCandidateCaptureActive(void);
rsRetVal rsReloadCandidateTakeObject(struct cnfobj *object);
rsRetVal rsReloadCandidateTakeDefaultScript(struct cnfstmt *script);
void rsReloadCandidateNoteParseError(void);

#endif /* RELOAD_CANDIDATE_H_INCLUDED */
