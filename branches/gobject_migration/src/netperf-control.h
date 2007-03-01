#ifndef NETPERF_CONTROL_H
#define NETPERF_CONTROL_H
/* declarations to provide us with a Control object to be used by
   netperf4's GObject conversion */

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

  /* the rest of the control stuff goes here.  how much should be
     "public" and how much "private" I've no idea... */
  control_state_t  state;     /* the present state of the control */
  control_state_t  state_req; /* the state in which we want the control
				 to be*/

  /* private */
  /* things which I think will remain purely private */
  GIOChannel  *source;   /* the io channel over which we communicate
			    with the remote control.  */

  int  sockfd;    /* the file descriptor associated with the
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
  void (*new_message)(NetperfControl *control, gpointer message);
  void (*control_closed)(NetperfControl *control);

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
