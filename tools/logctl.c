/**
 * logctl - a tool to access lumberjack logs in MongoDB
 *  ... and potentially other sources in the future.
 *
 * Copyright 2012 Ulrike Gerhards and Adiscon GmbH.
 *
 * long		short	

 * level	l	read records with level x
 * severity	s	read records with severity x
 * ret		r	number of records to return
 * skip		k	number of records to skip
 * sys		y	read records of system x
 * msg		m	read records with message containing x
 * datef	f	read records starting on time received x
 * dateu	u	read records until time received x
 *
 * examples:
 *
 * logctl -f 15/05/2012-12:00:00 -u 15/05/2012-12:37:00
 * logctl -s 50 --ret 10
 * logctl -m "closed"
 * logctl -l "INFO"
 * logctl -s 3
 * logctl -y "ubuntu"
 *
 * This file is part of rsyslog.
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
 */
#include "config.h"
#include <stdio.h>
#include <mongo.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>
#include <unistd.h>

#define N 80

static struct option long_options[] = 
{ 
	{"level", required_argument, NULL, 'l'}, 
    	{"severity", required_argument, NULL, 's'}, 
    	{"ret", required_argument, NULL, 'r'},
    	{"skip", required_argument, NULL, 'k'},
    	{"sys", required_argument, NULL, 'y'},
   	{"msg", required_argument, NULL, 'm'},
    	{"datef", required_argument, NULL, 'f'},
    	{"dateu", required_argument, NULL, 'u'},
    	{NULL, 0, NULL, 0} 
}; 

struct queryopt
{
	gint32 e_sever;
	gint32 e_ret;
	gint32 e_skip;
	char *e_date;
	char *e_level;
	char *e_msg;
	char *e_sys;	
	char *e_dateu;
	int  bsever;
	int blevel;
	int bskip;
	int bret;
	int bsys;
	int bmsg;
	int bdate;
	int bdatef;
	int bdateu;
};

struct ofields
{	
	const char *msg;
	const char *syslog_tag;
	const char *prog;	
	char *date;
	gint64 date_r;
};

struct query_doc
{	
	bson *query;
};

struct select_doc
{	
	bson *select;	
};

struct db_connect
{	
	mongo_sync_connection *conn;	
};

struct output 
{	
	mongo_packet *p;	
};

struct db_cursor 
{	
	mongo_sync_cursor *cursor;	
};

struct results 
{	
	bson *result;	
};



void formater(struct ofields *fields)
{
	time_t rtime;
	rtime = (time_t) (fields->date_r / 1000);
	char str[N];
	strftime(str, N, "%b %d %H:%M:%S", gmtime(&rtime));
 	printf("%s  %s %s %s\n", str, fields->prog, fields->syslog_tag, fields->msg);

}

struct ofields* get_data(struct results *res)
{
	struct ofields *fields;
	const char *msg;
	const char *prog;
	const char *syslog_tag;
	gint64 date_r;
	bson_cursor *c;

	fields = malloc(sizeof(struct ofields));
		
	c = bson_find (res->result, "msg");		 
	if (!bson_cursor_get_string (c, &msg))
     	{
		perror ("bson_cursor_get_string()");
		exit (1);
        }	
      	bson_cursor_free (c); 

	c = bson_find (res->result, "sys");
	if (!bson_cursor_get_string (c, &prog))
        {	 	
         	perror ("bson_cursor_get_string()");
         	exit (1);
        }
      	bson_cursor_free (c);   

	c = bson_find (res->result, "syslog_tag");
     	if (!bson_cursor_get_string (c, &syslog_tag))
        {		
          	perror ("bson_cursor_get_string()");
          	exit (1);
        }
      	bson_cursor_free (c); 

	c = bson_find (res->result, "time_rcvd");
     	if (!bson_cursor_get_utc_datetime (c, &date_r))
        {
          	perror ("bson_cursor_get_utc_datetime()");
          	exit (1);
        }

 	bson_cursor_free (c);  
	fields->msg = msg;
	fields->prog = prog;
	fields->syslog_tag = syslog_tag;
	fields->date_r = date_r;

