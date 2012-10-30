/* imzmq3.c
 *
 * This input plugin enables rsyslog to read messages from a ZeroMQ
 * queue.
 *
 * Copyright 2012 Talksum, Inc.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 * 
 * Author: David Kelly
 * <davidk@talksum.com>
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rsyslog.h"

#include "cfsysline.h"
#include "config.h"
#include "dirty.h"
#include "errmsg.h"
#include "glbl.h"
#include "module-template.h"
#include "msg.h"
#include "net.h"
#include "parser.h"
#include "prop.h"
#include "ruleset.h"
#include "srUtils.h"
#include "unicode-helper.h"

#include <czmq.h>

MODULE_TYPE_INPUT
MODULE_TYPE_NOKEEP

/* convienent symbols to denote a socket we want to bind
 * vs one we want to just connect to 
 */
#define ACTION_CONNECT 1
#define ACTION_BIND    2

/* Module static data */
DEF_IMOD_STATIC_DATA
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(prop)
DEFobjCurrIf(ruleset)

 
/* ----------------------------------------------------------------------------
 * structs to describe sockets
 */
typedef struct _socket_type {
    char*  name;
    int    type;
} socket_type;

/* more overkill, but seems nice to be consistent.*/
typedef struct _socket_action {
    char* name;
    int   action;
} socket_action;

typedef struct _poller_data {
    ruleset_t*  ruleset;
    thrdInfo_t* thread;
} poller_data;

typedef struct _socket_info {
    int        type;
    int        action;
    char*      description;
    int        sndHWM; /* if you want more than 2^32 messages, */
    int        rcvHWM; /* then pass in 0 (the default). */
    char*      identity;
    char**     subscriptions;
    ruleset_t* ruleset;
    int     sndBuf;
    int     rcvBuf;
    int     linger;
    int     backlog;
    int     sndTimeout;
    int     rcvTimeout;
    int     maxMsgSize;
    int     rate;
    int     recoveryIVL;
    int     multicastHops;
    int     reconnectIVL;
    int     reconnectIVLMax;
    int     ipv4Only;
    int     affinity;

} socket_info;


/* ----------------------------------------------------------------------------
 *  Static definitions/initializations.
 */
static socket_info*         s_socketInfo    = NULL;
static size_t				s_nitems        = 0;
static prop_t *				s_namep         = NULL;
static zloop_t*             s_zloop         = NULL;
static int                  s_io_threads    = 1;
static zctx_t*              s_context       = NULL;
static ruleset_t*           s_ruleset       = NULL;
static socket_type          socketTypes[]   = {
    {"SUB",  ZMQ_SUB  },
    {"PULL", ZMQ_PULL },
    {"XSUB", ZMQ_XSUB }
};

static socket_action        socketActions[] = {
    {"BIND",    ACTION_BIND},
    {"CONNECT", ACTION_CONNECT},
};


/* ----------------------------------------------------------------------------
 * Helper functions
 */

/* get the name of a socket type, return the ZMQ_XXX type
   or -1 if not a supported type (see above)
*/
static int getSocketType(char* name) {
    int type = -1;
    uint i;
    
    /* match name with known socket type */
    for(i=0; i<sizeof(socketTypes)/sizeof(socket_type); ++i) {
        if( !strcmp(socketTypes[i].name, name) ) {
            type = socketTypes[i].type;
            break;
        }
    }
    
    /* whine if no match was found. */
    if (type == -1) 
        errmsg.LogError(0, NO_ERRCODE, "unknown type %s",name);
    
    return type;
}


static int getSocketAction(char* name) {
    int action = -1;
    uint i;

    /* match name with known socket action */
    for(i=0; i < sizeof(socketActions)/sizeof(socket_action); ++i) {
        if(!strcmp(socketActions[i].name, name)) {
            action = socketActions[i].action;
            break;
        }
    }

    /* whine if no matching action was found */
    if (action == -1) 
        errmsg.LogError(0, NO_ERRCODE, "unknown action %s",name);
    
    return action;
}


