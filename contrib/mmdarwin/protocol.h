/* Copyright 2019 Advens
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
     * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef DARWIN_PROTOCOL_H
# define DARWIN_PROTOCOL_H

#define MAX_BODY_ELEMENTS 20

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <netinet/in.h>

#define DARWIN_FILTER_CODE_NO 0x00000000

// To generate a new filter code, please use the following piece of code:
// generator.c
//
// #include <stdio.h>
// #include <string.h>
//
// int main(int ac, char** av) {
//     if (ac != 2 || strlen(av[1]) != 4) {
//         printf("Usage: %s four_letter_name\n", av[0]);
//         return 1;
//     }
//     printf("0x");
//     for (int i = 0; i < 4; ++i) {
//         printf("%X", av[1][i]);
//     }
//     printf("\n");
//     return 0;
// }
//
// To compile it: gcc generator.c -o filter_code_generator
// ex: $ ./filter_code_generator xmpl
//     0x786D706Cc

/// Represent the receiver of the results.
///
/// \enum darwin_response_type
enum darwin_filter_response_type {
	DARWIN_RESPONSE_SEND_NO = 0,//!< Don't send results to anybody.
	DARWIN_RESPONSE_SEND_BACK, //!< Send results back to caller.
	DARWIN_RESPONSE_SEND_DARWIN, //!< Send results to the next filter.
	DARWIN_RESPONSE_SEND_BOTH, //!< Send results to both caller and the next filter.
};

/// Represent the type of information sent.
///
/// \enum darwin_packet_type
enum darwin_packet_type {
	DARWIN_PACKET_OTHER = 0, //!< Information sent by something else.
	DARWIN_PACKET_FILTER, //!< Information sent by another filter.
};

/// Represent the type of the IP sent.
///
/// \enum darwin_ip_type
enum darwin_ip_type {
	DARWIN_IP_UNKNOWN = 0, //!< Default value. Error value.
	DARWIN_IP_4, //!< IP sent is IPv4. MUST use 'ip' field of the structure.
	DARWIN_IP_6 //!< IP sent is IPv6. MUST use 'ip6' field of the structure.
};

/// First packet to be sent to a filter.
///
/// \struct darwin_filter_packet_t
typedef struct {
	//!< The type of information sent.
	enum darwin_packet_type type;
	//!< Whom the response will be sent to.
	enum darwin_filter_response_type response;
	//!< The type of the IP, either v4 or v6.
	enum darwin_ip_type     ip_type;
	//!< Contain an IPv4 if ip_type indicates it. May be uninitialized otherwise.
	struct in_addr          ip;
	//!< Contain an IPv6 if ip_type indicates it. May be uninitialized otherwise.
	struct in6_addr         ip6;
	//!< The unique identifier code of a filter.
	long                    filter_code;
	//!< The score or the certitude of the module. May be used to pass other info in specific cases.
	unsigned int            certitude;
	//!< The complete size of the the parameters to be sent (if needed).
	size_t                  body_size;
	//!< The number of parameters to be sent (if needed).
	size_t                  body_elements_number;
	//!< An array containing the size of each parameters to be sent.
	size_t                  body_elements_sizes[MAX_BODY_ELEMENTS];
} darwin_filter_packet_t;

#ifdef __cplusplus
};
#endif

#endif /* !DARWIN_PROTOCOL_H */
