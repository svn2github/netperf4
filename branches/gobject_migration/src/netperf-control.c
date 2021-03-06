/*
   netperf - network-oriented performance benchmarking

This file is part of netperf4.

Netperf4 is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

Netperf4 is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

In addition, as a special exception, the copyright holders give
permission to link the code of netperf4 with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the same
license as the "OpenSSL" library), and distribute the linked
executables.  You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".  If you modify
this file, you may extend this exception to your version of the file,
but you are not obligated to do so.  If you do not wish to do so,
delete this exception statement from your version.

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <arpa/inet.h>

#include <glib-object.h>
#include <stdio.h>
#include "netperf-control.h"
#include "netperf-netserver.h"
#include "netperf.h"

extern int debug;
extern FILE * where;
extern GHashTable *server_hash;

enum {
  CONTROL_PROP_DUMMY_0,    /* GObject does not like a zero value for a property id */
  CONTROL_PROP_ID,
  CONTROL_PROP_STATE,
  CONTROL_PROP_REQ_STATE,
  CONTROL_PROP_REMOTEHOST,
  CONTROL_PROP_REMOTEPORT,
  CONTROL_PROP_REMOTEFAMILY,
  CONTROL_PROP_LOCALHOST,
  CONTROL_PROP_LOCALPORT,
  CONTROL_PROP_LOCALFAMILY,
  CONTROL_PROP_NETSERVER,
  CONTROL_PROP_IS_NETPERF
};

/* some forward declarations to make the compiler happy regardless of
   the order in which things appear in the file */
static gboolean read_from_control_connection(GIOChannel *source,
					     GIOCondition condition,
					     gpointer data);

static void netperf_control_set_property(GObject *object,
					   guint prop_id,
					   const GValue *value,
					   GParamSpec *pspec);

static void netperf_control_get_property(GObject *object,
					   guint prop_id,
					   GValue *value,
					   GParamSpec *pspec);

static void netperf_control_class_init(NetperfControlClass *klass);

/* our control signals */
enum {
  MESSAGE,        /* this would be the control connection object
			 telling us a complete message has arrived. */
  CONTROL_CLOSED,     /* this would be the control connection object
			 telling us the control connection died. */
  CONNECT,            /* this would be the netserver telling us to
			 connect the control connection to the remote
			 */ 
  LAST_SIGNAL         /* this should always be the last in the list so
			 we can use it to size the array correctly. */
};

static void netperf_control_message(NetperfControl *control, gpointer message);
static void netperf_control_control_closed(NetperfControl *control);

static void netperf_control_connect(NetperfControl *control);

/* a place to stash the id's returned by g_signal_new should we ever
   need to refer to them by their ID. */ 

static guint netperf_control_signals[LAST_SIGNAL] = {0,0,0};

GType netperf_control_get_type(void) {
  static GType netperf_control_type = 0;

  if (!netperf_control_type) {
    static const GTypeInfo netperf_control_info = {
      sizeof(NetperfControlClass),   /* class structure size */
      NULL,                            /* base class initializer */
      NULL,                            /* base class finalizer */
      (GClassInitFunc)netperf_control_class_init, /* class initializer */
      NULL,                            /* class finalizer */
      NULL,                            /* class data */
      sizeof(NetperfControl),        /* instance structure size */
      1,                               /* number to preallocate */
      NULL,                            /* instance initializer */
      NULL                             /* function table */
    };

    netperf_control_type = 
      g_type_register_static(G_TYPE_OBJECT,      /* parent type */
			     "NetperfControl", /* type name */
			     &netperf_control_info, /* the information */
			     0);                 /* flags */
  }

  return netperf_control_type;
}