static void setDefaults(socket_info* info) {
    info->type            = ZMQ_SUB;
    info->action          = ACTION_BIND;
    info->description     = NULL;
    info->sndHWM          = 0;
    info->rcvHWM          = 0;
    info->identity        = NULL;
    info->subscriptions   = NULL;
    info->ruleset         = NULL;
    info->sndBuf          = -1;
    info->rcvBuf          = -1;
    info->linger          = -1;
    info->backlog         = -1;
    info->sndTimeout      = -1;
    info->rcvTimeout      = -1;
    info->maxMsgSize      = -1;
    info->rate            = -1;
    info->recoveryIVL     = -1;
    info->multicastHops   = -1;
    info->reconnectIVL    = -1;
    info->reconnectIVLMax = -1;
    info->ipv4Only        = -1;
    info->affinity        = -1;

};


/* The config string should look like:
 * "action=AAA,type=TTT,description=DDD,sndHWM=SSS,rcvHWM=RRR,subscribe='xxx',subscribe='yyy'"
 * 
 */
static rsRetVal parseConfig(char* config, socket_info* info) {
    int nsubs = 0;
    
    char* binding;
    char* ptr1;
    for (binding = strtok_r(config, ",", &ptr1);
         binding != NULL;
         binding = strtok_r(NULL, ",", &ptr1)) {
        
        /* Each binding looks like foo=bar */
        char * sep = strchr(binding, '=');
        if (sep == NULL)
        {
            errmsg.LogError(0, NO_ERRCODE,
                            "Invalid argument format %s, ignoring ...",
                            binding);
            continue;
        }

        /* Replace '=' with '\0'. */
        *sep = '\0';

        char * val = sep + 1;

        if (strcmp(binding, "action") == 0) {
            info->action = getSocketAction(val);
        } else if (strcmp(binding, "type") == 0) {
            info->type = getSocketType(val);
        } else if (strcmp(binding, "description") == 0) {
            info->description = strdup(val);
        } else if (strcmp(binding, "sndHWM") == 0) {
            info->sndHWM = atoi(val);
        } else if (strcmp(binding, "rcvHWM") == 0) { 
            info->sndHWM = atoi(val);
        } else if (strcmp(binding, "subscribe") == 0) {
            /* Add the subscription value to the list.*/
            char * substr = NULL;
            substr = strdup(val);
            info->subscriptions = realloc(info->subscriptions, sizeof(char *) * nsubs + 1);
            info->subscriptions[nsubs] = substr;
            ++nsubs;
        } else if (strcmp(binding, "sndBuf") == 0) {
            info->sndBuf = atoi(val);
        } else if (strcmp(binding, "rcvBuf") == 0) {
            info->rcvBuf = atoi(val);
        } else if (strcmp(binding, "linger") == 0) {
            info->linger = atoi(val);
        } else if (strcmp(binding, "backlog") == 0) {
            info->backlog = atoi(val);
        } else if (strcmp(binding, "sndTimeout") == 0) {
            info->sndTimeout = atoi(val);
        } else if (strcmp(binding, "rcvTimeout") == 0) {
            info->rcvTimeout = atoi(val);
        } else if (strcmp(binding, "maxMsgSize") == 0) {
            info->maxMsgSize = atoi(val);
        } else if (strcmp(binding, "rate") == 0) {
            info->rate = atoi(val);
        } else if (strcmp(binding, "recoveryIVL") == 0) {
            info->recoveryIVL = atoi(val);
        } else if (strcmp(binding, "multicastHops") == 0) {
            info->multicastHops = atoi(val);
        } else if (strcmp(binding, "reconnectIVL") == 0) {
            info->reconnectIVL = atoi(val);
        } else if (strcmp(binding, "reconnectIVLMax") == 0) {
            info->reconnectIVLMax = atoi(val);
        } else if (strcmp(binding, "ipv4Only") == 0) {
            info->ipv4Only = atoi(val);
        } else if (strcmp(binding, "affinity") == 0) {
            info->affinity = atoi(val);
        } else {
            errmsg.LogError(0, NO_ERRCODE, "Unknown argument %s", binding);
            return RS_RET_INVALID_PARAMS;
        }
    }

    return RS_RET_OK;
}

