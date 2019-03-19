/// \file     protocol.h
/// \authors  hsoszynski
/// \version  1.0
/// \date     05/06/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

/*
 *        WARNING!
 * DO NOT MODIFY THIS FILE.
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
    enum darwin_packet_type type; //!< The type of information sent.
    enum darwin_filter_response_type response; //!< Whom the response will be sent to.
    enum darwin_ip_type     ip_type; //!< The type of the IP, either v4 or v6.
    struct in_addr          ip; //!< Contain an IPv4 if ip_type indicates it. May be uninitialized otherwise.
    struct in6_addr         ip6; //!< Contain an IPv6 if ip_type indicates it. May be uninitialized otherwise.
    long                    filter_code; //!< The unique identifier code of a filter.
    unsigned int            certitude; //!< The score or the certitude of the module. May be used to pass other info in specific cases.
    size_t                  body_size; //!< The complete size of the the parameters to be sent (if needed).
    size_t                  body_elements_number; //!< The number of parameters to be sent (if needed).
    size_t                  body_elements_sizes[MAX_BODY_ELEMENTS]; //!< An array containing the size of each parameters to be sent.
} darwin_filter_packet_t;

#ifdef __cplusplus
};
#endif

#endif /* !DARWIN_PROTOCOL_H */
