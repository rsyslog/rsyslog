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
#define DARWIN_PROTOCOL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <netinet/in.h>

#define DARWIN_FILTER_CODE_NO 0x00000000
// the default certitude list size, which is 1, to allow FMAs (see flexible array members on C99) for both C and C++
// code
#define DEFAULT_CERTITUDE_LIST_SIZE 1

	/// Represent the receiver of the results.
	///
	/// \enum darwin_response_type
	enum darwin_filter_response_type
	{
		DARWIN_RESPONSE_SEND_NO = 0, //!< Don't send results to anybody.
		DARWIN_RESPONSE_SEND_BACK,   //!< Send results back to caller.
		DARWIN_RESPONSE_SEND_DARWIN, //!< Send results to the next filter.
		DARWIN_RESPONSE_SEND_BOTH,   //!< Send results to both caller and the next filter.
	};

	/// Represent the type of information sent.
	///
	/// \enum darwin_packet_type
	enum darwin_packet_type
	{
		DARWIN_PACKET_OTHER = 0, //!< Information sent by something else.
		DARWIN_PACKET_FILTER,	//!< Information sent by another filter.
	};

	/// First packet to be sent to a filter.
	///
	/// \struct darwin_filter_packet_t
	typedef struct
	{
		enum darwin_packet_type type;			//!< The type of information sent.
		enum darwin_filter_response_type response;	//!< Whom the response will be sent to.
		long filter_code;				//!< The unique identifier code of a filter.
		size_t body_size;				//!< The complete size of the the parameters to be sent
		unsigned char evt_id[16];			//!< An array containing the event ID
		size_t certitude_size;				//!< The size of the list containing the certitudes.
		unsigned int certitude_list[DEFAULT_CERTITUDE_LIST_SIZE];
		//!< The scores or the certitudes of the module. May be used to pass other info in specific cases.
	} darwin_filter_packet_t;

#ifdef __cplusplus
};
#endif

#endif /* !DARWIN_PROTOCOL_H */
