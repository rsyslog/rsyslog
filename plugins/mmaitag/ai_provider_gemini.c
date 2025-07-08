/**
 * @file ai_provider_gemini.c
 * @brief An AI provider that uses the Google Gemini REST API.
 *
 * This file provides a concrete implementation of the ai_provider.h
 * interface. It handles the specifics of formatting requests for
 * and parsing responses from the Google Gemini API to classify
 * log messages.
 */
#include "config.h"
#include "ai_provider.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>      /* for sleep() */
#include <ctype.h>       /* for isspace() */
#include <curl/curl.h>
#include <json.h>

/**
 * @brief Holds the private state for the Gemini provider.
 *
 * This struct contains all provider-specific configuration, such as
 * the model name and API key. An instance of this is stored in the
 * generic `ai_provider_t->data` void pointer.
 */
typedef struct gemini_data_s {
	char *model;
	char *apikey;
	char *prompt;
} gemini_data_t;

/**
 * @brief A simple buffer to accumulate the HTTP response from libcurl.
 */
struct curl_resp {
	char *buf;
	size_t len;
};

/**
 * @brief Strips trailing whitespace from a string in-place.
 *
 * @note This is a critical step because LLMs often add trailing
 * newlines or other whitespace to their responses. Client-side
 * cleaning is necessary for robust parsing.
 *
 * @param[in,out] str The string to be trimmed.
 */
static void
strip_trailing_whitespace(char *str)
{
	if (str == NULL) return;

	// Strip any true whitespace characters, including LF, CR, space, etc.
	int len = strlen(str);
	while (len > 0 && isspace((unsigned char)str[len - 1])) {
		str[--len] = '\0';
	}
}


/**
 * @brief libcurl callback function for writing received data.
 * @see CURLOPT_WRITEFUNCTION
 *
 * This function is called by libcurl whenever new data is received from
 * the server. It appends the new data chunk to our curl_resp buffer.
 */
static size_t
write_cb(char *ptr, size_t size, size_t nmemb, void *data)
{
	struct curl_resp *r = (struct curl_resp*)data;
	size_t tot = size * nmemb;
	char *tmp = realloc(r->buf, r->len + tot + 1);
	if(tmp == NULL)
		return 0;
	r->buf = tmp;
	memcpy(r->buf + r->len, ptr, tot);
	r->len += tot;
	r->buf[r->len] = '\0';
	return tot;
}

/**
 * @brief Implements the ai_provider_cleanup_t interface function.
 *
 * Frees all resources allocated by this provider, namely the
 * private gemini_data_t state object.
 */
static void
gemini_cleanup(ai_provider_t *prov)
{
	gemini_data_t *d = (gemini_data_t*)prov->data;
	if(d == NULL)
		return;
	free(d->model);
	free(d->apikey);
	free(d->prompt);
	free(d);
	prov->data = NULL;
}

/**
 * @brief Implements the ai_provider_init_t interface function.
 *
 * Allocates and populates the provider-specific gemini_data_t state
 * struct and attaches it to the generic provider's `data` pointer.
 *
 * @param[in] prov The generic provider instance to initialize.
 */
static rsRetVal
gemini_init(ai_provider_t *prov, const char *model, const char *key, const char *prompt)
{
	gemini_data_t *d;
	DEFiRet;
	d = calloc(1, sizeof(*d));
	if(d == NULL)
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	if(model)  d->model = strdup(model);
	if(key)    d->apikey = strdup(key);
	if(prompt) d->prompt = strdup(prompt);
	prov->data = d;
	prov->cleanup = gemini_cleanup;
finalize_it:
	RETiRet;
}

/**
 * @brief Implements the ai_provider_classify_t interface function for Gemini.
 *
 * @note This function iterates through the message batch and sends
 * **one API request per message**. This is inefficient for production
 * but suitable for a proof-of-concept. A future optimization
 * would be to construct a single request for the entire batch.
 *
 * @note The prompt is constructed by combining the configured initial_prompt
 * with the log message, separated by a newline.
 *
 * @param[in]  prov The provider instance, used to access private data.
 * @param[in]  msgs Array of C-strings, each a log message to classify.
 * @param[in]  n    The number of messages in the `msgs` array.
 * @param[out] tags A pointer to an array of C-strings. This function will
 * allocate this array and fill it with the resulting tags.
 * The caller is responsible for freeing the array and each
 * string within it.
 */