static void netperf_control_class_init(NetperfControlClass *klass)
{
  /* GParamSpecs for properties go here */
  GParamSpec *state_param;
  GParamSpec *req_state_param;
  GParamSpec *id_param;
  GParamSpec *remotehost_param;
  GParamSpec *remoteport_param;
  GParamSpec *remotefamily_param;
  GParamSpec *localhost_param;
  GParamSpec *localport_param;
  GParamSpec *localfamily_param;
  GParamSpec *netserver_param;
  GParamSpec *is_netperf_param;

  /* and on with the show */
  GObjectClass *g_object_class;

  g_object_class = G_OBJECT_CLASS(klass);

  /* we might create GParamSpec descriptions for properties here */
  state_param = 
    g_param_spec_uint("state", /* identifier */
		      "control state", /* longer name */
		      "currentstate of the control",
		      CONTROL_PREINIT, /* min value */
		      CONTROL_CLOSE,    /* max value */
		      CONTROL_PREINIT, /* dev value */
		      G_PARAM_READABLE); /* should this be read only? */
  req_state_param = 
    g_param_spec_uint("req-state", /* identifier */
		      "requested state", /* longer name */
		      "desired state of the control", /* description
							   */
		      CONTROL_PREINIT, /* min value */
		      CONTROL_CLOSE,   /* max value */
		      CONTROL_PREINIT, /* def value */
		      G_PARAM_READWRITE); 

  id_param = 
    g_param_spec_string("id",         /* identifier */
			"identifier", /* longer name */
			"unique control identifier", /* description */
			"unnamed",    /* default value */
			G_PARAM_READWRITE);

  remotehost_param = 
    g_param_spec_string("remotehost",
			"remote host",
			"remote hostname or IP address",
			"localhost",  /* belt and suspenders with the DTD */
			G_PARAM_READWRITE);
  remoteport_param = 
    g_param_spec_string("remoteport",
			"remote port",
			"remote port number",
			"56821",  /* belt and suspenders with the DTD */
			G_PARAM_READWRITE);
  remotefamily_param = 
    g_param_spec_uint("remotefamily",
		      "remote family",
		      "remote address family",
		      0,
		      INT_MAX,
		      AF_UNSPEC,
		      G_PARAM_READWRITE);

  localhost_param = 
    g_param_spec_string("localhost",
			"local host",
			"local hostname or IP address",
			"localhost",  /* belt and suspenders with the DTD */
			G_PARAM_READWRITE);

  localport_param = 
    g_param_spec_string("localport",
			"local port",
			"local port name or number",
			"0",  /* belt and suspenders with the DTD */
			G_PARAM_READWRITE);
  localfamily_param = 
    g_param_spec_uint("localfamily",
		      "local family",
		      "local IP address family",
		      0,
		      INT_MAX,
		      AF_UNSPEC,
		      G_PARAM_READWRITE);

  netserver_param = 
    g_param_spec_pointer("netserver",
			 "owning netserver",
			 "pointer to the owning netserver object",
			 G_PARAM_READWRITE);

  is_netperf_param = 
    g_param_spec_boolean("is_netperf",
			 "is netperf side?",
			 "is this a netperf-side control object or netserver?",
			 TRUE,
			 G_PARAM_READWRITE);
			 
  /* overwrite the base object methods with our own get and set
     property routines */

  g_object_class->set_property = netperf_control_set_property;
  g_object_class->get_property = netperf_control_get_property;

  /* we would install our properties here */
  g_object_class_install_property(g_object_class,
				  CONTROL_PROP_STATE,
				  state_param);

  g_object_class_install_property(g_object_class,
				  CONTROL_PROP_REQ_STATE,
				  req_state_param);

  g_object_class_install_property(g_object_class,
				  CONTROL_PROP_ID,
				  id_param);

  g_object_class_install_property(g_object_class,
				  CONTROL_PROP_REMOTEHOST,
				  remotehost_param);

  g_object_class_install_property(g_object_class,
				  CONTROL_PROP_REMOTEPORT,
				  remoteport_param);

  g_object_class_install_property(g_object_class,
				  CONTROL_PROP_REMOTEFAMILY,
				  remotefamily_param);

  g_object_class_install_property(g_object_class,
				  CONTROL_PROP_LOCALHOST,
				  localhost_param);

  g_object_class_install_property(g_object_class,
				  CONTROL_PROP_LOCALPORT,
				  localport_param);

  g_object_class_install_property(g_object_class,
				  CONTROL_PROP_LOCALFAMILY,
				  localfamily_param);

  g_object_class_install_property(g_object_class,
				  CONTROL_PROP_NETSERVER,
				  netserver_param);

  g_object_class_install_property(g_object_class,
				  CONTROL_PROP_IS_NETPERF,
				  is_netperf_param);

  /* we would set the signal handlers for the class here. we might
     have a signal say for the arrival of a complete message or
     perhaps the cotnrol connection going down or somesuch. */

  klass->message = netperf_control_message;
  klass->control_closed = netperf_control_control_closed;
  klass->connect_control = netperf_control_connect;

  netperf_control_signals[MESSAGE] = 
    g_signal_new("message",            /* signal name */
		 TYPE_NETPERF_CONTROL,   /* class type id */
		 G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED, /* options,
							   not sure if
							   G_SIGNAL_DETAILED
							   is
							   indicated */
		 G_STRUCT_OFFSET(NetperfControlClass, message),
		 NULL,                     /* no accumulator */
		 NULL,                     /* so no accumulator data */
		 g_cclosure_marshal_VOID__POINTER, /* return void,
						      take pointer */
		 G_TYPE_NONE,              /* type of return value */
		 1,                        /* one additional parameter */
		 G_TYPE_POINTER);          /* and its type */
  netperf_control_signals[CONTROL_CLOSED] = 
    g_signal_new("control_closed",
		 TYPE_NETPERF_CONTROL,
		 G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
		 G_STRUCT_OFFSET(NetperfControlClass, control_closed),
		 NULL,
		 NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE,
		 0);
  netperf_control_signals[CONTROL_CLOSED] = 
    g_signal_new("connect",
		 TYPE_NETPERF_CONTROL,
		 G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
		 G_STRUCT_OFFSET(NetperfControlClass, connect_control),
		 NULL,
		 NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE,
		 0);
}

