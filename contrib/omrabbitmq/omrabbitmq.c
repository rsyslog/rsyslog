/* omrabbitmq.c
 *
 * This output plugin enables rsyslog to send messages to the RabbitMQ.
 *
 * Copyright 2012-2013 Vaclav Tomec
 * Copyright 2014 Rainer Gerhards
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Vaclav Tomec
 * <vaclav.tomec@gmail.com>
 */
#include "config.h"
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include "rsyslog.h"
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "cfsysline.h"
#include "debug.h"

#include <sys/socket.h>

#include "amqp.h"
#include "amqp_framing.h"
#include "amqp_tcp_socket.h"
#if (AMQP_VERSION_MAJOR == 0) && (AMQP_VERSION_MINOR < 4)
#error "rabbitmq-c version must be >= 0.4.0"
#endif

#define HEARTBEAT_TIME 0

#define RABBITMQ_CHANNEL 1

MODULE_TYPE_OUTPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("omrabbitmq")

/*
 * internal structures
 */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(glbl)

static int instance_counter = 0;

typedef struct  {
	char *host;                    /* rabbitmq server fqdn or IP */
	int port;                      /* rabbitmq server port */
} server_t;

typedef struct  {
	server_t s;                    /* rabbitmq server */
	int failures;                  /* rabbitmq server failures */
} server_wrk_t;


typedef struct {
	time_t return_check_interval;      /* time interval between usual server health checks */
	time_t half_return_check_interval; /* for computing */
	time_t quick_oscillation_interval; /* time interval below which the service is not stable */
	int quick_oscillation_max; /* number of quick oscillation after which the connection is kept on backup */
	time_t graceful_interval; /* time interval the connection is kept on backup after which the usual server
							   * check restarts */
	int quick_oscillation_count; /* current number of simultaneous quick oscillation detected */
} failback_t;

typedef struct _instanceData {
	/* here you need to define all action-specific data. A record of type
	 * instanceData will be handed over to each instance of the action. Keep
	 * in mind that there may be several invocations of the same type of action
	 * inside rsyslog.conf, and this is what keeps them apart. Do NOT use
	 * static data for this!
	 */
	amqp_bytes_t exchange;                    /* exchange to send message to */

	amqp_bytes_t routing_key;                 /* fixed routing_key to use */
	uchar *routing_key_template;              /* routing_key template */
	int idx_routing_key_template;             /* routing_key template index in doAction tab */

	sbool populate_properties;                /* populates message properties */
	int delivery_mode;                        /* delivery mode transient or persistent message */
	amqp_bytes_t expiration;                  /* message expiration */

	uchar *body_template;                     /* body template */
	int idx_body_template;                    /* body template index in doAction tab */

	amqp_basic_properties_t amqp_props_tpl_type; /*  */
	char *content_type;                       /*  */
	amqp_basic_properties_t amqp_props_plaintext;   /*  */

	char *exchange_type;                      /*  */
	int durable;                              /*  */
	int auto_delete;                          /*  */

	int iidx;
	int nbWrkr;

	int heartbeat;

	server_t server1;               /* first rabbitmq server  */
	server_t server2;               /* second rabbitmq server */

	char *vhost;                    /* rabbitmq server vhost */
	char *user;                     /* rabbitmq username */
	char *password;                 /* rabbitmq username's password */

	failback_t failback_policy;

} instanceData;

typedef struct wrkrInstanceData {
	amqp_connection_state_t a_conn; /* amqp connection */

	pthread_t thread;               /*  */
	short thread_running;           /*  */
	pthread_mutex_t send_mutex;     /*  */
	pthread_mutex_t cond_mutex;     /*  */
	pthread_cond_t cond;            /*  */

	rsRetVal state;                 /* state of the connection */

	server_wrk_t serverPrefered;               /* usual rabbitmq server */
	server_wrk_t serverRescue;               /* rescue rabbitmq server */
	server_wrk_t *serverActive;               /* active rabbitmq server */

	instanceData *pData;

	failback_t failback_policy;
	time_t last_failback;

	int iidx;
	int widx;
	int go_on;
} wrkrInstanceData_t;


/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {
	{ "host", eCmdHdlrString, 0 },
	{ "port", eCmdHdlrInt, 0 },
	{ "virtual_host", eCmdHdlrGetWord, 0 },
	{ "user", eCmdHdlrGetWord, 0 },
	{ "password", eCmdHdlrGetWord, 0 },
	{ "exchange", eCmdHdlrGetWord, 0 },
	{ "routing_key", eCmdHdlrGetWord, 0 },
	{ "routing_key_template", eCmdHdlrGetWord, 0 },
	{ "delivery_mode", eCmdHdlrGetWord, 0 },
	{ "expiration", eCmdHdlrInt, 0 },
	{ "populate_properties", eCmdHdlrBinary, 0 },
	{ "body_template", eCmdHdlrGetWord, 0 },
	{ "content_type", eCmdHdlrGetWord, 0 },
	{ "exchange_type", eCmdHdlrGetWord, 0},
	{ "durable", eCmdHdlrNonNegInt, 0},
	{ "auto_delete", eCmdHdlrNonNegInt, 0},
	{ "failback_policy", eCmdHdlrString, 0 },
};
static struct cnfparamblk actpblk =
	{
		CNFPARAMBLK_VERSION,
		sizeof(actpdescr)/sizeof(struct cnfparamdescr),
		actpdescr
	};

