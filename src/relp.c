/* The RELP (reliable event logging protocol) core protocol library.
 *
 * Copyright 2008 by Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of librelp.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 *
 * If the terms of the GPL are unsuitable for your needs, you may obtain
 * a commercial license from Adiscon. Contact sales@adiscon.com for further
 * details.
 *
 * ALL CONTRIBUTORS PLEASE NOTE that by sending contributions, you assign
 * your copyright to Adiscon GmbH, Germany. This is necessary to permit the
 * dual-licensing set forth here. Our apologies for this inconvenience, but
 * we sincerely believe that the dual-licensing model helps us provide great
 * free software while at the same time obtaining some funding for further
 * development.
 */


/* DESCRIPTION OF THE RELP PROTOCOL
 *
 * Relp uses a client-server model with fixed roles. The initiating part of the
 * connection is called the client, the listening part the server. In the state
 * diagrams below, C stand for client and S for server.
 *
 * Relp employs a command-response model, that is the client issues command to
 * which the server responds. To facilitate full-duplex communication, multiple
 * commands can be issued at the same time, thus multiple responses may be 
 * outstanding at a given time. The server may reply in any order. To conserve
 * ressources, the number of outstanding commands is limited by a window. Each
 * command is assigned a (relative) unique, monotonically increasing ID. Each
 * response must include that ID. New commands may only be issued if the new ID
 * is less than the oldes unresponded ID plus the window size. So a connection
 * stalls if the server does not respond to all requests. (TODO: evaluate if this
 * needs to be a hard requirement or if it is sufficient to just allow "n" outstanding
 * commands at a time).
 *
 * A command and its response is called a relp transaction.
 *
 * If something goes really wrong, both the client and the server may terminate
 * the TCP connection at any time. Any outstanding commands are considered to
 * have been unsuccessful in this case.
 *
 * SENDING MESSAGES
 * Because it is so important, I'd like to point it specifically out: sending a
 * message is "just" another RELP command. The reply to that command is the ACK/NAK
 * for the message. So every message *is* acknowledged. RELP options indicate how
 * "deep" this acknowledge is (one we have implemented that), in the most extreme
 * case a RELP client may ask a RELP server to ack only after the message has been
 * completely acted upon (e.g. successfully written to database) - but this is far
 * away in the future. For now, keep in mind that message loss will always be detected
 * because we have app-level acknowledgement.
 *
 * RELP FRAME
 * All relp transaction are carried out over a consistent framing.
 *
 * RELP-FRAME     =  HEADER DATA TRAILER
 * DATA           = *OCTET ; command-defined data
 * HEADER         = TXNR SP COMMAND SP DATALEN
 * TXNR           = NUMBER ; relp transaction number, monotonically increases
 * DATALEN        = NUMBER
 * COMMAND        = "init" / "go" / "msg" / "close" / "rsp"
 * TRAILER        = LF ; to detect framing errors and enhance human readibility
 * NUMBER         = 1*DIGIT
 * DIGIT          = %d48-57
 * LF             = %d10
 * SP             = %d32
 *
 * Note that TXNR montonically increases, but at some point latches. The requirement
 * is to have enough different number values to handle a complete window. This may be
 * used to optimize traffic a bit by using short numbers. E.g. transaction numbers
 * may (may!) be latched at 1000 (so the next TXNR after 999 will be 0).
 *
 * COMMAND SEMANTICS
 *
 * Command "rsp"
 * Response to a client-issued command. The TXNR MUST match the client's command
 * TXN. The data part contains a response number, optionally followed by a space and
 * additional data (depending on the client's command).
 *   Return state values are: 200 - OK, 500 - error
 *   TODO: formalize response packet
 *
 *
 * Command "init"
 * Intializes a connection to the server. May include offers inside the data. Offers
 * provide information about services supported by the client.
 *
 * When the server receives an init, it parses the offers, checks what it itself supports
 * and provides those offers that it accepts in the "rsp". 
 *
 * When the client receives the "rsp", it checks the servers offers and finally selects
 * those that should be used during the session. Please note that this doesn't imply the
 * client selects e.g. security strength. To require a specific security strength, the
 * server must be configured to offer only those options back to the client that it is
 * happy to accept. So the client can only select from those. As such, even though the
 * client makes the final feature selection, the server is dictating what needs to be 
 * used.
 *
 * Once the client has made its selection, it sends back a "go" command to the server,
 * which finally initializes the connection and makes it ready for transmission of other
 * commands. Note that the connection is only ready AFTER the server has sent a positive
 * response to the "go" command, so the client must wait for it (and a negative response
 * means the connection is NOT usable).
 *
 * STATE DIAGRAMS
 * ... detailling some communications scenarios:
 *
 * Session Startup:
 * C                                          S
 * cmd: "init", data: offer          -----> (selects supported offers)
 * (selects offers to use)           <----- cmd: "rsp", data "accepted offers"
 * cmd: "go", data: "offer to use"   -----> (initializes connection)
 *                                   <----- cmd: "rsp", data "200 OK" (or error)
 *
 *                 ... transmission channel is ready to use ....
 *
 * Message Transmission
 * C                                          S
 * cmd: "msg", data: msgtext         -----> (processes message)
 * (indicates msg as processed)      <----- cmd: "rsp", data OK/Error
 *
 * Session Termination
 * C                                          S
 * cmd: "close", data: none?         -----> (processes termination request)
 * (terminates session)              <----- cmd: "rsp", data OK/Error
 *                                          (terminates session)
 */
int	relpInit(void)
{
}