static rsRetVal validateConfig(socket_info* info) {

    if (info->type == -1) {
        errmsg.LogError(0, RS_RET_INVALID_PARAMS,
                        "you entered an invalid type");
        return RS_RET_INVALID_PARAMS;
    }
    if (info->action == -1) {
        errmsg.LogError(0, RS_RET_INVALID_PARAMS,
                        "you entered an invalid action");
        return RS_RET_INVALID_PARAMS;
    }
    if (info->description == NULL) {
        errmsg.LogError(0, RS_RET_INVALID_PARAMS,
                        "you didn't enter a description");
        return RS_RET_INVALID_PARAMS;
    }
    if(info->type == ZMQ_SUB && info->subscriptions == NULL) {
        errmsg.LogError(0, RS_RET_INVALID_PARAMS,
                        "SUB sockets need at least one subscription");
        return RS_RET_INVALID_PARAMS;
    }
    if(info->type != ZMQ_SUB && info->subscriptions != NULL) {
        errmsg.LogError(0, RS_RET_INVALID_PARAMS,
                        "only SUB sockets can have subscriptions");
        return RS_RET_INVALID_PARAMS;
    }
    return RS_RET_OK;
}

static rsRetVal createContext() {
    if (s_context == NULL) {
        errmsg.LogError(0, NO_ERRCODE, "creating zctx.");
        s_context = zctx_new();
        
        if (s_context == NULL) {
            errmsg.LogError(0, RS_RET_INVALID_PARAMS,
                            "zctx_new failed: %s",
                            strerror(errno));
            /* DK: really should do better than invalid params...*/
            return RS_RET_INVALID_PARAMS;
        }
        
        if (s_io_threads > 1) {
            errmsg.LogError(0, NO_ERRCODE, "setting io worker threads to %d", s_io_threads);
            zctx_set_iothreads(s_context, s_io_threads);
        }
    }
    return RS_RET_OK;
}

static rsRetVal createSocket(socket_info* info, void** sock) {
    size_t ii;
    int rv;

    *sock = zsocket_new(s_context, info->type);
    if (!sock) {
		errmsg.LogError(0,
                        RS_RET_INVALID_PARAMS,
                        "zsocket_new failed: %s, for type %d",
                        strerror(errno),info->type);
		/* DK: invalid params seems right here */
        return RS_RET_INVALID_PARAMS;
    }

    /* Set options *before* the connect/bind. */
    if (info->identity)             zsocket_set_identity(*sock, info->identity);
    if (info->sndBuf > -1)          zsocket_set_sndbuf(*sock, info->sndBuf);
    if (info->rcvBuf > -1)          zsocket_set_rcvbuf(*sock, info->rcvBuf);
    if (info->linger > -1)          zsocket_set_linger(*sock, info->linger);
    if (info->backlog > -1)         zsocket_set_backlog(*sock, info->backlog);
    if (info->sndTimeout > -1)      zsocket_set_sndtimeo(*sock, info->sndTimeout);
    if (info->rcvTimeout > -1)      zsocket_set_rcvtimeo(*sock, info->rcvTimeout);
    if (info->maxMsgSize > -1)      zsocket_set_maxmsgsize(*sock, info->maxMsgSize);
    if (info->rate > -1)            zsocket_set_rate(*sock, info->rate);
    if (info->recoveryIVL > -1)     zsocket_set_recovery_ivl(*sock, info->recoveryIVL);
    if (info->multicastHops > -1)   zsocket_set_multicast_hops(*sock, info->multicastHops);
    if (info->reconnectIVL > -1)    zsocket_set_reconnect_ivl(*sock, info->reconnectIVL);
    if (info->reconnectIVLMax > -1) zsocket_set_reconnect_ivl_max(*sock, info->reconnectIVLMax);
    if (info->ipv4Only > -1)        zsocket_set_ipv4only(*sock, info->ipv4Only);
    if (info->affinity > -1)        zsocket_set_affinity(*sock, info->affinity);
   
    /* since HWM have defaults, we always set them.  No return codes to check, either.*/
    zsocket_set_sndhwm(*sock, info->sndHWM);
    zsocket_set_rcvhwm(*sock, info->rcvHWM);

    /* Set subscriptions.*/
    if (info->type == ZMQ_SUB) {
	    for (ii = 0; ii < sizeof(info->subscriptions)/sizeof(char*); ++ii)
		zsocket_set_subscribe(*sock, info->subscriptions[ii]);
    }

    

    /* Do the bind/connect... */
    if (info->action==ACTION_CONNECT) {
        rv = zsocket_connect(*sock, info->description);
        if (rv < 0) {
            errmsg.LogError(0,
                            RS_RET_INVALID_PARAMS,
                            "zmq_connect using %s failed: %s",
                            info->description, strerror(errno));
            return RS_RET_INVALID_PARAMS;
        }
    } else {
        rv = zsocket_bind(*sock, info->description);
        if (rv <= 0) {
            errmsg.LogError(0,
                            RS_RET_INVALID_PARAMS,
                            "zmq_bind using %s failed: %s",
                            info->description, strerror(errno));
            return RS_RET_INVALID_PARAMS;
        }
    }
    return RS_RET_OK;
}