static amqp_bytes_t cstring_bytes(const char *str)
{
	return str ? amqp_cstring_bytes(str) : amqp_empty_bytes;
}

static void init_failback(failback_t *fb, char *str)
{
	time_t value[4] = { 0, 0, 0, 0 };

	if (str && *str){
		int i = -1;
		do {
			value[++i] = strtoul(str, &str, 10);
			if (*str) str++;
		} while (i < 3 && value[i] && *str);
	}

	fb->return_check_interval = (value[0]) ? value[0] : 60;
	fb->half_return_check_interval = fb->return_check_interval / 2;
	fb->quick_oscillation_interval = (value[1]) ? value[1] : (fb->return_check_interval / 10);
	fb->quick_oscillation_max = (value[2]) ? (int)(value[2]) : 3;
	fb->graceful_interval = (value[3]) ? value[3] : (fb->return_check_interval * 60) -
							fb->half_return_check_interval;
	fb->quick_oscillation_count = 0;
}

static unsigned long next_check(failback_t *fb, time_t last_failback)
{
	time_t now = time(NULL);

	if (now - last_failback < fb->quick_oscillation_interval) {
		/* quick oscillation detected */
		fb->quick_oscillation_count++;

		if (fb->quick_oscillation_count > fb->quick_oscillation_max) {
			/* too much quick simultaneous quick oscillation going to graceful sleep */
			fb->quick_oscillation_count = 0;
			srandom(now);
			return fb->graceful_interval + fb->return_check_interval * random() / RAND_MAX;
		}
	} else
		fb->quick_oscillation_count = 0;

	return fb->half_return_check_interval + fb->return_check_interval * random() / RAND_MAX;
}

static int amqp_authenticate(wrkrInstanceData_t *self, amqp_connection_state_t a_conn)
{
	amqp_rpc_reply_t ret;

	int frame_size = (glbl.GetMaxLine()<130000 ? 131072 : (glbl.GetMaxLine()+1072));

	ret = amqp_login(a_conn, (char const *)self->pData->vhost, 1, frame_size, self->pData->heartbeat,
			AMQP_SASL_METHOD_PLAIN, self->pData->user, self->pData->password);

	if (ret.reply_type != AMQP_RESPONSE_NORMAL)
	{
		LogError(0, RS_RET_RABBITMQ_LOGIN_ERR, "omrabbitmq module %d/%d: login to AMQP server %s failed.",
				self->iidx, self->widx, self->serverActive->s.host);
		return 0;
	}

	amqp_channel_open(a_conn, 1);

	if (amqp_get_rpc_reply(a_conn).reply_type != AMQP_RESPONSE_NORMAL)
	{
		LogError(0, RS_RET_RABBITMQ_CHANNEL_ERR, "omrabbitmq module %d/%d: open channel failed.",
				self->iidx, self->widx);
		return 0;
	}

	amqp_maybe_release_buffers(a_conn);

	return 1;
}

static amqp_connection_state_t tryConnection(wrkrInstanceData_t *self, server_t *server)
{
	int retconn = 0;
	struct timeval delay;
	delay.tv_sec = 1;
	delay.tv_usec = 0;

	amqp_connection_state_t a_conn = amqp_new_connection();
	amqp_socket_t *sockfd = (a_conn) ? amqp_tcp_socket_new(a_conn) : NULL;

	if (sockfd)
#if defined(_AIX)
		retconn = amqp_socket_open(sockfd, server->host, server->port);
#else
		retconn = amqp_socket_open_noblock(sockfd, (const char*)server->host, server->port, &delay);
#endif

	if (retconn == AMQP_STATUS_OK && amqp_authenticate(self, a_conn))
		return a_conn;

	amqp_destroy_connection(a_conn);

	return NULL;
}

/**
 * afamqp_dd_run_connection_thread:
 *
 * This is the thread connection to RabbitMQ.
 **/
