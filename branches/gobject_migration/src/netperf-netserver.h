#ifndef NETPERF_NETSERVER_H
#define NETPERF_NETSERVER_H

#include <glib-object.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

/* declarations to provide us with a Netserver object to be used by
   netperf4's GObject conversion */

typedef enum netperf_netserver_state {
  NETSERVER_PREINIT,
  NETSERVER_CONNECTED,
  NETSERVER_VERS,
  NETSERVER_INIT,
  NETSERVER_WORK,
  NETSERVER_ERROR,
  NETSERVER_CLOSE,
  NETSERVER_EXIT
} netserver_state_t;

/* state machine data structure for process message */

typedef struct _NetperfNetserver NetperfNetserver;
typedef struct _NetperfNetserverClass NetperfNetserverClass;

typedef int (*netperf_msg_func_t)(xmlNodePtr msg, xmlDocPtr doc, NetperfNetserver *server);

struct netperf_msgs {
  char *msg_name;
  netperf_msg_func_t msg_func;
  unsigned int valid_states;
};

/* first the instance structure. this will look remarkably like the
   old netserver_t structure. */

struct _NetperfNetserver {
  /* the parent object - in this case a GObject object */
  GObject parent_instance;

  /* the rest of the netserver stuff goes here.  how much should be
     "public" and how much "private" I've no idea... */

  gchar   *id;         /* the identifier for this netserver
			    instance. it will be duplicated with the
			    key used in the GHash to find this
			    object. */

  gchar   *my_nid;     /* an identifier used by netserver? */

  GIOChannel  *source;   /* the io channel over which we communicate
			    with the remote netserver. should this
			    instead be a weak reference to a control
			    connection object? */

  gchar  *node;      /* the XML document node containing the
			    servers configuration data */

  netserver_state_t  state;     /* the present state of the netserver */
  netserver_state_t  state_req; /* the state in which we want the netserver
			    to be*/

  gint        err_rc;    /* error code received which caused this
			    server to enter the NETSERVER_ERROR state */
  char        *err_fn;   /* name of the routine which placed this
			    server into the NETSERVER_ERROR state*/

  struct netperf_msgs *message_state_table; /* a pointer to the table
					      of the various messages,
					      the routine to call and
					      the states in which
					      that message is
					      valid. there will be one
					      main one for a netperf
					      and one for a netserver
					      */

  /* do we need to have references to the test instances associated
     with this netserver instance? */
};

/* second, the class structure, which is where we will put any method
   functions we wish to have and any signals we wish to define */

struct _NetperfNetserverClass {
  /* the parent class, matching what we do with the instance */
  GObjectClass parent_class;

  /* signals */
  void (*new_message)(NetperfNetserver *netserver, gpointer message);
  void (*control_closed)(NetperfNetserver *netserver);

  /* methods */
};


GType netperf_netserver_get_type (void);

#define TYPE_NETPERF_NETSERVER (netperf_netserver_get_type())

#define NETPERF_NETSERVER(object) \
 (G_TYPE_CHECK_INSTANCE_CAST((object), TYPE_NETPERF_NETSERVER, NetperfNetserver))

#define NETPERF_NETSERVER_CLASS(klass) \
 (G_TYPE_CHECK_CLASS_CAST((klass), TYPE_NETPERF_NETSERVER, MediaClass))

#define IS_NETPERF_NETSERVER(object) \
 (G_TYPE_CHECK_INSTANCE_TYPE((object), TYPE_NETPERF_NETSERVER))

#define IS_NETPERF_NETSERVER_CLASS(klass) \
 (G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_NETPERF_NETSERVER))

#define NETPERF_NETSERVER_GET_CLASS(object) \
 (G_TYPE_INSTANCE_GET_CLASS((object), TYPE_NETPERF_NETSERVER, NetperfNetserverClass))

#endif
