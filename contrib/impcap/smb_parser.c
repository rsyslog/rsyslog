/* smb_parser.c
 *
 * This file contains functions to parse SMB (version 2 and 3) headers.
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

/* SMB2 opCodes */
#define SMB2_NEGOTIATE        0x00
#define SMB2_SESSIONSET       0x01
#define SMB2_SESSIONLOGOFF    0x02
#define SMB2_TREECONNECT      0x03
#define SMB2_TREEDISCONNECT   0x04
#define SMB2_CREATE           0x05
#define SMB2_CLOSE            0x06
#define SMB2_FLUSH            0x07
#define SMB2_READ             0x08
#define SMB2_WRITE            0x09
#define SMB2_LOCK             0x0a
#define SMB2_IOCTL            0x0b
#define SMB2_CANCEL           0x0c
#define SMB2_KEEPALIVE        0x0d
#define SMB2_FIND             0x0e
#define SMB2_NOTIFY           0x0f
#define SMB2_GETINFO          0x10
#define SMB2_SETINFO          0x11
#define SMB2_BREAK            0x12

struct smb_header_s {
	uint32_t version;
	uint16_t headerLength;
	uint16_t padding1;
	uint32_t ntStatus;
	uint16_t opCode;
	uint16_t padding2;
	uint32_t flags;
	uint32_t chainOffset;
	uint32_t comSeqNumber[2];
	uint32_t processID;
	uint32_t treeID;
	uint32_t userID[2];
	uint32_t signature[4];
};

typedef struct smb_header_s smb_header_t;

static char flagCodes[5] = "RPCS";

/*
 *  This function parses the bytes in the received packet to extract SMB2 metadata.
 *
 *  its parameters are:
 *    - a pointer on the list of bytes representing the packet
 *        the beginning of the header will be checked by the function
 *    - the size of the list passed as first parameter
 *    - a pointer on a json_object, containing all the metadata recovered so far
 *      this is also where SMB2 metadata will be added
 *
 *  This function returns a structure containing the data unprocessed by this parser
 *  or the ones after (as a list of bytes), and the length of this data.
*/
data_ret_t *smb_parse(const uchar *packet, int pktSize, struct json_object *jparent) {
	DBGPRINTF("smb_parse\n");
	DBGPRINTF("packet size %d\n", pktSize);

	int pktSizeCpy = pktSize;
	const uchar *packetCpy = packet;

	while (pktSizeCpy > 0) {
		/* don't check packetCpy[0] to include SMB version byte at the beginning */
		if (packetCpy[1] == 'S') {
			if (packetCpy[2] == 'M') {
				if (packetCpy[3] == 'B') {
					break;
				}
			}
		}
		packetCpy++, pktSizeCpy--;
	}

	if ((int)pktSizeCpy < 64) {
		DBGPRINTF("SMB packet too small : %d\n", pktSizeCpy);
		RETURN_DATA_AFTER(0)
	}

	/* Union to prevent cast from uchar to smb_header_t */
	union {
		const uchar *pck;
		smb_header_t *hdr;
	} smb_header_to_char;

	smb_header_to_char.pck = packetCpy;
	smb_header_t *smb_header = smb_header_to_char.hdr;

	char flags[5] = {0};
	uint64_t seqNum, userID;
	uint8_t version;

	version = (smb_header->version == 0xFF) ? 1 : 2;
	seqNum = smb_header->comSeqNumber[0] | smb_header->comSeqNumber[1] << 16;
	userID = smb_header->userID[0] | smb_header->userID[1] << 16;

	uint8_t i, pos = 0;
	for (i = 0; i < 4; ++i) {
		if (smb_header->flags & (0x01 << i))
			flags[pos++] = flagCodes[i];
	}

	json_object_object_add(jparent, "SMB_version", json_object_new_int(version));
	json_object_object_add(jparent, "SMB_NTstatus", json_object_new_int64(smb_header->ntStatus));
	json_object_object_add(jparent, "SMB_operation", json_object_new_int(smb_header->opCode));
	json_object_object_add(jparent, "SMB_flags", json_object_new_string(flags));
	json_object_object_add(jparent, "SMB_seqNumber", json_object_new_int64(seqNum));
	json_object_object_add(jparent, "SMB_processID", json_object_new_int64(smb_header->processID));
	json_object_object_add(jparent, "SMB_treeID", json_object_new_int64(smb_header->treeID));
	json_object_object_add(jparent, "SMB_userID", json_object_new_int64(userID));

	RETURN_DATA_AFTER(0)
}