static void* run_connection_routine(void* arg)
{
	wrkrInstanceData_t *self = (wrkrInstanceData_t *) arg;
	amqp_frame_t frame;
	int result;
	rsRetVal state_out = RS_RET_SUSPENDED;

	dbgSetThrdName((uchar*)"amqp connection");

	d_pthread_mutex_lock(&self->send_mutex);

	self->thread_running = 1;

	pthread_cond_signal(&self->cond);

	self->state = RS_RET_OK;

	srSleep(0,100);

	DBGPRINTF("omrabbitmq module %d/%d: connection thread started\n", self->iidx, self->widx);

	int go_on = self->go_on;

	while (go_on) // this loop is used to reconnect on connection failure
	{
		if (self->a_conn != NULL)
		{
			amqp_connection_close(self->a_conn, 200);
			amqp_destroy_connection(self->a_conn);
		}

		self->a_conn = NULL;

		if (self->go_on)
		{
			if (self->serverActive == &self->serverRescue) {
				self->serverRescue.failures = 0;
				self->serverPrefered.failures = 0;
				self->serverActive = &self->serverPrefered;
			}

			struct timeval delay;
			delay.tv_sec = 1;
			delay.tv_usec = 0;

			do {
				if ((self->a_conn = tryConnection(self, &(self->serverActive->s))) != NULL) {
					self->serverActive->failures = 0;
				} else {
					self->serverActive->failures++;

					/* if 3 tries */
					if (self->serverActive->failures == 3) {
						/* on usual server switch to backup server */
						if (self->serverActive == &self->serverPrefered &&
								self->serverRescue.s.host)
							self->serverActive = &self->serverRescue;
					}
				}
			}
			while (self->a_conn == NULL && self->serverActive->failures < 3);
	
			if (self->a_conn == NULL)
			{
				LogError(0, RS_RET_RABBITMQ_CONN_ERR, "omrabbitmq module connection failed "
						"3 times on each server.");
			}
			else
			{
				int connected = 1;

				DBGPRINTF("omrabbitmq module %d: connected.\n", self->iidx);

				self->state = RS_RET_OK;

				if (self->serverActive == &self->serverRescue)
					self->last_failback = time(NULL);

				while (connected) // this loop is used to manage a connection
				{
					d_pthread_mutex_unlock(&self->send_mutex);

					if (self->serverActive == &self->serverRescue)
					{
						amqp_connection_state_t new_conn;

						delay.tv_sec = next_check(&self->failback_policy, self->last_failback);
						delay.tv_usec = 0;

						result = amqp_simple_wait_frame_noblock(self->a_conn, &frame, &delay);

						/* if connected to rescue server then check if usual server is alive.
						 * if so then disconnect from rescue */
						if (result == AMQP_STATUS_TIMEOUT &&
								(new_conn = tryConnection(self, &(self->serverPrefered.s)))
								!= NULL) {
							amqp_connection_state_t old_conn = self->a_conn;
							d_pthread_mutex_lock(&self->send_mutex);
							self->a_conn = new_conn;
							self->serverActive = &self->serverPrefered;
							self->serverActive->failures = 0;
							d_pthread_mutex_unlock(&self->send_mutex);
							DBGPRINTF("omrabbitmq module %d: reconnects to usual server.\n",
										self->iidx);
							amqp_connection_close(old_conn, 200);
							amqp_destroy_connection(old_conn);
						}

					} else {

						result = amqp_simple_wait_frame(self->a_conn, &frame);

					}

					d_pthread_mutex_lock(&self->send_mutex);

					switch (result)
					{
					case AMQP_STATUS_TIMEOUT:
						break;
					case AMQP_STATUS_NO_MEMORY:
						LogError(0, RS_RET_OUT_OF_MEMORY, "omrabbitmq module %d/%d: no memory "
							": aborting module.", self->iidx, self->widx);
						go_on = 0; /* non recoverable error let's go out */
						connected = 0;
						state_out = RS_RET_DISABLE_ACTION;
						break;
					case AMQP_STATUS_BAD_AMQP_DATA:
						LogError(0, RS_RET_RABBITMQ_CONN_ERR, "omrabbitmq module %d/%d: bad "
							"data received : reconnect.", self->iidx, self->widx);
						connected = 0;
						break;
					case AMQP_STATUS_SOCKET_ERROR:
						LogError(0, RS_RET_RABBITMQ_CONN_ERR, "omrabbitmq module %d/%d: Socket"
							" error : reconnect.", self->iidx, self->widx);
						connected = 0;
						break;
					case AMQP_STATUS_CONNECTION_CLOSED:
						LogError(0, RS_RET_OUT_OF_MEMORY, "omrabbitmq module %d/%d: Connection"
							" closed : reconnect.", self->iidx, self->widx);
						connected = 0;
						break;
					case AMQP_STATUS_OK:
						// perhaps not a frame type so ignore it
						if (frame.frame_type == AMQP_FRAME_METHOD)
						{
							// now handle other responses or methods from the server
							switch (frame.payload.method.id)
							{
							case AMQP_CONNECTION_CLOSE_OK_METHOD:
								connected = 0;
								go_on = 0;
								break;
							case AMQP_CHANNEL_CLOSE_METHOD:
								LogMsg(0, RS_RET_OK, LOG_WARNING,
										"omrabbitmq module %d/%d: Close Channel Received (%X).",
										self->iidx, self->widx, frame.payload.method.id);
								{
									amqp_channel_close_ok_t req;
									req.dummy = '\0';
									// send the method
									amqp_send_method(self->a_conn, frame.channel,
												AMQP_CHANNEL_CLOSE_OK_METHOD, &req);
								}
								break;
							case AMQP_CONNECTION_CLOSE_METHOD:
								LogMsg(0, RS_RET_OK, LOG_WARNING,
										"omrabbitmq module %d/%d: Close Connection Received (%X).",
										self->iidx, self->widx, frame.payload.method.id);
								{
								 amqp_connection_close_ok_t req;
								 req.dummy = '\0';
								 // send the method
								 amqp_send_method(self->a_conn, frame.channel,
										AMQP_CONNECTION_CLOSE_OK_METHOD, &req);
								}
								connected = 0;
								break;
							default :
								LogMsg(0, RS_RET_OK, LOG_WARNING,
									"omrabbitmq module %d/%d: unmanaged amqp method"
									" received (%X) : ignored.",
									self->iidx, self->widx, frame.payload.method.id);
							} /* switch (frame.payload.method.id) */
						}
						break;
					} /* switch (result) */
				} /* if (frame.frame_type == AMQP_FRAME_METHOD) */
			} /* if (sockfd_ret != AMQP_STATUS_OK) */
		} /* if (go_on) */
		else
		{
			go_on = 0;
			state_out = RS_RET_DISABLE_ACTION;
		}
	}
	self->state = state_out;

	if (self->a_conn != NULL)
	{
		amqp_connection_close(self->a_conn, 200);
		amqp_destroy_connection(self->a_conn);
	}
	self->a_conn = NULL;

	d_pthread_mutex_unlock(&self->send_mutex);

	self->thread_running = 0;

	pthread_cond_signal(&self->cond);

	return NULL;
}