/* signal handler for new message */
static void netperf_control_message(NetperfControl *control, gpointer message)
{
  netperf_control_msg_desc_t *descriptor = message;

  g_print("Control object %p was requested to send a message from descriptor %p\n",
	  control,
	  message);

  int rc;
  int32_t length;

  gchar *chars_to_send;

  xmlDocPtr doc;
  xmlDtdPtr dtd;
  xmlNodePtr message_header;
  xmlNodePtr new_node;
  char *control_message;
  int  control_message_len;
  gsize bytes_written;
  GError *error = NULL;
  GIOStatus status;

  if ((doc = xmlNewDoc((xmlChar *)"1.0")) != NULL) {
    /* zippity do dah */
    doc->standalone = 0;
    dtd = xmlCreateIntSubset(doc,(xmlChar *)"message",NULL,NETPERF_DTD_FILE);
    if (dtd != NULL) {
      if (((message_header = xmlNewNode(NULL,(xmlChar *)"message")) != NULL) &&
          (xmlSetProp(message_header,(xmlChar *)"tonid",descriptor->nid) != NULL) &&
          (xmlSetProp(message_header,(xmlChar *)"fromnid",descriptor->fromnid) != NULL)) {
        /* zippity ay */
        xmlDocSetRootElement(doc,message_header);
        /* it certainly would be nice to not have to do this with two
           calls... raj 2003-02-28 */
        if (((new_node = xmlDocCopyNode(descriptor->body,doc,1)) != NULL) &&
            (xmlAddChild(message_header, new_node) != NULL)) {
	  /* IF there were a call where I could specify the buffer,
	     then we wouldn't have to copy this again to get things
	     contiguous with the "header" - then again, if the glib IO
	     channel stuff offered a gathering write call it wouldn't
	     matter... raj 2006-03-24 */
          xmlDocDumpMemory(doc,
                           (xmlChar **)&control_message,
                           &control_message_len);
          if (control_message_len > 0) {
            /* what a wonderful day */

	    length = control_message_len;

	    if (debug) {
	      g_fprintf(where,
			"%s allocating %d bytes\n",
			__func__,
			length+NETPERF_MESSAGE_HEADER_SIZE);
	    }

	    chars_to_send = g_malloc(length+NETPERF_MESSAGE_HEADER_SIZE);

	    /* if glib IO channels offered a gathering write, this
	       silliness wouldn't be necessary.  yes, they offer
	       buffered I/O and I could do two writes and then a
	       flush, but dagnabit, if I could just call sendmsg()
	       before, so no extra copies, no flushes, it certainly
	       would be nice to be able to do the same with an IO
	       channel. raj 2006-03-24 */

            /* the message length send via the network does not include
               the length itself... raj 2003-02-27 */
            length = htonl(strlen(control_message));

	    /* first copy the "header" ... */
	    memcpy(chars_to_send,
		   &length,
		   NETPERF_MESSAGE_HEADER_SIZE);
	    if (debug) {
	      g_fprintf(where,
			"%s copied %d bytes to %p\n",
			__func__,
			NETPERF_MESSAGE_HEADER_SIZE,
			chars_to_send);
	    }
	    /* ... now copy the "data" */
	    memcpy(chars_to_send+NETPERF_MESSAGE_HEADER_SIZE,
		   control_message,
		   control_message_len);

	    if (debug) {
	      g_fprintf(where,
			"%s copied %d bytes to %p\n",
			__func__,
			control_message_len,
			chars_to_send+NETPERF_MESSAGE_HEADER_SIZE);
	    }

	    /* and finally, send the data */
            status = g_io_channel_write_chars(control->source,
					      chars_to_send,
					      control_message_len +
					        NETPERF_MESSAGE_HEADER_SIZE,
					      &bytes_written,
					      &error);

            if (debug) {
              /* first display the header */
              fprintf(where, "Sending %d byte message\n",
                      control_message_len);
              fprintf(where, "|%*s| ",control_message_len,control_message);
              fflush(where);
            }
            if (bytes_written == 
		(control_message_len+NETPERF_MESSAGE_HEADER_SIZE)) {
	      if (debug) {
		fprintf(where,"was successful\n");
	      }
              rc = NPE_SUCCESS;
            } else {
              rc = NPE_SEND_CTL_MSG_FAILURE;
	      if (debug) {
		fprintf(where,"failed\n");
	      }
            }
	    g_free(chars_to_send);
	    /* this may not be the 100% correct place for this */
	    xmlFreeDoc(doc);
	    free(control_message);
          } else {
            rc = NPE_SEND_CTL_MSG_XMLDOCDUMPMEMORY_FAILED;
          }
        } else {
          rc = NPE_SEND_CTL_MSG_XMLCOPYNODE_FAILED;
        }
      } else {
        rc = NPE_SEND_CTL_MSG_XMLNEWNODE_FAILED;
      }
    } else {
      rc = NPE_SEND_CTL_MSG_XMLNEWDTD_FAILED;
    }
  } else {
    rc = NPE_SEND_CTL_MSG_XMLNEWDOC_FAILED;
  }
  /* the signaller allocated, we free the descriptor - do we also free
     any of the contents? */
  g_free(descriptor);
  return;
}

