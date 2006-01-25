

#ifndef _NETMSG_H
#define _NETMSG_H
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "netperf.h"
#include "netlib.h"

extern int send_version_message(server_t *server, const xmlChar *fromnid);
extern int process_message(server_t *server, xmlDocPtr doc);
extern int ns_version_check(xmlNodePtr msg, xmlDocPtr doc, server_t *netperf);
#endif