/* ============================================================================================
 * Main thread
 * ============================================================================================
 */

static rsRetVal startAMQPConnection(wrkrInstanceData_t *self)
{
	DEFiRet;
	d_pthread_mutex_lock(&self->cond_mutex);
	self->go_on = 1;
	if (self->thread_running == 0)
	{
		if (!pthread_create(&self->thread, NULL, run_connection_routine, self))
		{
			d_pthread_cond_wait(&self->cond,&self->cond_mutex);

		}
		iRet = self->state;
	}
	d_pthread_mutex_unlock(&self->cond_mutex);
	RETiRet;
}

static void closeAMQPConnection(wrkrInstanceData_t *self)
{
	if (!self || !self->a_conn) return;

	amqp_channel_close_t req;
	void *ret;

	req.reply_code = 200;
	req.reply_text.bytes = "200";
	req.reply_text.len = 3;
	req.class_id = 0;
	req.method_id = 0;

	d_pthread_mutex_lock(&self->send_mutex);

	self->go_on = 0;

	// send the method
	if (self->a_conn)
			amqp_send_method(self->a_conn, 0, AMQP_CONNECTION_CLOSE_METHOD, &req);

	d_pthread_mutex_unlock(&self->send_mutex);

	pthread_join(self->thread, &ret);
}

/*
 * Report general error
 */
static int manage_error(int x, char const *context)
{
	int retVal = 0; // false

	if (x < 0) {
#if (AMQP_VERSION_MINOR >= 4)
		const char *errstr = amqp_error_string2(-x);
		LogError(0, RS_RET_ERR, "omrabbitmq: %s: %s", context, errstr);
#else
		char *errstr = amqp_error_string(-x);
		LogError(0, RS_RET_ERR, "omrabbitmq: %s: %s", context, errstr);
		free(errstr);
#endif
		retVal = 1; // true
	}

	return retVal;
}

typedef struct _msg2amqp_props_ {
	propid_t id;
	char *name;
	amqp_bytes_t *standardprop;
	int flag;
	} msg2amqp_props_t;

static rsRetVal publishRabbitMQ(wrkrInstanceData_t *self, amqp_bytes_t exchange, 
		amqp_bytes_t routing_key, amqp_basic_properties_t *p_amqp_props,
		amqp_bytes_t body_bytes)
{
	DEFiRet;
	d_pthread_mutex_lock(&self->send_mutex);
	if (self->state != RS_RET_OK)
		 ABORT_FINALIZE(self->state);

	if (!self->a_conn){
		ABORT_FINALIZE(RS_RET_RABBITMQ_CONN_ERR);
	}

	if (manage_error(amqp_basic_publish(self->a_conn, 1,
	    exchange, routing_key,
			0, 0, p_amqp_props, body_bytes), "amqp_basic_publish")) {
	}
finalize_it:
	d_pthread_mutex_unlock(&self->send_mutex);
	RETiRet;
}

BEGINdoAction
	int iLen;