/* signal handler for control connection closed */
static void netperf_control_control_closed(NetperfControl *control)
{
  g_print("The control connection for control object %p has been closed\n",
	  control);

  return;
}

static void netperf_control_connect(NetperfControl *control)
{
  GIOStatus   status;
  GError      *error = NULL;

  g_print("asked to connect control at %p\n",control);

  control->sockfd = establish_control(control->remotehost,
				      control->remoteport,
				      control->remotefamily,
				      control->localhost,
				      control->localport,
				      control->localfamily);

  /* REVISIT - do the g_io_channel stuff here */
#ifdef G_OS_WIN32
  control->source = g_io_channel_win32_new_socket(control->sockfd);
#else
  control->source = g_io_channel_unix_new(control->sockfd);
#endif
  status = g_io_channel_set_flags(control->source,
				  G_IO_FLAG_NONBLOCK,
				  &error);
  if (error) {
    g_warning("g_io_channel_set_flags %s %d %s\n",
	      g_quark_to_string(error->domain),
	      error->code,
	      error->message);
    g_clear_error(&error);
  }
  
  status = g_io_channel_set_encoding(control->source,NULL,&error);
  if (error) {
    g_warning("g_io_channel_set_encoding %s %d %s\n",
	      g_quark_to_string(error->domain),
	      error->code,
	      error->message);
    g_clear_error(&error);
  }
  
  g_io_channel_set_buffered(control->source,FALSE);
  
  control->state     = CONTROL_CONNECTED;
  control->state_req = CONTROL_CONNECTED;
  
  /* we need to add a watch to the main event loop so we can start
     pulling messages from the socket */
  control->watch_id = g_io_add_watch(control->source,
				     G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP,
				     read_from_control_connection,
				     control);
  g_print("ho! adding a watch_id returned %u\n",control->watch_id);

  /* REVISIT this should be conditional on all the control connection stuff
     being successful */
  g_signal_emit_by_name(control->netserver,"control_connected");

}