static rsRetVal
gemini_classify_batch(ai_provider_t *prov, const char **msgs, size_t n, char ***tags)
{
	gemini_data_t *d = (gemini_data_t*)prov->data;
	DEFiRet;
	char *url = NULL;
	CURL *curl = NULL;

	if(d == NULL || d->apikey == NULL)
		ABORT_FINALIZE(RS_RET_ERR);

	CHKmalloc(*tags = calloc(n, sizeof(char*)));

	curl = curl_easy_init();
	if(curl == NULL)
		ABORT_FINALIZE(RS_RET_ERR);

	if(asprintf(&url,
		"https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent",
		d->model ? d->model : "gemini-2.0-flash") == -1) {
		ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
	}

	for (size_t i = 0; i < n; ++i) {
		struct curl_slist *headers = NULL;
		struct curl_resp resp = {0};
		struct json_object *req = NULL;
		char *full_prompt = NULL;
		char *api_key_header = NULL;

		if (asprintf(&full_prompt, "%s\n%s", d->prompt ? d->prompt : "", msgs[i]) == -1) {
			(*tags)[i] = strdup("REGULAR");
			continue;
		}
		
		// ✅ Dynamically allocate header to prevent buffer overflow
		if (asprintf(&api_key_header, "x-goog-api-key: %s", d->apikey) == -1) {
			(*tags)[i] = strdup("REGULAR");
			free(full_prompt);
			continue;
		}

		req = json_object_new_object();
		struct json_object *contents = json_object_new_array();
		struct json_object *user_turn = json_object_new_object();
		struct json_object *parts = json_object_new_array();
		struct json_object *text_part = json_object_new_object();

		json_object_object_add(text_part, "text", json_object_new_string(full_prompt));
		json_object_array_add(parts, text_part);
		json_object_object_add(user_turn, "role", json_object_new_string("user"));
		json_object_object_add(user_turn, "parts", parts);
		json_object_array_add(contents, user_turn);
		json_object_object_add(req, "contents", contents);

		const char *body = json_object_to_json_string(req);

		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
		
		headers = curl_slist_append(NULL, "Content-Type: application/json");
		headers = curl_slist_append(headers, api_key_header);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

		long http_code = 0;
		CURLcode cc = curl_easy_perform(curl);
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

		if (cc == CURLE_OK && http_code == 200) {
			struct json_object *parsed = json_tokener_parse(resp.buf);
			struct json_object *candidates = NULL;
			struct json_object *first_candidate = NULL;
			struct json_object *content = NULL;
			struct json_object *parts_array = NULL;
			struct json_object *part_obj = NULL;
			struct json_object *text_obj = NULL;
			const char *raw_tag = NULL;

			// This chain of checks ensures we don't dereference a NULL pointer.
			if (parsed &&
				json_object_object_get_ex(parsed, "candidates", &candidates) &&
				(first_candidate = json_object_array_get_idx(candidates, 0)) != NULL &&
				json_object_object_get_ex(first_candidate, "content", &content) &&
				json_object_object_get_ex(content, "parts", &parts_array) &&
				(part_obj = json_object_array_get_idx(parts_array, 0)) != NULL &&
				json_object_object_get_ex(part_obj, "text", &text_obj)) {

				raw_tag = json_object_get_string(text_obj);
			}

			if (raw_tag) {
				char *mutable_tag = strdup(raw_tag);
				strip_trailing_whitespace(mutable_tag);
				(*tags)[i] = mutable_tag;
			} else {
				(*tags)[i] = strdup("REGULAR");
			}

			if (parsed) {
				json_object_put(parsed);
			}
		} else {
			(*tags)[i] = strdup("REGULAR");
			char err_details[1024];
			snprintf(err_details, sizeof(err_details), "HTTP %ld - %s", http_code, resp.buf ? resp.buf : curl_easy_strerror(cc));
			LogError(0, RS_RET_ERR, "mmaitag: gemini request for a message failed: %s", err_details);
		}

		// Cleanup for this loop iteration
		curl_slist_free_all(headers);
		free(api_key_header); // ✅ Free the dynamically allocated header
		free(full_prompt);
		free(resp.buf);
		json_object_put(req);
	}

finalize_it:
	if(curl)
		curl_easy_cleanup(curl);
	free(url);
	RETiRet;
}


/**
 * @brief The global instance of the Gemini provider.
 *
 * This struct "plugs" the static gemini_* functions into the interface
 * defined in ai_provider.h. It is declared `extern` in `ai_provider.h`
 * so that mmaitag.c can find and use it at runtime.
 */
ai_provider_t gemini_provider = {
	.data     = NULL,
	.init     = gemini_init,
	.classify = gemini_classify_batch,
	.cleanup  = gemini_cleanup
};
