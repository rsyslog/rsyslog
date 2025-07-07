#include "config.h"
#include "ai_provider.h"
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json.h>

typedef struct gemini_data_s {
	char *model;
	char *apikey;
	char *prompt;
} gemini_data_t;

static void
gemini_cleanup(ai_provider_t *prov)
{
	gemini_data_t *data = (gemini_data_t*)prov->data;
	if(data == NULL)
		return;
	free(data->model);
	free(data->apikey);
	free(data->prompt);
	free(data);
	prov->data = NULL;
}

static rsRetVal
gemini_init(ai_provider_t *prov, const char *model, const char *apikey,
	const char *prompt)
{
	gemini_data_t *data;
	DEFiRet;
       CHKmalloc(data = calloc(1, sizeof(*data)));
	if(model)
		data->model = strdup(model);
	if(apikey)
		data->apikey = strdup(apikey);
	if(prompt)
		data->prompt = strdup(prompt);
	prov->data = data;
       prov->cleanup = gemini_cleanup;
finalize_it:
       RETiRet;
}

static rsRetVal
gemini_classify_batch(ai_provider_t *prov, const char **messages, size_t n,
       char ***tags)
{
       const char *mock = getenv("GEMINI_MOCK_RESPONSE");
       (void)prov;
       (void)messages;
	char *tmp;
	   char *tok;
	   char *saveptr = NULL;
	char **out;
	size_t idx = 0;
	DEFiRet;
	if(mock == NULL)
		return RS_RET_ERR;
	CHKmalloc(tmp = strdup(mock));
       CHKmalloc(out = calloc(n, sizeof(char*)));
	   tok = strtok_r(tmp, ",", &saveptr);
	   while(tok != NULL && idx < n) {
	       out[idx++] = strdup(tok);
	       tok = strtok_r(NULL, ",", &saveptr);
	   }
	while(idx < n)
	out[idx++] = strdup("REGULAR");
	free(tmp);
	*tags = out;
finalize_it:
	RETiRet;
}

ai_provider_t gemini_mock_provider = {
	.data = NULL,
	.init = gemini_init,
	.classify = gemini_classify_batch,
	.cleanup = gemini_cleanup
};