/* get and set property routines */
static void netperf_control_set_property(GObject *object,
					   guint prop_id,
					   const GValue *value,
					   GParamSpec *pspec) {
  NetperfControl *control;
  guint req_state;

  control = NETPERF_CONTROL(object);

  switch(prop_id) {
  case CONTROL_PROP_STATE:
    g_print("naughty naughty, trying to set the state. rick should make that readonly \n");
    break;
  case CONTROL_PROP_REQ_STATE:
    req_state = g_value_get_uint(value);
    /* we really need to sanity check the reqeusted state against the
       current state here... */
    control->state_req = req_state;
    break;

  case CONTROL_PROP_REMOTEHOST:
    control->remotehost = g_value_dup_string(value);
    break;

  case CONTROL_PROP_REMOTEPORT:
    control->remoteport = g_value_dup_string(value);
    break;

  case CONTROL_PROP_REMOTEFAMILY:
    control->remotefamily = g_value_get_uint(value);
    break;

  case CONTROL_PROP_LOCALHOST:
    control->localhost = g_value_dup_string(value);
    break;

  case CONTROL_PROP_LOCALPORT:
    control->localport = g_value_dup_string(value);
    break;

  case CONTROL_PROP_LOCALFAMILY:
    control->localfamily = g_value_get_uint(value);
    break;

  case CONTROL_PROP_NETSERVER:
    control->netserver = g_value_get_pointer(value);
    break;

  case CONTROL_PROP_IS_NETPERF:
    control->is_netperf = g_value_get_boolean(value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void netperf_control_get_property(GObject *object,
					   guint prop_id,
					   GValue *value,
					   GParamSpec *pspec) {

  NetperfControl *control;

  control = NETPERF_CONTROL(object);

  switch(prop_id) {
  case CONTROL_PROP_STATE:
    g_value_set_uint(value, control->state);
    break;

  case CONTROL_PROP_REQ_STATE:
    g_value_set_uint(value, control->state_req);
    break;

  case CONTROL_PROP_REMOTEHOST:
    g_value_set_string(value, control->remotehost);
    break;

  case CONTROL_PROP_REMOTEPORT:
    g_value_set_string(value, control->remoteport);
    break;

  case CONTROL_PROP_REMOTEFAMILY:
    g_value_set_uint(value, control->remotefamily);
    break;

  case CONTROL_PROP_LOCALHOST:
    g_value_set_string(value, control->localhost);
    break;

  case CONTROL_PROP_LOCALPORT:
    g_value_set_string(value, control->localport);
    break;

  case CONTROL_PROP_LOCALFAMILY:
    g_value_set_uint(value, control->localfamily);
    break;

  case CONTROL_PROP_NETSERVER:
    g_value_set_pointer(value, control->netserver);
    break;

  case CONTROL_PROP_IS_NETPERF:
    g_value_set_boolean(value, control->is_netperf);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

/* loop and grab all the available bytes, but no more than N from the
   source and return. add the number of bytes received to the
   bytes_read parameter */
GIOStatus
read_n_available_bytes(GIOChannel *source, gchar *data, gsize n, gsize *bytes_read, GError **error) {
  GIOStatus status;

  gsize bytes_to_read;
  gsize bytes_this_read;
  gchar *buffer;

  char *foo;

  NETPERF_DEBUG_ENTRY(debug,where);

  bytes_to_read = n;
  *bytes_read = 0;
  buffer = data;

  while (((status =  g_io_channel_read_chars(source,
					     buffer,
					     bytes_to_read,
					     &bytes_this_read,
					     error)) ==
	  G_IO_STATUS_NORMAL) && (bytes_to_read > 0)) {
    *bytes_read += bytes_this_read;
    bytes_to_read -= bytes_this_read;
    buffer = buffer + *bytes_read;
    if (debug) {
      foo = data;
      g_fprintf(where,
		"%s g_io_channel_read_chars returned status %d bytes_this_read %d and error %p\n",
		__func__,
		status,
		bytes_this_read,
		error);
    }
  }
  NETPERF_DEBUG_EXIT(debug,where);
  return(status);
}


/* we call this when we get an EOF on the control channel */
static gboolean
handle_control_connection_eof(GIOChannel *source, NetperfControl *control_object) {

  /* not entirely sure what we should do here.  we _probably_ ? need
     to remove ourselves from the mainloop somehow and perhaps signal
     the netserver that we have gone belly-up? */
  /* g_main_loop_quit(global_state->loop); */
  return(FALSE);
}

gboolean
handle_control_connection_error(GIOChannel *source, gpointer data) {
  /* not sure exactly how to handle things here - a simple
     g_main_loop_quit() may not be indicated */
#ifdef notdef
  global_state_t *global_state;

  global_state = data;

  /* for now, it is rather simple - cause the mainloop to exit which
     may take quite a bit with it... */
  g_main_loop_quit(global_state->loop);
#endif

  return(FALSE);
}

/* given a buffer with a complete control message, XML parse it and
   then send it on its way. */
static gboolean
netperf_control_parse_message(NetperfControl *control) {
  
  xmlDocPtr xml_doc;
  xmlNodePtr xml_message;
  gchar *key;
  gboolean ret;
  NetperfNetserver *netserver;

  NETPERF_DEBUG_ENTRY(debug,where);

  if (debug) {
    g_fprintf(where,
	      "%s asked to parse %d byte message '%*s'\n",
	      __func__,
	      control->bytes_received,
	      control->bytes_received,
	      control->buffer);
  }

  if ((xml_doc = xmlParseMemory(control->buffer, control->bytes_received)) != NULL) {
    /* got the message, run with it */
    if (debug) {
      g_fprintf(where,
		"%s:xmlParseMemory parsed a %d byte message at %p giving doc at %p\n",
		__func__,
		control->bytes_received,
		control->buffer,
		xml_doc);
    }
#ifdef notdef
    /* we used to see if this was the first message on the control
       connection, which was related to this routine being used only
       by netserver.c and not netperf.c et al.  we will have to
       figure-out something else to do in that case. */
    /* was this the first message on the control connection? */
    if (global_state->first_message) {
      allocate_netperf(source,xml_message,data);
      global_state->first_message = FALSE;
    }
#endif

    xml_message =  xmlDocGetRootElement(xml_doc);

    /* extract the id from the to portion of the message. if the
       destination is "netperf" then it implies we are the netperf
       side (should have a sanity check there at some point) which
       means to find the netserver we really want to be looking
       based on the fromnid, not the tonid */

    if (control->is_netperf) 
      key = xmlGetProp(xml_message, (const xmlChar *)"fromnid");
    else
      key = xmlGetProp(xml_message, (const xmlChar *)"tonid");

    if (NULL != key) {
      /* lookup its destination and send it on its way. we need to be
	 certain that netserver.c has added a suitable entry to a
	 netperf_netserver_hash like netperf.c does. */

      netserver = g_hash_table_lookup(server_hash,key);

      if (debug) {
	g_fprintf(where,
		  "%s found netserver at %p in the hash using %s as the key\n",
		  __func__,
		  netserver,
		  key);
	fflush(where);
      }
      /* should this be a signal sent to the netserver object, a
	 method we invoke, or a property we attempt to set? I suspect
	 just about any of those would actually work, question is
	 which to use? if we start with either a signal or a property
	 we won't be dereferencing anything from the object pointer
	 here which I suppose is a good thing. REVISIT we used to call
	 a routine called process_message with a pointer to the server
	 structure and a pointer to the message */
      g_signal_emit_by_name(netserver,"new-message",xml_doc);
      /* REVISIT this - should we have a return value from the signal?
	 */
      ret = TRUE;
    }
    else {
      g_print("YoHo! the key was null!\n");
      ret = FALSE;
    }
  }
  else {
    if (debug) {
      g_fprintf(where,
		"%s: xmlParseMemory gagged on a %d byte message at %p\n",
		__func__,
		control->bytes_received,
		control->buffer);
    }
    ret = FALSE;
  }
  NETPERF_DEBUG_EXIT(debug,where);
  return(ret);
}

static gboolean
read_from_control_connection(GIOChannel *source, GIOCondition condition, gpointer data) {

  gsize bytes_read;
  GError *error = NULL;
  gchar *ptr;
  GIOStatus status;
  int ret;
  NetperfControl *control_object = data;

  NETPERF_DEBUG_ENTRY(debug,where);

  g_print("Yo! reading from control connection debug is %d where is %p\n",
	  debug,where);

  if (debug) {
    g_fprintf(where,
	      "%s called with source %p condition %x and data %p\n",
	      __func__,
	      source,
	      condition,
	      data);
  }
  if (control_object) {
    if (debug) {
      g_fprintf(where,
		"%s control_object have_header %d bytes_remaining %d buffer %p\n",
		__func__,
		control_object->have_header,
		control_object->bytes_remaining,
		control_object->buffer);
    }
  }
  else {
    g_error("%s called with null control_object\n",__func__);
  }


  /* ok, so here we go... */
  if (!control_object->have_header) {
    /* we still have to get the header */
    if (NULL == control_object->buffer) {
      /* the very first time round */
      if (debug) {
	g_fprintf(where,
		  "%s allocating %d byte buffer for netperf control message header\n",
		  __func__,
		  NETPERF_MESSAGE_HEADER_SIZE);
      }
      control_object->bytes_remaining = NETPERF_MESSAGE_HEADER_SIZE;
      control_object->bytes_received = 0;
      control_object->buffer = g_malloc(NETPERF_MESSAGE_HEADER_SIZE);
    }
    /* now try to grab the rest of the header */
    ptr = control_object->buffer + control_object->bytes_received;
    bytes_read = 0;
    status = read_n_available_bytes(source,
				    ptr,
				    control_object->bytes_remaining,
				    &bytes_read,
				    &error);

    /* we need some sort of error and status check here don't we? */

    if (G_IO_STATUS_EOF == status) {
      /* do something when the remote has told us they are going away.
	 it needs to be different based on whether or not we've
	 spawned from the parent */
      return(handle_control_connection_eof(source, data));
    }
    else if ((G_IO_STATUS_ERROR == status) ||
	     (NULL != error)) {
      if (debug) {
	g_fprintf(where,
		  "%s encountered an error and is unhappy\n",
		  __func__);
	exit(-1);
      }
    }

    control_object->bytes_received += bytes_read;
    control_object->bytes_remaining -= bytes_read;

    if (debug) {
      g_fprintf(where,
		"%s got %d bytes, setting bytes_recieved to %d and remaining to %d\n",
		__func__,
		bytes_read,
		control_object->bytes_received,
		control_object->bytes_remaining);
    }
    /* now, do we have the whole header? */
    if (control_object->bytes_received == NETPERF_MESSAGE_HEADER_SIZE) {
      /* setup for the message body */
      control_object->have_header = TRUE;
      memcpy(&(control_object->bytes_remaining),
	     control_object->buffer,
	     NETPERF_MESSAGE_HEADER_SIZE);
      control_object->bytes_remaining = ntohl(control_object->bytes_remaining);
      control_object->bytes_received = 0;
      if (debug) {
	g_fprintf(where,
		  "%s has a complete header, now expecting %d bytes of message_body\n",
		  __func__,
		  control_object->bytes_remaining);
      }
      g_free(control_object->buffer);
      /* allocate an extra byte to make sure we can null-terminate the
	 buffer on the off chance we do something like try to print it
	 as a string... raj 2006-03-29 */
      control_object->buffer = g_malloc(control_object->bytes_remaining+1);
    }
  }

  /* this is a separate if rather than an else clause because we want
     to possibly execute this code in addition to the "get the rest of
     the header" code. I suspect there is some way to do this with
     less actual code but for now it seems sufficient. besides, we
     want to make sure we drain the channel fully - otherwise there
     may be some issues on Windows, maybe something to do with
     possibly not getting notified of other bytes if we simply came
     back out to let the event loop call us again. raj 2006-03-17 */

  /* of course, draining the channel fully would seem to suggest we
     shouldn't stop pulling data from the channel after one message's
     worth, but until we get no data at all? */
  if (control_object->have_header) {
    bytes_read = 0;
    ptr = control_object->buffer + control_object->bytes_received;
    status = read_n_available_bytes(source,
				    ptr,
				    control_object->bytes_remaining,
				    &bytes_read,
				    &error);

    /* we need some sort of error and status check here don't we? */

    if (G_IO_STATUS_EOF == status) {
      /* do something when the remote has told us they are going away */
      return(handle_control_connection_eof(source, control_object));
    }
    else if ((G_IO_STATUS_ERROR == status) ||
	     (NULL != error)) {
      /* do something to deal with an error condition */
    }

    control_object->bytes_received += bytes_read;
    control_object->bytes_remaining -= bytes_read;

    if (0 == control_object->bytes_remaining) {
      /* we have an entire message, time to process it */
      /* make sure we are NULL terminated just in case someone tries
	 to print it as a string or something. */
      control_object->buffer[control_object->bytes_received] = '\0';

      /* should we be "signaling" here or just go ahead and pretend
	 that this routing is control object code anyway - might as
	 well for the time being */

      ret = netperf_control_parse_message(control_object);

      /* let us not forget to reset our control_object shall we?  we
	 don't really want to re-parse the same message over and over
	 again... raj 2006-03-22 */
      control_object->have_header = FALSE;
      control_object->bytes_received = 0;
      control_object->bytes_remaining = NETPERF_MESSAGE_HEADER_SIZE;
      g_free(control_object->buffer);
      control_object->buffer = NULL;
      return(ret);
    }
  }

  NETPERF_DEBUG_EXIT(debug,where);

  return(TRUE);
}