	return fields;
		
}

void getoptions(int argc, char *argv[], struct queryopt *opt)
{
	int iarg;
	while ((iarg = getopt_long(argc, argv, "l:s:r:k:y:f:u:m:", long_options, NULL)) != -1) 
	{ 
    	// check to see if a single character or long option came through 
   	switch (iarg) 
    		{ 
		 // short option 's' 
         		case 's': 
			opt->bsever = 1;
			opt->e_sever = atoi(optarg);
            		break; 
		// short option 'r' 
         		case 'r': 
			opt->bret = 1;
			opt->e_ret = atoi(optarg);
			break; 
		// short option 'f' : date from
         		case 'f': 
			opt->bdate = 1;
			opt->bdatef = 1;
			opt->e_date = optarg;
	             	break; 
		// short option 'u': date until
         		case 'u': 
			opt->bdate = 1;
			opt->bdateu = 1;
			opt->e_dateu = optarg;		
             		break; 
		// short option 'k' 
         		case 'k': 
			opt->bskip = 1;
			opt->e_skip = atoi(optarg);
			break; 
         	// short option 'l' 
         		case 'l': 
		 	opt->blevel = 1; 
            	 	opt->e_level = optarg; 
		 	break; 
	 	// short option 'm' 
         		case 'm': 
			opt->bmsg = 1; 
            	 	opt->e_msg = optarg; 
		 	break; 
		// short option 'y' 
         		case 'y': 
		 	opt->bsys = 1; 
            	 	opt->e_sys = optarg; 
		 	break; 
    		}  				// end switch iarg
	} 					// end while
	
}						// end void getoptions

struct select_doc* create_select()
// BSON object indicating the fields to return
{
	struct select_doc *s_doc;
	s_doc = malloc(sizeof(struct select_doc));
	s_doc->select = bson_new ();
	bson_append_string (s_doc->select, "syslog_tag", "s", -1);
	bson_append_string (s_doc->select, "msg", "ERROR", -1);
	bson_append_string (s_doc->select, "sys", "sys", -1);
	bson_append_utc_datetime (s_doc->select, "time_rcvd", 1ll);
	bson_finish (s_doc->select); 
	return s_doc;
} 

struct query_doc* create_query(struct queryopt *opt)
{
	struct query_doc *qu_doc;
	bson  *query_what, *order_what, *msg_what, *date_what;
	struct tm tm;
	time_t t;
	gint64  	ts;
	qu_doc = malloc(sizeof(struct query_doc));
	qu_doc->query = bson_new ();
	
	query_what = bson_new ();
	if (opt->bsever == 1)
	{
		bson_append_int32 (query_what, "syslog_sever",  opt->e_sever);
	}
	if (opt->blevel == 1)
	{
		bson_append_string (query_what, "level", opt->e_level, -1);
	}

	if (opt->bmsg == 1)
	{
		msg_what = bson_new ();
		bson_append_string (msg_what, "$regex",  opt->e_msg, -1);
		bson_append_string (msg_what, "$options",  "i", -1);
		bson_finish (msg_what);  
		bson_append_document (query_what, "msg", msg_what); 	
	}

	if (opt->bdate == 1)
	{
		date_what = bson_new ();
		if (opt->bdatef == 1)
		{
			tm.tm_isdst = -1;
			strptime(opt->e_date, "%d/%m/%Y-%H:%M:%S", &tm);
			tm.tm_hour = tm.tm_hour + 1;
			t = mktime(&tm);
			ts = 1000 * (gint64) t;
			
			bson_append_utc_datetime (date_what,"$gt", ts) ;
		}

		if (opt->bdateu == 1)
		{
			tm.tm_isdst = -1;
			strptime(opt->e_dateu, "%d/%m/%Y-%H:%M:%S", &tm);
			tm.tm_hour = tm.tm_hour +1;
			t = mktime(&tm);
			ts = 1000 * (gint64) t;
			bson_append_utc_datetime (date_what,"$lt", ts);
			
		}
		bson_finish (date_what);
		bson_append_document (query_what, "time_rcvd", date_what);
	}