CODESTARTdoAction
	/* this is where you receive the message and need to carry out the
	 * action. Data is provided in ppString[i] where 0 <= i <= num of strings
	 * requested.
	 * Return RS_RET_OK if all goes well, RS_RET_SUSPENDED if the action can
	 * currently not complete, or an error code or RS_RET_DISABLED. The later
	 * two should only be returned if there is no hope that the action can be
	 * restored unless an rsyslog restart (prime example is an invalid config).
	 * Error code or RS_RET_DISABLED permanently disables the action, up to
	 * the next restart.
	 */
	smsg_t *msg = (smsg_t*)ppString[0];

	amqp_bytes_t body_bytes;
	amqp_basic_properties_t *amqp_props_msg;

	if (!pWrkrData->pData->idx_body_template || (msg->msgFlags & KEEP_FORMAT))
	{
		getRawMsg(msg, (uchar**)(&body_bytes.bytes), &iLen);
		body_bytes.len = (size_t)iLen;
		amqp_props_msg = &pWrkrData->pData->amqp_props_plaintext;
	}
	else
	{
		body_bytes = cstring_bytes((char*)ppString[pWrkrData->pData->idx_body_template]);
		amqp_props_msg = &pWrkrData->pData->amqp_props_tpl_type;
	}

	if (pWrkrData->pData->populate_properties) {
		msgPropDescr_t pProp;
		int i, custom = 0;
		amqp_basic_properties_t amqp_props;

		memcpy(&amqp_props, amqp_props_msg, sizeof(amqp_basic_properties_t));

		msg2amqp_props_t prop_list[] = {
				{ PROP_SYSLOGFACILITY_TEXT, "facility",  NULL, 0 },
				{ PROP_SYSLOGSEVERITY_TEXT, "severity",  NULL, 0 },
				{ PROP_TIMESTAMP, "timestamp", NULL, 0 },
				{ PROP_HOSTNAME, "hostname", NULL, 0 },
				{ PROP_FROMHOST, "fromhost", NULL, 0 },
				{ PROP_SYSLOGTAG, NULL, &(amqp_props.app_id), AMQP_BASIC_APP_ID_FLAG }
		};
		int len = sizeof(prop_list)/sizeof(msg2amqp_props_t);
		uchar *val[sizeof(prop_list)/sizeof(msg2amqp_props_t)];
		rs_size_t valLen[sizeof(prop_list)/sizeof(msg2amqp_props_t)];
		unsigned short mustBeFreed[sizeof(prop_list)/sizeof(msg2amqp_props_t)];
		struct amqp_table_entry_t_ tab_entries[sizeof(prop_list)/sizeof(msg2amqp_props_t)];

		amqp_props.headers.entries = tab_entries;

		for (i=0; i<len; i++)
		{
			pProp.id = prop_list[i].id;
			valLen[i] = 0;
			mustBeFreed[i] = 0;
			val[i] = (uchar*)MsgGetProp(msg, NULL, &pProp, &(valLen[i]),
						&(mustBeFreed[i]), NULL);
			if (val[i] && *val[i])
			{
				if (prop_list[i].name && amqp_props.headers.entries)
				{/* custom */
					tab_entries[custom].key = amqp_cstring_bytes(prop_list[i].name);
					tab_entries[custom].value.kind = AMQP_FIELD_KIND_UTF8;
					tab_entries[custom].value.value.bytes = amqp_cstring_bytes((char*)val[i]);
					amqp_props._flags |= AMQP_BASIC_HEADERS_FLAG;
					custom++;
				}
				else
				{ /* standard */
					prop_list[i].standardprop->bytes = val[i];
					prop_list[i].standardprop->len = (size_t)valLen[i];
					amqp_props._flags |= prop_list[i].flag;
				}
			}
		}
		amqp_props.headers.num_entries = custom;

		/* CHKiRet could not be used because we need to release allocations */
		iRet = publishRabbitMQ(pWrkrData, pWrkrData->pData->exchange,
				(pWrkrData->pData->routing_key_template)?
					cstring_bytes((char*)ppString[pWrkrData->pData->idx_routing_key_template])
					: pWrkrData->pData->routing_key,
				&amqp_props, body_bytes);

		for (i=0; i<len; i++)
			if (mustBeFreed[i]) free(val[i]);
	}
	else
	{
		/* As CHKiRet could not be used earlier, iRet is directly used again */
		iRet = publishRabbitMQ(pWrkrData, pWrkrData->pData->exchange,
				(pWrkrData->pData->routing_key_template)?
					cstring_bytes((char*)ppString[pWrkrData->pData->idx_routing_key_template])
					: pWrkrData->pData->routing_key,
				amqp_props_msg, body_bytes);
	}

ENDdoAction

BEGINtryResume
CODESTARTtryResume
	iRet = startAMQPConnection(pWrkrData);
ENDtryResume

BEGINcreateInstance
CODESTARTcreateInstance
	memset(pData, 0, sizeof(instanceData));
	pData->iidx = ++instance_counter;
	pData->delivery_mode = 2;
	pData->heartbeat = HEARTBEAT_TIME;
ENDcreateInstance

BEGINfreeInstance
CODESTARTfreeInstance
	/* this is a cleanup callback. All dynamically-allocated resources
	 * in instance data must be cleaned up here. Prime examples are
	 * malloc()ed memory, file & database handles and the like.
	 */
	if (pData->exchange.bytes) free(pData->exchange.bytes);
	if (pData->routing_key.bytes) free(pData->routing_key.bytes);
	if (pData->routing_key_template) free(pData->routing_key_template);
	if (pData->body_template) free(pData->body_template);
	if (pData->expiration.bytes) free(pData->expiration.bytes);
	if (pData->content_type) free(pData->content_type);
	if (pData->server1.host) free(pData->server1.host);
	if (pData->vhost) free(pData->vhost);
	if (pData->user) free(pData->user);
	if (pData->password) free(pData->password);
