/* http_parser.c
 *
 * This file contains functions to parse HTTP headers.
 *
 * File begun on 2018-11-13
 *
 * Created by:
 *  - Théo Bertin (theo.bertin@advens.fr)
 *
 * With:
 *  - François Bernard (francois.bernard@isen.yncrea.fr)
 *  - Tianyu Geng (tianyu.geng@isen.yncrea.fr)
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"
#include "parsers.h"

static const char *keywords[] = {
		"OPTIONS",
		"GET",
		"HEAD",
		"POST",
		"PUT",
		"DELETE",
		"TRACE",
		"CONNECT",
		"HTTP",
		NULL
};

static inline char *string_split(char **initString, const char *delimiterString) {
	char *ret = *initString;

	if (*initString) {
		char *pos = strstr(*initString, delimiterString);
		if (pos) {
			*initString = pos;
			**initString = '\0';
			*initString += strlen(delimiterString);
		} else {
			*initString = NULL;
		}
	}

	return ret;
}

static inline int has_status_keyword(char *http) {
	const char *found;
	int i;

	for (i = 0; keywords[i] != NULL; i++) {
		found = strstr(http, keywords[i]);
		if (found && (found - http) < 20) {
			return 1;
		}
	}

	return 0;
}

/*
 *  This function catches HTTP header fields and status line
 *  and adds them to the provided json object
*/
static inline void catch_status_and_fields(char *header, struct json_object *jparent) {
	DBGPRINTF("catch_status_and_fields\n");

	struct json_object *fields = json_object_new_object();

	char *statusLine = string_split(&header, "\r\n");
	char *firstPart, *secondPart, *thirdPart;
	firstPart = string_split(&statusLine, " ");
	secondPart = string_split(&statusLine, " ");
	thirdPart = statusLine;
	if (firstPart && secondPart && thirdPart) {
		if (strstr(firstPart, "HTTP")) {
			json_object_object_add(jparent, "HTTP_version", json_object_new_string(firstPart));
			json_object_object_add(jparent, "HTTP_status_code", json_object_new_string(secondPart));
			json_object_object_add(jparent, "HTTP_reason", json_object_new_string(thirdPart));
		} else {
			json_object_object_add(jparent, "HTTP_method", json_object_new_string(firstPart));
			json_object_object_add(jparent, "HTTP_request_URI", json_object_new_string(secondPart));
			json_object_object_add(jparent, "HTTP_version", json_object_new_string(thirdPart));
		}
	}

	char *fieldValue = string_split(&header, "\r\n");
	char *field, *value;
	while (fieldValue) {
		field = string_split(&fieldValue, ":");
		value = fieldValue;
		if (value) {
			while (*value == ' ') { value++; }
			DBGPRINTF("got header field -> '%s': '%s'\n", field, value);
			json_object_object_add(fields, field, json_object_new_string(value));
		}

		fieldValue = string_split(&header, "\r\n");
	}

	json_object_object_add(jparent, "HTTP_header_fields", fields);

	return;
}

/*
 *  This function parses the bytes in the received packet to extract HTTP metadata.
 *
 *  its parameters are:
 *    - a pointer on the list of bytes representing the packet
 *        the beginning of the header will be checked by the function
 *    - the size of the list passed as first parameter
 *    - a pointer on a json_object, containing all the metadata recovered so far
 *      this is also where HTTP metadata will be added
 *
 *  This function returns a structure containing the data unprocessed by this parser
 *  or the ones after (as a list of bytes), and the length of this data.
*/
data_ret_t *http_parse(const uchar *packet, int pktSize, struct json_object *jparent) {
	DBGPRINTF("http_parse\n");
	DBGPRINTF("packet size %d\n", pktSize);
	if (pktSize < 6) {
		RETURN_DATA_AFTER(0)
	}

	char *pHttp = malloc(pktSize + 1);
	char *http = pHttp;
	memcpy(http, packet, pktSize);
	*(http + pktSize) = '\0';

	if (!has_status_keyword(http)) {
		free(pHttp);
		RETURN_DATA_AFTER(0)
	}

	char *header = string_split(&http, "\r\n\r\n");

	catch_status_and_fields(header, jparent);

	free(pHttp);
	RETURN_DATA_AFTER(0)
}