/* ----------------------------------------------------------------------------
 * Module endpoints
 */

/* accept a new ruleset to bind. Checks if it exists and complains, if not.  Note
 * that this makes the assumption that after the bind ruleset is called in the config,
 * another call will be made to add an endpoint. 
*/
static rsRetVal
set_ruleset(void __attribute__((unused)) *pVal, uchar *pszName) {
	ruleset_t* ruleset_ptr;
	rsRetVal localRet;
	DEFiRet;

	localRet = ruleset.GetRuleset(ourConf, &ruleset_ptr, pszName);
	if(localRet == RS_RET_NOT_FOUND) {
		errmsg.LogError(0, NO_ERRCODE, "error: "
                        "ruleset '%s' not found - ignored", pszName);
	}
	CHKiRet(localRet);
	s_ruleset = ruleset_ptr;
	DBGPRINTF("imzmq3 current bind ruleset '%s'\n", pszName);

finalize_it:
	free(pszName); /* no longer needed */
	RETiRet;
}

/* add an actual endpoint
 */
static rsRetVal add_endpoint(void __attribute__((unused)) * oldp, uchar * valp) {
    DEFiRet;
    
    /* increment number of items and store old num items, as it will be handy.*/
    size_t idx = s_nitems++;
    
    /* allocate a new socket_info array to accomidate this new endpoint*/
    socket_info* tmpSocketInfo;
    CHKmalloc(tmpSocketInfo = (socket_info*)MALLOC(sizeof(socket_info) * s_nitems));
    
    /* copy existing socket_info across into new array, if any, and free old storage*/
    if(idx) {
        memcpy(tmpSocketInfo, s_socketInfo, sizeof(socket_info) * idx);
        free(s_socketInfo);
    }

    /* set the static to hold the new array */
    s_socketInfo = tmpSocketInfo;
        
    /* point to the new one */
    socket_info* sockInfo = &s_socketInfo[idx];
    
    /* set defaults for the new socket info */
    setDefaults(sockInfo);
    
    /* Make a writeable copy of the string so we can use strtok
       in the parseConfig call */
    char * copy = NULL;
    CHKmalloc(copy = strdup((char *) valp));
    
    /* parse the config string */
    CHKiRet(parseConfig(copy, sockInfo));
    
    /* validate it */
    CHKiRet(validateConfig(sockInfo));

    /* bind to the current ruleset (if any)*/
    sockInfo->ruleset = s_ruleset;
    
finalize_it:
	free(valp); /* in any case, this is no longer needed */
	RETiRet;
}


static int handlePoll(zloop_t __attribute__((unused)) * loop, zmq_pollitem_t *poller, void* pd) {
    msg_t* logmsg;
    poller_data* pollerData = (poller_data*)pd;

    char* buf = zstr_recv(poller->socket);
    if (msgConstruct(&logmsg) == RS_RET_OK) {
        MsgSetRawMsg(logmsg, buf, strlen(buf));
        MsgSetInputName(logmsg, s_namep);
        MsgSetFlowControlType(logmsg, eFLOWCTL_NO_DELAY);
        MsgSetRuleset(logmsg, pollerData->ruleset);
        logmsg->msgFlags = NEEDS_PARSING;
        submitMsg(logmsg);
    }
    
    /* gotta free the string returned from zstr_recv() */
    free(buf);
    
    if( pollerData->thread->bShallStop == TRUE) {
        /* a handler that returns -1 will terminate the 
           czmq reactor loop
        */
        return -1; 
    }
    
    return 0;
}