ENDfreeInstance

BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	/* use this to specify if select features are supported by this
	 * plugin. If not, the framework will handle that. Currently, only
	 * RepeatedMsgReduction ("last message repeated n times") is optional.
	 */
	if(eFeat == sFEATURERepeatedMsgReduction)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature

BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	/* permits to spit out some debug info */
	dbgprintf("omrabbitmq instance : %d\n", pData->iidx);
	if (pData->server2.host) {
		dbgprintf("\thost1='%s' \n", pData->server1.host);
		dbgprintf("\tport1=%d\n", pData->server1.port);
		dbgprintf("\thost2='%s' \n", pData->server2.host);
		dbgprintf("\tport2=%d\n", pData->server2.port);
		dbgprintf("\tfailback policy :");
		dbgprintf("\t\tusual server check interval=%ld s", pData->failback_policy.return_check_interval);
		dbgprintf("\t\tquick oscillation limit=%ld s", pData->failback_policy.quick_oscillation_interval);
		dbgprintf("\t\tmax number of oscillation=%d s", pData->failback_policy.quick_oscillation_max);
		dbgprintf("\t\tgraceful interval after quick oscillation detection=%ld s", 
				pData->failback_policy.graceful_interval);
	}else{
		dbgprintf("\thost='%s' \n", pData->server1.host);
		dbgprintf("\tport=%d\n", pData->server1.port);
	}
	dbgprintf("\tvirtual_host='%s'\n", pData->vhost);
	dbgprintf("\tuser='%s'\n",  pData->user == NULL ? "(not configured)" : pData->user);
	dbgprintf("\tpassword=(%sconfigured)\n", pData->password == NULL ? "not " : "");

	dbgprintf("\texchange='%*s'\n", (int)pData->exchange.len, (char*)pData->exchange.bytes);
	dbgprintf("\trouting_key='%*s'\n", (int)pData->routing_key.len, (char*) pData->routing_key.bytes);
	dbgprintf("\trouting_key_template='%s'\n", pData->routing_key_template);
	dbgprintf("\tbody_template='%s'\n", pData->body_template);
	dbgprintf("\texchange_type='%s'\n", pData->exchange_type);
	dbgprintf("\tauto_delete=%d\n", pData->auto_delete);
	dbgprintf("\tdurable=%d\n", pData->durable);
	dbgprintf("\tpopulate_properties=%s\n", (pData->populate_properties)?"ON":"OFF");
	dbgprintf((pData->delivery_mode == 1) ? "\tdelivery_mode=TRANSIENT\n": "\tdelivery_mode=PERSISTANT\n");
	dbgprintf((pData->expiration.len == 0) ? "\texpiration=UNLIMITED\n" : "\texpiration=%*s\n", 
			(int)pData->expiration.len, (char*) pData->expiration.bytes);
ENDdbgPrintInstInfo

BEGINnewActInst
	struct cnfparamvals *pvals;
	int i;
	char *host = NULL, *vhost= NULL, *user = NULL, *password = NULL, *failback = NULL;
	int port = 0;
	long long expiration = 0;