	if (opt->bsys == 1)
	{
		bson_append_string (query_what, "sys", opt->e_sys, -1);
	}

	bson_finish (query_what);

	order_what = bson_new ();
	bson_append_utc_datetime (order_what, "time_rcvd", 1ll);
	bson_finish (order_what);

	bson_append_document (qu_doc->query, "$query", query_what);
	bson_append_document (qu_doc->query, "$orderby", order_what);  
	bson_finish (qu_doc->query);
	bson_free (order_what);
	return qu_doc;
} 

struct db_connect* create_conn()
{
	struct db_connect *db_conn;
	db_conn = malloc(sizeof(struct db_connect));
	db_conn->conn = mongo_sync_connect ("localhost", 27017, TRUE);

	if (!db_conn->conn)
	{
        	perror ("mongo_sync_connect()");
    		exit (1);
   	}
	return db_conn;
} 

void close_conn(struct db_connect *db_conn)
{
	mongo_sync_disconnect (db_conn->conn);
}

void free_cursor(struct db_cursor *db_c)
{
	mongo_sync_cursor_free (db_c->cursor);
}

struct output* launch_query(struct queryopt *opt, struct select_doc *s_doc,
				struct query_doc *qu_doc,
				struct db_connect *db_conn)
{
	struct output *out;
	out = malloc(sizeof(struct output));
	out->p = mongo_sync_cmd_query (db_conn->conn, "syslog.log", 0,
			    opt->e_skip, opt->e_ret, qu_doc->query, s_doc->select); 
	if (!out->p)
	{
	 	perror ("mongo_sync_cmd_query()");
     	  	printf("no records found\n");
	    	exit (1);
	}
	return out;
} 

struct db_cursor* open_cursor(struct db_connect *db_conn, struct output *out)
{
	struct db_cursor *db_c;
	db_c = malloc(sizeof(struct db_cursor));
	
	db_c->cursor = mongo_sync_cursor_new (db_conn->conn, "syslog.log", out->p);
	if (!db_c->cursor)
	{
 	     	perror ("mongo_sync_cursor_new()");
             	exit (1);
  	}
	return db_c;
} 

struct results* read_data(struct db_cursor *db_c)
{
	struct results *res;
	res = malloc(sizeof(struct results));
	res->result = mongo_sync_cursor_get_data (db_c->cursor);
	if (!res->result)
	{
	   	perror ("mongo_sync_cursor_get_data()");
	   	exit (1);
	}
	return res;
} 

gboolean cursor_next (struct db_cursor *db_c)
{
	if (!mongo_sync_cursor_next (db_c->cursor))
		return FALSE;
	else
		return TRUE;

}

int main (int argc, char *argv[])
{
	
	struct queryopt opt;
	struct ofields *fields;
	struct select_doc *s_doc;
	struct query_doc *qu_doc;
	struct db_connect *db_conn;
	struct output *out;
	struct db_cursor *db_c;
	struct results *res;

	opt.e_skip = 0;	  	  // standard
	opt.e_ret = 0;	  	  // standard
	opt.bsever  = 0;
        opt.blevel = 0;
	opt.bdate = 0;
	opt.bdateu = 0;
	opt.bdatef = 0;
	opt.bmsg = 0;
	opt.bskip = 0;
	opt.bsys = 0;
	
	getoptions(argc, argv, &opt);
	qu_doc = create_query(&opt);				// crate query
	s_doc = create_select();
	db_conn = create_conn();				// create connection
	out = launch_query(&opt, s_doc, qu_doc, db_conn);	// launch the query 
	db_c = open_cursor(db_conn, out);			// open cursor
	
	while (cursor_next(db_c))
	{
		res = read_data(db_c);
		fields = get_data(res);
        	formater(fields);				// formate output
		free(fields);
    	}

	free_cursor(db_c);
	close_conn(db_conn); 
 
	return (0);
}
