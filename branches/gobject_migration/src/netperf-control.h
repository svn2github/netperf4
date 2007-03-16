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

#ifndef NETPERF_CONTROL_H
#define NETPERF_CONTROL_H
/* declarations to provide us with a Control object to be used by
   netperf4's GObject conversion */

/* initially I wanted to keep netperf-control free of XML, but that is
   not terribly practical */

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

typedef struct netperf_control_msg_desc {
  xmlNodePtr  body;    /* message body */
  xmlChar    *nid;     /* the destination id */
  xmlChar    *fromnid; /* the source id */
} netperf_control_msg_desc_t;

typedef enum netperf_control_state {
  CONTROL_PREINIT,
  CONTROL_CONNECTED,
  CONTROL_ERROR,
  CONTROL_CLOSE,
} control_state_t;

/* first the instance structure. this will look remarkably like the
   old control_t structure. */

typedef struct _NetperfControl {
  /* the parent object - in this case a GObject object */
  GObject parent_instance;

  /* one wonders if this should be something other than void * */
  void *netserver; /* the object pointer of the netserver which "owns"
		      this control object. used to allow us to send it
		      signals like "connected" and the like */

  /* the rest of the control stuff goes here.  how much should be
     "public" and how much "private" I've no idea... */
  control_state_t  state;     /* the present state of the control */
  control_state_t  state_req; /* the state in which we want the control
				 to be*/
  gchar *remotehost;
  gchar *remoteport;
  gchar *localhost;
  gchar *localport;

  guint remotefamily;
  guint localfamily;

  /* private */
  /* things which I think will remain purely private */
  GIOChannel  *source;   /* the io channel over which we communicate
			    with the remote control.  */

  int  sockfd;    /* REVISIT the file descriptor associated with the
		     socket. THIS NEEDS TO CHANGE TO "SOCKET" ASAP */

  /* do we need anything to store messages which could not be sent
     initially? */

  /* stuff concerning the incomplete message we are trying to receive */
  gboolean  have_header;
  gint32    bytes_received;
  gint32    bytes_remaining;
  gchar     *buffer;

} NetperfControl;

/* second, the class structure, which is where we will put any method
   functions we wish to have and any signals we wish to define */

typedef struct _NetperfControlClass {
  /* the parent class, matching what we do with the instance */
  GObjectClass parent_class;

  /* signals */
  void (*message)(NetperfControl *control, gpointer message);
  void (*control_closed)(NetperfControl *control);
  void (*connect_control)(NetperfControl *control);

  /* methods */
} NetperfControlClass;


GType netperf_control_get_type (void);

#define TYPE_NETPERF_CONTROL (netperf_control_get_type())

#define NETPERF_CONTROL(object) \
 (G_TYPE_CHECK_INSTANCE_CAST((object), TYPE_NETPERF_CONTROL, NetperfControl))

#define NETPERF_CONTROL_CLASS(klass) \
 (G_TYPE_CHECK_CLASS_CAST((klass), TYPE_NETPERF_CONTROL, MediaClass))

#define IS_NETPERF_CONTROL(object) \
 (G_TYPE_CHECK_INSTANCE_TYPE((object), TYPE_NETPERF_CONTROL))

#define IS_NETPERF_CONTROL_CLASS(klass) \
 (G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_NETPERF_CONTROL))

#define NETPERF_CONTROL_GET_CLASS(object) \
 (G_TYPE_INSTANCE_GET_CLASS((object), TYPE_NETPERF_CONTROL, NetperfControlClass))

#endif
