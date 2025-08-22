/**
 * @file ai_provider_gemini_mock.c
 * @brief Mock provider used for mmaitag tests.
 *
 * This provider implements the ai_provider.h interface but does not
 * contact any external service. It is designed to make test runs
 * deterministic by returning predefined labels.
 */
#include "config.h"
#include "ai_provider.h"
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json.h>

/**
 * @brief Fulfills the private data requirement for the provider interface.
 *
 * While the mock classification logic does not use this state, it is
 * allocated and freed to ensure the provider interface is correctly
 * exercised during tests.
 */
typedef struct gemini_data_s {
    char *model;
    char *apikey;
    char *prompt;
} gemini_data_t;

/**
 * @brief Implements the standard cleanup function for the mock provider.
 */
static void gemini_cleanup(ai_provider_t *prov) {
    gemini_data_t *data = (gemini_data_t *)prov->data;
    if (data == NULL) return;
    free(data->model);
    free(data->apikey);
    free(data->prompt);
    free(data);
    prov->data = NULL;
}

/**
 * @brief Implements the standard init function for the mock provider.
 */
static rsRetVal gemini_init(ai_provider_t *prov, const char *model, const char *apikey, const char *prompt) {
    gemini_data_t *data;
    DEFiRet;
    CHKmalloc(data = calloc(1, sizeof(*data)));
    if (model) data->model = strdup(model);
    if (apikey) data->apikey = strdup(apikey);
    if (prompt) data->prompt = strdup(prompt);
    prov->data = data;
    prov->cleanup = gemini_cleanup;
finalize_it:
    RETiRet;
}

/**
 * @brief Mock implementation of the ai_provider_classify_t function.
 *
 * @note This function simulates an API call by parsing a comma-separated
 * string from the `GEMINI_MOCK_RESPONSE` environment variable
 * (e.g., "NOISE,CRITICAL,REGULAR").
 *
 * @note It is stateful across calls within a single test run. It uses a
 * static counter to return subsequent tags from the list on
 * subsequent calls, allowing for testing of sequential message
 * processing.
 *
 * @note If the environment variable is not set or runs out of tags, it
 * returns "REGULAR" as a default fallback value.
 */
static rsRetVal gemini_classify_batch(ai_provider_t *prov, const char **messages, size_t n, char ***tags) {
    static size_t next = 0;
    const char *mock = getenv("GEMINI_MOCK_RESPONSE");
    (void)prov;
    (void)messages;
    char *tmp = NULL;
    char *tok = NULL;
    char *saveptr = NULL;
    char **out = NULL;
    size_t idx = 0;
    DEFiRet;

    if (mock == NULL) {
        LogError(0, RS_RET_ERR,
                 "mmaitag: mock provider requires "
                 "GEMINI_MOCK_RESPONSE environment variable to be set.");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    CHKmalloc(tmp = strdup(mock));

    // Advance to the correct starting position in the token list
    tok = strtok_r(tmp, ",", &saveptr);
    for (size_t i = 0; i < next && tok != NULL; ++i) tok = strtok_r(NULL, ",", &saveptr);

    CHKmalloc(out = calloc(n, sizeof(char *)));

    for (idx = 0; idx < n; ++idx) {
        if (tok != NULL) {
            CHKmalloc(out[idx] = strdup(tok));
            tok = strtok_r(NULL, ",", &saveptr);
        } else {
            // Fallback if we run out of mock tags
            CHKmalloc(out[idx] = strdup("REGULAR"));
        }
    }
    next += n;  // Update state for the next call

    *tags = out;
    out = NULL;

finalize_it:
    if (out != NULL) {
        for (idx = 0; idx < n; ++idx) {
            free(out[idx]);
        }
        free(out);
    }
    free(tmp);
    RETiRet;
}

/**
 * @brief The global instance of the mock Gemini provider.
 *
 * This struct plugs the mock functions into the ai_provider.h interface
 * for use in testing environments.
 */
ai_provider_t gemini_mock_provider = {
    .data = NULL, .init = gemini_init, .classify = gemini_classify_batch, .cleanup = gemini_cleanup};
