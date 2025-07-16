/**
 * @file ai_provider.h
 * @brief Defines the interface for AI classification providers.
 *
 * This header defines an abstraction layer for different AI backends.
 * The mmaitag module uses this interface to classify messages without
 * needing to know the specifics of the chosen AI service (e.g., Gemini,
 * OpenAI). Each provider implements the functions defined here and
 * exposes its implementation via a global ai_provider_t struct.
 */
#ifndef AI_PROVIDER_H
#define AI_PROVIDER_H

#include "rsyslog.h"

typedef struct ai_provider_s ai_provider_t;

/**
 * @brief Function pointer type for initializing a provider.
 *
 * @param[in] prov The provider instance to initialize.
 * @param[in] model The specific AI model name to use.
 * @param[in] apikey The API key for service authentication.
 * @param[in] prompt The base system/initial prompt to use for classification.
 * @return RS_RET_OK on success, or an error code.
 */
typedef rsRetVal (*ai_provider_init_t)(ai_provider_t *prov, const char *model, const char *apikey, const char *prompt);

/**
 * @brief Function pointer type for classifying a batch of messages.
 *
 * @param[in]  prov The provider instance.
 * @param[in]  messages An array of message strings to classify.
 * @param[in]  n The number of messages in the array.
 * @param[out] tags A pointer to a string array that will be allocated by the
 * implementation and filled with the resulting classification tags.
 *
 * @note The provider's implementation MUST allocate memory for the `tags`
 * array and each string within it. The caller is responsible for
 * freeing this memory.
 *
 * @return RS_RET_OK on success, or an error code.
 */
typedef rsRetVal (*ai_provider_classify_t)(ai_provider_t *prov, const char **messages, size_t n, char ***tags);

/**
 * @brief Function pointer type for cleaning up a provider's resources.
 *
 * @param[in] prov The provider instance to clean up.
 */
typedef void (*ai_provider_cleanup_t)(ai_provider_t *prov);


/**
 * @brief The central struct defining a provider's implementation.
 */
struct ai_provider_s {
    void *data;  ///< Pointer to provider-specific private state data.
    ai_provider_init_t init;  ///< Pointer to the provider's init function.
    ai_provider_classify_t classify;  ///< Pointer to the provider's classification function.
    ai_provider_cleanup_t cleanup;  ///< Pointer to the provider's cleanup function.
};

/*
 * Extern declarations for the global provider instances.
 * These make the provider singletons, defined in their respective .c files,
 * available to the main mmaitag module.
 */
extern ai_provider_t gemini_provider;
extern ai_provider_t gemini_mock_provider;

#endif /* AI_PROVIDER_H */
