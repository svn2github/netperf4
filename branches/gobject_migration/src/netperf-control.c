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

#include <glib-object.h>
#include <stdio.h>
#include "netperf-control.h"
#include "netperf.h"

extern int debug;
extern FILE * where;

enum {
  CONTROL_PROP_DUMMY_0,    /* GObject does not like a zero value for a property id */
  CONTROL_PROP_ID,
  CONTROL_PROP_STATE,
  CONTROL_PROP_REQ_STATE
};

/* some forward declarations to make the compiler happy regardless of
   the order in which things appear in the file */
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
  NEW_MESSAGE,        /* this would be the control connection object
			 telling us a complete message has arrived. */
  CONTROL_CLOSED,     /* this would be the control connection object
			 telling us the controll connection died. */
  LAST_SIGNAL         /* this should always be the last in the list so
			 we can use it to size the array correctly. */
};

static void netperf_control_new_message(NetperfControl *control, gpointer message);
static void netperf_control_control_closed(NetperfControl *control);

/* a place to stash the id's returned by g_signal_new should we ever
   need to refer to them by their ID. */ 

static guint netperf_control_signals[LAST_SIGNAL] = {0,0};

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
		      CONTROL_CLOSE,    /* max value */
		      CONTROL_PREINIT, /* def value */
		      G_PARAM_READWRITE); 

  id_param = 
    g_param_spec_string("id",         /* identifier */
			"identifier", /* longer name */
			"unique control identifier", /* description */
			"unnamed",    /* default value */
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

  /* we would set the signal handlers for the class here. we might
     have a signal say for the arrival of a complete message or
     perhaps the cotnrol connection going down or somesuch. */

  klass->new_message = netperf_control_new_message;
  klass->control_closed = netperf_control_control_closed;

  netperf_control_signals[NEW_MESSAGE] = 
    g_signal_new("new_message",            /* signal name */
		 TYPE_NETPERF_CONTROL,   /* class type id */
		 G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED, /* options,
							   not sure if
							   G_SIGNAL_DETAILED
							   is
							   indicated */
		 G_STRUCT_OFFSET(NetperfControlClass, new_message),
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
}

/* signal handler for new message */
static void netperf_control_new_message(NetperfControl *control, gpointer message)
{
  g_print("A new message %p arrived for control object %p\n",
	  message,
	  control);

  return;
}

/* signal handler for control connection closed */
static void netperf_control_control_closed(NetperfControl *control)
{
  g_print("The control connection for control object %p has been closed\n",
	  control);

  return;
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
  guint state;

  control = NETPERF_CONTROL(object);

  switch(prop_id) {
  case CONTROL_PROP_STATE:
    g_value_set_uint(value, control->state);
    break;

  case CONTROL_PROP_REQ_STATE:
    g_value_set_uint(value, control->state_req);
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

gboolean
read_from_control_connection(GIOChannel *source, GIOCondition condition, gpointer data) {

  gsize bytes_read;
  GError *error = NULL;
  gchar *ptr;
  GIOStatus status;
  int ret;
  NetperfControl *control_object = data;

  NETPERF_DEBUG_ENTRY(debug,where);

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
      /* xml_parse_control_message, from netlib, will attempt to
	 convert the buffer into an XML document and will then parse
	 it to find the correct destination. we have it in netlib
	 because we don't want no stinkin XML here in the control
	 object code :) */
      ret = xml_parse_control_message(control_object->buffer,
				      control_object->bytes_received,
				      data,
				      source);
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