CODESTARTnewActInst

	if((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	CHKiRet(createInstance(&pData));

	/* let read parameters */
	for(i = 0 ; i < actpblk.nParams ; ++i) {
		if (!pvals[i].bUsed)
			continue;
		if (!strcmp(actpblk.descr[i].name, "host")) {
			host = (char*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if (!strcmp(actpblk.descr[i].name, "failback_policy")) {
			failback = es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if (!strcmp(actpblk.descr[i].name, "port")) {
			port = (int) pvals[i].val.d.n;
		} else if (!strcmp(actpblk.descr[i].name, "virtual_host")) {
			vhost = (char*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if (!strcmp(actpblk.descr[i].name, "user")) {
			user = (char*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if (!strcmp(actpblk.descr[i].name, "password")) {
			password = (char*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if (!strcmp(actpblk.descr[i].name, "exchange")) {
			pData->exchange = cstring_bytes(es_str2cstr(pvals[i].val.d.estr, NULL));
		} else if (!strcmp(actpblk.descr[i].name, "routing_key")) {
			pData->routing_key = cstring_bytes(es_str2cstr(pvals[i].val.d.estr, NULL));
		} else if (!strcmp(actpblk.descr[i].name, "routing_key_template")) {
			pData->routing_key_template = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if (!strcmp(actpblk.descr[i].name, "populate_properties")) {
			pData->populate_properties = (sbool) pvals[i].val.d.n;
		} else if (!strcmp(actpblk.descr[i].name, "delivery_mode")) {
			char *temp = (char*)es_str2cstr(pvals[i].val.d.estr, NULL);
			if (temp){
			  if (!strcasecmp(temp, "TRANSIENT") || !strcmp(temp, "1"))
					pData->delivery_mode = 1;
				else if (!strcasecmp(temp, "PERSISTANT") || !strcmp(temp, "2"))
					pData->delivery_mode = 2;
				else
				  pData->delivery_mode = 0;
				free(temp);
			}
		} else if (!strcmp(actpblk.descr[i].name, "expiration")) {
		  expiration =  pvals[i].val.d.n;
			if (expiration > 0) {
				char buf[40];
				snprintf(buf, 40, "%lld", expiration);
				pData->expiration = cstring_bytes(strdup(buf));
			}
		} else if (!strcmp(actpblk.descr[i].name, "body_template")) {
			pData->body_template = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if (!strcmp(actpblk.descr[i].name, "exchange_type")) {
			pData->exchange_type = es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if (!strcmp(actpblk.descr[i].name, "content_type")) {
			pData->content_type = es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if (!strcmp(actpblk.descr[i].name, "auto_delete")) {
			pData->auto_delete = (int) pvals[i].val.d.n;
		} else if (!strcmp(actpblk.descr[i].name, "durable")) {
			pData->durable = (int) pvals[i].val.d.n;
		} else {
		  LogError(0, RS_RET_INVALID_PARAMS, "omrabbitmq module %d: program error, non-handled param '%s'\n", 
					pData->iidx, actpblk.descr[i].name);
		}
	}

	/* let's check config validity */

	/* first if a template for routing_key is set let verify its existence */
	if (pData->routing_key_template && tplFind(ourConf, (char*)pData->routing_key_template,
					strlen((char*)pData->routing_key_template)) == NULL)
	{
		LogError(0, RS_RET_INVALID_PARAMS, "omrabbitmq module %d : template '%s' used for routing key does not exist !",
				            pData->iidx, pData->routing_key_template);
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}

	/* an exchange must be defined */
	if (pData->exchange.bytes == NULL) {
		LogError(0, RS_RET_INVALID_PARAMS, "omrabbitmq module %d disabled: parameter exchange must be specified",
					pData->iidx);
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}

	/* a static or a template's routing_key must be defined */
	if (pData->routing_key.bytes == NULL && pData->routing_key_template == NULL) {
		LogError(0, RS_RET_INVALID_PARAMS, "omrabbitmq module %d disabled: one of parameters routing_key or "
						"routing_key_template must be specified", pData->iidx);
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}

	/* a valid delivery mode must be defined : a 0 means that an invalid value has been done */
	if (!pData->delivery_mode)
	{
		LogError(0, RS_RET_CONF_PARAM_INVLD, "omrabbitmq module %d disabled: parameters delivery_mode must be "
				"TRANSIENT or PERSISTANT (default)", pData->iidx);
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}

	/* an expiration must be positive */
	if (expiration < 0) {
		LogError(0, RS_RET_CONF_PARAM_INVLD, "omrabbitmq module %d disabled: parameters expiration must be a "
		" positive integer", pData->iidx);
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}

	/* first if a template for message body is set let verify its existence */
	if (pData->body_template && tplFind(ourConf, (char*)pData->body_template, strlen((char*)pData->body_template))
		== NULL)
	{
		LogError(0, RS_RET_CONF_PARAM_INVLD, "omrabbitmq module %d : template '%s' used for body does not exist !",
				pData->iidx, pData->body_template);
		ABORT_FINALIZE(RS_RET_CONFIG_ERROR);
	}

	/* Let's define the size of the doAction tab */
	CODE_STD_STRING_REQUESTnewActInst(1 + ((pData->routing_key_template) ? 1 : 0) + 
					((pData->body_template && *pData->body_template == '\0') ? 0 : 1));

	/* Set the plain text message props */
	memset(&pData->amqp_props_plaintext, 0, sizeof(amqp_basic_properties_t));
	pData->amqp_props_plaintext._flags = AMQP_BASIC_DELIVERY_MODE_FLAG | AMQP_BASIC_CONTENT_TYPE_FLAG;
	pData->amqp_props_plaintext.delivery_mode = pData->delivery_mode; /* persistent delivery mode */
	pData->amqp_props_plaintext.content_type = amqp_cstring_bytes("plain/text");
	if (pData->expiration.len)
	{
		pData->amqp_props_plaintext._flags |= AMQP_BASIC_EXPIRATION_FLAG;
		pData->amqp_props_plaintext.expiration = pData->expiration;
	}

	memcpy(&pData->amqp_props_tpl_type, &pData->amqp_props_plaintext, sizeof(amqp_basic_properties_t));

	/* The first position of doAction tab will contain the internal message */
	CHKiRet(OMSRsetEntry(*ppOMSR, 0, NULL, OMSR_TPL_AS_MSG));

	// RabbitMQ properties initialization
	if (pData->routing_key_template)
	{
		pData->idx_routing_key_template = 1;
		CHKiRet(OMSRsetEntry(*ppOMSR, 1, (uchar*)strdup((const char *)pData->routing_key_template), OMSR_NO_RQD_TPL_OPTS));
	}

	/* if pData->body_template is NULL (not defined) then let's use former json format
	 * if pData->body_template is not an empty string then let's use it. In this case the content type is defined either
	 *  by the template name or the user defined content_type if set
	 * otherwise raw data (unformatted) are sent this is done setting pData->idx_body_template to 0 */
	if (pData->body_template == NULL)
	{ /* no template */
		DBGPRINTF("Body_template is using default StdJSONFmt definition.\n");
		pData->idx_body_template = pData->idx_routing_key_template + 1;
		CHKiRet(OMSRsetEntry(*ppOMSR, pData->idx_body_template, (uchar*)strdup(" StdJSONFmt"), OMSR_NO_RQD_TPL_OPTS));
		pData->amqp_props_tpl_type.content_type = amqp_cstring_bytes("application/json");
	}
	else if (*pData->body_template)
	{
		pData->idx_body_template = pData->idx_routing_key_template + 1;
		CHKiRet(OMSRsetEntry(*ppOMSR, pData->idx_body_template, (uchar*)strdup((const char *)pData->body_template),
					OMSR_NO_RQD_TPL_OPTS));
		pData->amqp_props_tpl_type.content_type = amqp_cstring_bytes((pData->content_type)
		                                                             ? pData->content_type
		                                                             : (char*)pData->body_template);
	}else{
		pData->idx_body_template = 0;
		pData->amqp_props_tpl_type.content_type = amqp_cstring_bytes((pData->content_type)
		                                                             ? pData->content_type
		                                                             :"raw");
	}

	/* treatment of the server parameter
	 * first the default port */
	pData->server2.port = pData->server1.port = port ? port : 5672;

	char *temp;
	int p;
	pData->server1.host = host;
	/* Is there more than one server in parameter */
	if ((pData->server2.host = strchr(pData->server1.host,' ')) != NULL)
	{
		*pData->server2.host++ ='\0';
		/* is there a port with the second server */
		if ((temp = strchr(pData->server2.host,':')) != NULL)
		{
			*temp++ ='\0';
			p = atoi(temp);
			if (p) pData->server2.port = p;
		}
	}

	/* is there a port with the first/unique server */
	if ((temp = strchr(pData->server1.host,':')) != NULL)
	{
		*temp++ ='\0';
		p = atoi(temp);
		if (p) pData->server1.port = p;
	}

	pData->vhost = vhost ? vhost : strdup("/");
	pData->user = user ? user : strdup("");
	pData->password = password ? password : strdup("");

	init_failback(&pData->failback_policy, failback);

	if (failback)
	  free(failback);

	pData->iidx =

	dbgPrintInstInfo(pData);

CODE_STD_FINALIZERnewActInst
	cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


NO_LEGACY_CONF_parseSelectorAct


BEGINmodExit
CODESTARTmodExit
	objRelease(glbl, CORE_COMPONENT);
ENDmodExit

BEGINcreateWrkrInstance
CODESTARTcreateWrkrInstance
	memset(pWrkrData, 0, sizeof(wrkrInstanceData_t));

	pWrkrData->pData = pData;

	pthread_mutex_init(&pWrkrData->send_mutex, NULL);
	pthread_mutex_init(&pWrkrData->cond_mutex, NULL);
	pthread_cond_init(&pWrkrData->cond, NULL);

	pWrkrData->state = RS_RET_SUSPENDED;
	pWrkrData->iidx = pData->iidx;
	pWrkrData->widx = ++pData->nbWrkr;

	memcpy(&(pWrkrData->failback_policy), &(pData->failback_policy), sizeof(failback_t));

	if (pData->server2.host && *pData->server2.host) {
		time_t odd = time(NULL) % 2;
		memcpy(&(pWrkrData->serverPrefered.s), (odd) ? &pData->server1 :  &pData->server2, sizeof(server_t));
		memcpy(&(pWrkrData->serverRescue.s), (odd) ? &pData->server2 :  &pData->server1, sizeof(server_t));
	}else{
		memcpy(&(pWrkrData->serverPrefered.s), &pData->server1, sizeof(server_t));
	}
	pWrkrData->serverActive = &pWrkrData->serverPrefered;

	startAMQPConnection(pWrkrData);

ENDcreateWrkrInstance

BEGINfreeWrkrInstance
CODESTARTfreeWrkrInstance

	closeAMQPConnection(pWrkrData);

	pthread_mutex_destroy(&pWrkrData->send_mutex);
	pthread_mutex_destroy(&pWrkrData->cond_mutex);
	pthread_cond_destroy(&pWrkrData->cond);

ENDfreeWrkrInstance

BEGINqueryEtryPt
CODESTARTqueryEtryPt
	CODEqueryEtryPt_STD_OMOD_QUERIES
	CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES
	CODEqueryEtryPt_STD_OMOD8_QUERIES
ENDqueryEtryPt

BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(objUse(glbl, CORE_COMPONENT));
ENDmodInit
