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

struct curl_resp {
	char *buf;
	size_t len;
};

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
	r->buf[r->len] = "\0";
	return tot;
}

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

static rsRetVal
gemini_init(ai_provider_t *prov, const char *model, const char *key, const char *prompt)
{
	gemini_data_t *d;
	DEFiRet;
	CHKiRet(rsCAlloc((void**)&d, sizeof(*d)));
	if(model) d->model = strdup(model);
	if(key) d->apikey = strdup(key);
	if(prompt) d->prompt = strdup(prompt);
	prov->data = d;
	prov->cleanup = gemini_cleanup;
	RETiRet;
}

static rsRetVal
gemini_classify_batch(ai_provider_t *prov, const char **msgs, size_t n, char ***tags)
{
	gemini_data_t *d = (gemini_data_t*)prov->data;
	DEFiRet;
	struct curl_resp resp = {0};
	char *url = NULL;
	CURL *curl;
	struct json_object *req = NULL;
	if(d == NULL || d->apikey == NULL)
		ABORT_FINALIZE(RS_RET_ERR);
	curl = curl_easy_init();
	if(curl == NULL)
		ABORT_FINALIZE(RS_RET_ERR);
	asprintf(&url,
		"https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s",
		d->model?d->model:"gemini-1.5-pro", d->apikey);
	req = json_object_new_object();
	struct json_object *contents = json_object_new_array();
	struct json_object *prompt = json_object_new_object();
	json_object_object_add(prompt, "role", json_object_new_string("user"));
	struct json_object *pa = json_object_new_array();
	json_object_array_add(pa, json_object_new_string(d->prompt?d->prompt:""));
	json_object_object_add(prompt, "parts", pa);
	json_object_array_add(contents, prompt);
	struct json_object *arr = json_object_new_object();
	json_object_object_add(arr, "role", json_object_new_string("user"));
	struct json_object *parts = json_object_new_array();
	struct json_object *msgsArr = json_object_new_array();
	for(size_t i=0;i<n;++i) json_object_array_add(msgsArr, json_object_new_string(msgs[i]));
	json_object_array_add(parts, msgsArr);
	json_object_object_add(arr, "parts", parts);
	json_object_array_add(contents, arr);
	json_object_object_add(req, "contents", contents);
	const char *body = json_object_to_json_string(req);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	if(curl_easy_perform(curl) != CURLE_OK)
		ABORT_FINALIZE(RS_RET_IO_ERROR);
	struct json_object *parsed = json_tokener_parse(resp.buf);
	struct json_object *candidates;
	if(parsed == NULL || !json_object_object_get_ex(parsed, "candidates", &candidates))
		ABORT_FINALIZE(RS_RET_ERR);
	struct json_object *first = json_object_array_get_idx(candidates, 0);
	struct json_object *content;
	json_object_object_get_ex(first, "content", &content);
	struct json_object *p;
	json_object_object_get_ex(content, "parts", &p);
	const char *raw = json_object_get_string(json_object_array_get_idx(p,0));
	char *tmp = strdup(raw);
	char *tok = strtok(tmp, ",");
	CHKiRet(rsCAlloc((void**)&*tags, n * sizeof(char*)));
	size_t i=0;
	while(tok!=NULL && i<n){ (*tags)[i++] = strdup(tok); tok=strtok(NULL, ","); }
	while(i<n) (*tags)[i++] = strdup("REGULAR");
finalize_it:
	free(tmp);
	free(url);
	free(resp.buf);
	json_object_put(req);
	RETiRet;
}

ai_provider_t gemini_provider = {
.data = NULL,
.init = gemini_init,
.classify = gemini_classify_batch,
.cleanup = gemini_cleanup
};