/* called when runInput is called by rsyslog 
 */
static rsRetVal rcv_loop(thrdInfo_t* pThrd){
    size_t          i;
    int             rv;
    zmq_pollitem_t* items;
    poller_data*    pollerData;

    DEFiRet;
    
    /* create the context*/
    CHKiRet(createContext());

    /* create the poll items*/ 
    CHKmalloc(items = (zmq_pollitem_t*)MALLOC(sizeof(zmq_pollitem_t)*s_nitems));
    
    /* create poller data (stuff to pass into the zmq closure called when we get a message)*/
    CHKmalloc(pollerData = (poller_data*)MALLOC(sizeof(poller_data)*s_nitems));

    /* loop through and initialize the poll items and poller_data arrays...*/
    for(i=0; i<s_nitems;++i) {
        /* create the socket, update items.*/
        createSocket(&s_socketInfo[i], &items[i].socket);
        items[i].events = ZMQ_POLLIN;
        
        /* now update the poller_data for this item */
        pollerData[i].thread  = pThrd;
        pollerData[i].ruleset = s_socketInfo[i].ruleset;
    }
    
    s_zloop = zloop_new();
    for(i=0; i<s_nitems; ++i) {
        
        rv = zloop_poller(s_zloop, &items[i], handlePoll, &pollerData[i]);
        if (rv) {
            errmsg.LogError(0, NO_ERRCODE, "imzmq3: zloop_poller failed for item %zu", i);
        } 
    }
    zloop_start(s_zloop);   
    zloop_destroy(&s_zloop);
 finalize_it:
    for(i=0; i< s_nitems; ++i) {
        zsocket_destroy(s_context, items[i].socket);
    }

    zctx_destroy(&s_context);

    free(items);
    RETiRet;
}

/* ----------------------------------------------------------------------------
 * input module functions
 */

BEGINrunInput
CODESTARTrunInput
	iRet = rcv_loop(pThrd);
    RETiRet;
ENDrunInput


/* initialize and return if will run or not */
BEGINwillRun
CODESTARTwillRun
	/* we need to create the inputName property (only once during our
       lifetime) */
	CHKiRet(prop.Construct(&s_namep));
	CHKiRet(prop.SetString(s_namep,
                           UCHAR_CONSTANT("imzmq3"),
                           sizeof("imzmq3") - 1));
	CHKiRet(prop.ConstructFinalize(s_namep));

/* If there are no endpoints this is pointless ...*/
    if (s_nitems == 0)
        ABORT_FINALIZE(RS_RET_NO_RUN);

finalize_it:
ENDwillRun


BEGINafterRun
CODESTARTafterRun
	/* do cleanup here */
	if(s_namep != NULL)
		prop.Destruct(&s_namep);
ENDafterRun


BEGINmodExit
CODESTARTmodExit
	/* release what we no longer need */
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(glbl, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
    objRelease(ruleset, CORE_COMPONENT);
ENDmodExit


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURENonCancelInputTermination)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES
ENDqueryEtryPt

static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp,
                                     void __attribute__((unused)) *pVal) {
	return RS_RET_OK;
}
static rsRetVal setGlobalWorkerThreads(uchar __attribute__((unused)) *pp, int val) {
	errmsg.LogError(0, NO_ERRCODE, "setGlobalWorkerThreads called with %d",val);
    s_io_threads = val;
    return RS_RET_OK;
}

BEGINmodInit()
CODESTARTmodInit
    /* we only support the current interface specification */
	*ipIFVersProvided = CURR_MOD_IF_VERSION;
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));
    CHKiRet(objUse(ruleset, CORE_COMPONENT));

	/* register config file handlers */
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputzmq3serverbindruleset",
                               0, eCmdHdlrGetWord,
                               set_ruleset, NULL, 
                               STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputzmq3serverrun",
                               0, eCmdHdlrGetWord,
                               add_endpoint, NULL, 
                               STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables",
                               1, eCmdHdlrCustomHandler,
                               resetConfigVariables, NULL, 
                               STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr((uchar *)"inputzmq3globalWorkerThreads",
                               1, eCmdHdlrInt,
                               setGlobalWorkerThreads, NULL, 
                               STD_LOADABLE_MODULE_ID));
ENDmodInit
