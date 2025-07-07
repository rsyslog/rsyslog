/**
 * @file ai_provider.h
 * @brief Interface for AI classification providers.
 *
 * Providers implement the functions defined here so that the
 * mmaitag module can call out to different backends. Each provider
 * may maintain private state which is pointed to by the `data`
 * member in the structure.
 */
#ifndef AI_PROVIDER_H
#define AI_PROVIDER_H
#include "rsyslog.h"

typedef struct ai_provider_s ai_provider_t;

typedef rsRetVal (*ai_provider_init_t)(ai_provider_t *prov, const char *model,
const char *apikey, const char *prompt);
typedef rsRetVal (*ai_provider_classify_t)(ai_provider_t *prov,
const char **messages, size_t n, char ***tags);
typedef void (*ai_provider_cleanup_t)(ai_provider_t *prov);

struct ai_provider_s {
	void *data;
	ai_provider_init_t init;
	ai_provider_classify_t classify;
	ai_provider_cleanup_t cleanup;
};

extern ai_provider_t gemini_provider;
extern ai_provider_t gemini_mock_provider;

#endif /* AI_PROVIDER_H */
