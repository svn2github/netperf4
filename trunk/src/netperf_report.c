/*

WARNING!!!  This file is for development of a generic report generator.
            Do not use this file until this warning is removed.

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

#ifndef lint
char    netperf_report_id[]="\
@(#)netperf_report.c (c) Copyright 2007 Hewlett-Packard Co. $Id: netperf_report.c 20 2006-2-28 19:45:00Z burger $";
#endif /* lint */

/* requires 64 bit file offsets */
#define _FILE_OFFSET_BITS 64

/* turn on histogram capability */
#define WANT_HISTOGRAM


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef OFF

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIM
#include <time.h>
#endif
#endif


#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/diskio.h>
#include <sys/scsi.h>

#endif

#include "netperf.h"

#include "netperf_report.h"

/* forward declarations for local procedures */
static void print_results_for_test(report_data_t *rd, xmlChar *tid);


/* CODE which initializes and sets up report generation data structures */

static uint32_t
generate_attributes(char       *agg,
                    char       *unit,
                    char       *type,
                    char       *class)
{
  uint32_t  attr;

  attr = 0;
  if (!strcmp(agg,   "TRUE")) {
    attr = attr | CNTR_AGG_FLAG;
  }
  if (!strcmp(unit,  "bytes")) {
    attr = attr | CNTR_UNIT_BYTES;
  }
  if (!strcmp(unit,  "calls")) {
    attr = attr | CNTR_UNIT_CALLS;
  }
  if (!strcmp(unit,  "trans")) {
    attr = attr | CNTR_UNIT_TRANS;
  }
  if (!strcmp(type,  "in")) {
    attr = attr | CNTR_TYPE_IN;
  }
  if (!strcmp(type,  "out")) {
    attr = attr | CNTR_TYPE_OUT;
  }
  if (!strcmp(type,  "bidir")) {
    attr = attr | CNTR_TYPE_BIDIR;
  }
  if (!strcmp(type,  "other")) {
    attr = attr | CNTR_TYPE_OTHER;
  }
  if (!strcmp(class, "lan")) {
    attr = attr | CNTR_CLASS_LAN;
  }
  if (!strcmp(class, "disk")) {
    attr = attr | CNTR_CLASS_DISK;
  }
  if (!strcmp(class, "other")) {
    attr = attr | CNTR_CLASS_OTHER;
  }
  return(attr);
}


static void
get_sd_data_sets(report_data_t      *rd)
{
  report_data_set_t  *ds;
  xmlChar            *set;
  int                 d;
  int                 s;
  int                 c;


  c   = 0;
  ds  = rd->data_sets;
  for (s = 0; s < rd->num_data_sets; s++) {
    set = xmlGetProp(ds[s].data_set,(const xmlChar *)"sd_value_set");
    if (set) {
      for (d = 0; d < rd->num_data_sets; d++) {
        if (!xmlStrcmp(set, ds[d].name)) {
          ds[s].sd_data_set = &(ds[d]);
          c++;
          break;
        }
      }
      xmlFree(set);
    }
  }
  rd->num_sd_sets  = c;
}


static report_data_set_t*
initialize_cpu_set(report_data_t    *rd)
{
  report_data_set_t *ds;
  int                d;
  int                max;

  
  d   = rd->num_data_sets;
  ds  = rd->data_sets;
  max = rd->max_ds_values;
  ds  = &(ds[d]);
  ds->data_set   = NULL;
  ds->name       = (xmlChar *)"CPU Util";
  ds->min        = DBL_MAX;
  ds->max        = DBL_MIN;
  ds->mean       = 0.0;
  ds->interval   = 0.0;
  ds->confidence = -100.0;
  ds->values     = &(rd->values[max * d]);
  ds->attr       = 0;
  return(ds);
}


static report_data_set_t*
initialize_run_time_set(report_data_t    *rd)
{
  report_data_set_t *ds;
  int                d;
  int                max;

  d   = rd->num_data_sets + 1;
  ds  = rd->data_sets;
  max = rd->max_ds_values;
  ds  = &(ds[d]);
  ds->data_set   = NULL;
  ds->name       = (xmlChar *)"Run Time";
  ds->min        = DBL_MAX;
  ds->max        = DBL_MIN;
  ds->mean       = 0.0;
  ds->interval   = 0.0;
  ds->confidence = -100.0;
  ds->values     = &(rd->values[max * d]);
  ds->attr       = 0;
  return(ds);
}


static report_data_set_t*
get_data_set_for_field(report_data_t    *rd,
                       xmlNodePtr        field)
{
  report_data_set_t *ds;
  xmlChar           *set;
  int                d;

  ds  = rd->data_sets;
  set = xmlGetProp(field,(const xmlChar *)"data_set");
  for (d = 0; d < rd->num_data_sets; d++) {
    if (!xmlStrcmp(set, ds[d].name)) {
      ds = &(ds[d]);
      break;
    }
  }
  if (d >= rd->num_data_sets) {
    ds = NULL;
  }
  xmlFree(set);
  return(ds);
}


static double*
get_data_value(report_data_t      *rd,
	       report_data_set_t  *ds,
               xmlNodePtr          field,
               int                 per_test)
{
  double  *value_addr;
  xmlChar *value_type;

  value_addr  =  NULL;
  value_type  =  xmlGetProp(field,(const xmlChar *)"type");
  if (ds) {
    if (per_test) {
      value_addr  =  &(ds->per_test);
    }
    else if (!xmlStrcmp(value_type, (const xmlChar *)"min")) {
      value_addr  =  &(ds->min);
    }
    else if (!xmlStrcmp(value_type, (const xmlChar *)"max")) {
      value_addr  =  &(ds->max);
    }
    else if (!xmlStrcmp(value_type, (const xmlChar *)"mean")) {
      value_addr  =  &(ds->mean);
    }
    else if (!xmlStrcmp(value_type, (const xmlChar *)"intrvl")) {
      value_addr  =  &(ds->interval);
    }
    else if (!xmlStrcmp(value_type, (const xmlChar *)"conf")) {
      value_addr  =  &(ds->confidence);
    }
    else if (!xmlStrcmp(value_type, (const xmlChar *)"value")) {
      value_addr  =  ds->values;
    }
    else if (!xmlStrcmp(value_type, (const xmlChar *)"if_value")) {
      value_addr  =  ds->values;
    }
  }
  if (!xmlStrcmp(value_type, (const xmlChar *)"count")) {
    value_addr    =  &(rd->count);
  }
  xmlFree(value_type);
  return(value_addr);
}


static char*
get_label(report_data_t  *rd,
	  xmlNodePtr      field)
{
  xmlChar *value_type;
  char    *label;

  value_type  =  xmlGetProp(field,(const xmlChar *)"type");
  if (!xmlStrcmp(value_type, (const xmlChar *)"tset")) {
    label  =  (char *)rd->tset_id;
  }
  else if (!xmlStrcmp(value_type, (const xmlChar *)"tid")) {
    label  =  (char *)rd->tid;
  }
  else {
    label  =  (char *)xmlGetProp(field,(const xmlChar *)"label");
  }
  xmlFree(value_type);
  return(label);
}


static double
get_unit_value(report_data_set_t  *ds,
               xmlNodePtr          field)
{
  xmlChar *units;
  double   unit;
  uint32_t attr;

  unit   =  0.0;
  units  =  xmlGetProp(field,(const xmlChar *)"units");
  if ((units != NULL) && (ds != NULL)) {
    attr  =  (ds->attr & CNTR_UNITS);
    if (attr == CNTR_UNIT_BYTES) {
      /* unit conversions for service demand with byte counters */
      /* Equation is microseconds cpu time in 1 sec  (num_cpu * util * 10^6)
         util is in the range of 0.0-1.0.
         divided by number of Kbytes in 1 usec
         divided by 10^6 to get microseconds per KB.
         (((num_cpu * util * 10^6) / (bytes / (time * 1024)) / 10^6) 
         simplified equation is:
         (num_cpu * util * unit * time) / bytes */
      if (!xmlStrcmp(units, (const xmlChar *)"us/KB")) {
        unit = 1024.0;
      }
      /* unit conversions for throughput */
      /* equation is byte_count * unit / time */
      if (!xmlStrcmp(units, (const xmlChar *)"mbps"))  {
        unit = 8.0/1000000.0;
      }
      if (!xmlStrcmp(units, (const xmlChar *)"gbps"))  {
        unit = 8.0/1000000000.0;
      }
      if (!xmlStrcmp(units, (const xmlChar *)"MBps"))  {
        unit = 1.0/1000000.0;
      }
      if (!xmlStrcmp(units, (const xmlChar *)"GBps"))  {
        unit = 1.0/1000000000.0;
      }
    }
    if ((attr == CNTR_UNIT_TRANS) ||
	(attr == CNTR_UNIT_CALLS)) {
      /* unit conversions for service demand with trans or call counters */
      if (!xmlStrcmp(units, (const xmlChar *)"us/t"))  {
        unit = 1.0;
      }
      /* unit conversions for transactions */
      if (!xmlStrcmp(units, (const xmlChar *)"tps"))  {
        unit = 1.0;
      }
      if (!xmlStrcmp(units, (const xmlChar *)"iops"))  {
        unit = 1.0;
      }
    }
    /* unit conversions for CPU utilization */
    if (!xmlStrcmp(units, (const xmlChar *)"%/100")) {
      unit = 1.0;
    }
    if (!xmlStrcmp(units, (const xmlChar *)"%")) {
      unit = 100.0;
    }
  }
  if (unit == 0.0) {
    /* report error for field and data set incompatibility */
  }
  xmlFree(units);
  return(unit);
}


static int
get_field_width(xmlNodePtr  field)
{
  int    value;
  char  *attr_str;

  attr_str       = (char *)xmlGetProp(field,(const xmlChar *)"width");
  if (attr_str) {
    value  = atoi(attr_str);
  }
  xmlFree(attr_str);
  return(value);
}


static int
get_field_precision(xmlNodePtr  field)
{
  int    value;
  char  *attr_str;

  value = 0;
  attr_str       = (char *)xmlGetProp(field,(const xmlChar *)"precision");
  if (attr_str) {
    value  = atoi(attr_str);
  }
  xmlFree(attr_str);
  return(value);
}


static int
get_field_per_run_flag(xmlNodePtr  field)
{
  int      value;
  char    *attr_str;

  value = FALSE;
  attr_str       = (char *)xmlGetProp(field,(const xmlChar *)"when");
  if (!strcmp(attr_str,   "run")) {
    value = TRUE;
  }
  if (!strcmp(attr_str,   "run&sum")) {
    value = TRUE;
  }
  xmlFree(attr_str);
  return(value);
}


static int
get_field_summary_flag(xmlNodePtr  field)
{
  int      value;
  char    *attr_str;

  value = FALSE;
  attr_str       = (char *)xmlGetProp(field,(const xmlChar *)"when");
  if (!strcmp(attr_str,   "summary")) {
    value = TRUE;
  }
  if (!strcmp(attr_str,   "run&sum")) {
    value = TRUE;
  }
  xmlFree(attr_str);
  return(value);
}


static int
get_field_type_if_value_flag(xmlNodePtr      field)
{
  xmlChar *value_type;
  int      if_value;

  if_value = 0;
  value_type  =  xmlGetProp(field,(const xmlChar *)"type");
  if (!xmlStrcmp(value_type, (const xmlChar *)"if_value")) {
    if_value  =  1;
  }
  xmlFree(value_type);
  return(if_value);
}


static uint32_t
get_data_set_attributes(xmlNodePtr  set)
{
  xmlNodePtr  attr;
  char       *agg;
  char       *unit;
  char       *type;
  char       *class;
  uint32_t    all;
  uint32_t    each;

  all  = 0;
  attr = set->xmlChildrenNode;
  while (attr != NULL) {
    if (!xmlStrcmp(attr->name, (const xmlChar *)"cntr_attributes")) {
      agg   = (char *)xmlGetProp(attr, (const xmlChar *)"cntr_agg_flag");
      unit  = (char *)xmlGetProp(attr, (const xmlChar *)"cntr_unit");
      type  = (char *)xmlGetProp(attr, (const xmlChar *)"cntr_type");
      class = (char *)xmlGetProp(attr, (const xmlChar *)"cntr_class");
      each  = generate_attributes(agg, unit, type, class);
      xmlFree(agg);
      xmlFree(unit);
      xmlFree(type);
      xmlFree(class);
      all   = all | each;
    }
    attr  = attr->next;
  }
  return(all);
}


static int
initialize_fields(report_data_t   *rd,
                  xmlNodePtr       part,
                  int              f)
{
  xmlNodePtr         field;
  report_field_t    *rf;
  report_data_set_t *ds;
  char              *sep_str;
  int                per_test;

  /* initialize the field data structures */
  field = part->xmlChildrenNode;
  rf    = rd->fields;
  if ( !xmlStrcmp(part->name,(xmlChar *)"report_each_test") ) {
    per_test = 1;
  }
  else {
    per_test = 0;
  }
  while (field != NULL) {
    if ( !xmlStrcmp(field->name,(xmlChar *)"report_field") ) {
      sep_str  = (char *)xmlGetProp(part, (const xmlChar *)"field_separator");
      if (sep_str == NULL) {
        sep_str  = rd->field_sep;
      }
      rf[f].field      =  field;
      rf[f].set        =  get_data_set_for_field(rd, field);
      rf[f].value      =  get_data_value(rd, rf[f].set, field, per_test);
      rf[f].label      =  get_label(rd, field);
      rf[f].units      =  get_unit_value(rf[f].set, field);
      rf[f].next       = &(rf[f+1]); 
      rf[f].separator  =  sep_str;
      rf[f].width      =  get_field_width(field);
      rf[f].precision  =  get_field_precision(field);
      rf[f].per_run    =  get_field_per_run_flag(field);
      rf[f].summary    =  get_field_summary_flag(field);
      rf[f].if_value   =  get_field_type_if_value_flag(field);
      f++;
    }
    field  = field->next;
  }
  rf[f-1].next = NULL;

  return(f);
}


static void
initialize_report_data(report_data_t *rd)
{
  xmlNodePtr         part;
  report_part_t     *rp;
  report_field_t    *rf;
  report_data_set_t *ds;
  double            *vp;
  char              *sep_str;
  
  int                p;
  int                f;
  int                d;
  int                max;
  
  max  = rd->max_ds_values;

  /* initialize the report data structure */
  sep_str  = (char *)xmlGetProp(rd->report_desc,
                                (const xmlChar *)"field_separator");
  if (sep_str) {
    rd->field_sep  = sep_str;
  }
  else {
    rd->field_sep  = " ";
  }
  sep_str  = (char *)xmlGetProp(rd->report_desc,
                                (const xmlChar *)"part_separator");
  if (sep_str) {
    rd->part_sep   = sep_str;
  }
  else {
    rd->part_sep   = "\n\n";
  }
    
  rp   = rd->summary;
  rf   = rd->fields;
  vp   = rd->values;
  ds   = rd->data_sets;

  /* initialize data_set structures */
  d    = 0;
  while (part != NULL) {
    if ( !xmlStrcmp(part->name,(xmlChar *)"data_set") ) {
      ds[d].data_set   = part;
      ds[d].name       = xmlGetProp(part, (const xmlChar *)"name");
      ds[d].min        = DBL_MAX;
      ds[d].max        = DBL_MIN;
      ds[d].mean       = 0.0;
      ds[d].interval   = 0.0;
      ds[d].confidence = -100.0;
      ds[d].values     = &(vp[max * d]);
      ds[d].attr       = get_data_set_attributes(part);
      d++;
    }
    part  = part->next;
  }
  rd->cpu_set          = initialize_cpu_set(rd);
  rd->run_time_set     = initialize_run_time_set(rd); 
  get_sd_data_sets(rd);

  /* initialize per_run and summary structures */
  part = rd->report_desc->xmlChildrenNode;
  p = 0;
  f = 0;
  while (part != NULL) {
    if ( !xmlStrcmp(part->name,(xmlChar *)"report_summary") ) {
      sep_str  = (char *)xmlGetProp(part, (const xmlChar *)"part_separator");
      if (sep_str == NULL) {
        sep_str  = rd->part_sep;
      }
      rp[p].part               = part;
      rp[p].fields             = &(rf[f]);
      rp[p].next               = &(rp[p+1]);
      rp[p].separator          = sep_str;
      rp[p].hdr_lines          = 0;
      f = initialize_fields(rd, part, f);
      p++;
    }
    part  = part->next;
  }
  rp[p-1].next = NULL;

  /* initialize per_test structures */
  if (p < rd->num_parts) {
    rd->per_test = &(rp[p]);
  }
  part = rd->report_desc->xmlChildrenNode;
  while (part != NULL) {
    if ( !xmlStrcmp(part->name,(xmlChar *)"report_each_test") ) {
      sep_str  = (char *)xmlGetProp(part, (const xmlChar *)"part_separator");
      if (sep_str == NULL) {
        sep_str  = rd->part_sep;
      }
      rp[p].part               = part;
      rp[p].fields             = &(rf[f]);
      rp[p].next               = &(rp[p+1]);
      rp[p].separator          = sep_str;
      rp[p].hdr_lines          = 0;
      f = initialize_fields(rd, part, f);
      p++;
    }
    part  = part->next;
  }
  rp[p-1].next = NULL;
}


report_data_t *
parse_report_description(tset_t     *test_set, 
                         char       *report_flags,
                         xmlNodePtr  report_desc,
                         FILE       *opf)
{
  report_data_t  *rd;
  xmlNodePtr      part;
  xmlNodePtr      field;
  xmlChar        *print_str;
  int             p;
  int             f;
  int             d;
  int             max;
  
  max  = test_set->confidence.max_count;

  /* count number structures needed based on contents of the report_desc */
  d = 0;
  p = 0;
  f = 0;
  part = report_desc->xmlChildrenNode;
  while (part != NULL) {
    field = NULL;
    if ( !xmlStrcmp(part->name,(xmlChar *)"data_set") ) {
      d++;
    }
    if ( !xmlStrcmp(part->name,(xmlChar *)"report_summary") ) {
      p++;
      field = part->xmlChildrenNode;
    }
    if ( !xmlStrcmp(part->name,(xmlChar *)"report_each_test") ) {
      p++;
      field = part->xmlChildrenNode;
    }
    while (field != NULL) {
      if ( !xmlStrcmp(field->name,(xmlChar *)"report_field") ) {
        f++;
      }
      field = field->next;
    }
    part = part->next;
  }

  /* allocate memory for the report data structure */
  rd = malloc(sizeof(report_data_t));
  if (rd) {
    /* zero and initialize report_data_t structure */
    memset(rd, 0, sizeof(report_data_t));
    rd->out_file    = opf;
    rd->report_desc = report_desc;
    if (NULL != report_flags) {
      if (!strcmp(report_flags,"PRINT_RUN")) {
        rd->print_run       = 1;
      }
      if (!strcmp(report_flags,"PRINT_TESTS")) {
        rd->print_test      = 1;
      }
      if (!strcmp(report_flags,"PRINT_ALL")) {
        rd->print_run       = 1;
        rd->print_test      = 1;
        rd->print_raw_cntrs = 1;
        rd->print_hist      = 1;
      }
    }
    rd->num_parts      = p;
    rd->num_fields     = f;
    rd->num_data_sets  = d;
    rd->max_ds_values  = max;

    /* allocate memory for additional structures */
    rd->summary   = calloc(p, sizeof(report_part_t));
    rd->fields    = calloc(f, sizeof(report_field_t));
    rd->data_sets = calloc(d + 2, sizeof(report_data_set_t));
    rd->values    = calloc((d + 2) * max, sizeof(double));
    if ((rd->summary   != NULL) &&
        (rd->fields    != NULL) &&
        (rd->data_sets != NULL) &&
        (rd->values    != NULL)) {
      initialize_report_data(rd);
    }
    else {
      /* could not allocate memory can't generate report */
      fprintf(opf,
              "%s: calloc failed can't generate report\n",
              __func__);
      fflush(opf);
      exit(-1);
    }
  }
  else {
    /* could not allocate memory can't generate report */
    fprintf(opf,
            "%s: malloc failed can't generate report\n",
            __func__);
    fflush(opf);
    exit(-1);
  }
}


/* CODE which processes statistics messages and updates results */

static void
accumulate_rate(report_data_t   *rd,
                double           rate, 
                uint32_t         cntr_attr)
{
  report_data_set_t  *ds;
  int                 d;
  int                 index;

  index  = rd->index;
  if (cntr_attr & CNTR_AGG_FLAG) {
    ds  = rd->data_sets;
    for (d = 0; d < (rd->num_data_sets - rd->num_sd_sets); d++) {
      if ((ds[d].attr & cntr_attr) == cntr_attr) {
        ds[d].values[index] += rate;
        ds[d].per_test      += rate;
        ds[d].num_per_run++;
        ds[d].num_per_test++;
      }
    }
  }
}


static void
clear_per_test_results(report_data_t  *rd)
{
  report_data_set_t  *ds;
  int                 d;

  ds  = rd->data_sets;
  for (d = 0; d < (rd->num_data_sets - rd->num_sd_sets); d++) {
    ds[d].per_test      = 0.0;
    ds[d].num_per_test  = 0;
  }
}


static double
get_elapsed_time(report_data_t  *rd,
                 xmlNodePtr      test_stats)
{
  char   *value_str;
  double  sec;
  double  usec;

  sec  = 0.0;
  usec = 0.0;
  value_str  = (char *)xmlGetProp(test_stats, (const xmlChar *)"elapsed_sec");
  if (value_str) {
    sec = strtod(value_str,NULL);
    if (sec = 0.0) {
      uint64_t x;
      sscanf(value_str,"%"PRIx64,&x);
      sec = (double)x;
    }
  }
  xmlFree(value_str);
  value_str  = (char *)xmlGetProp(test_stats, (const xmlChar *)"elapsed_usec");
  if (value_str) {
    usec = strtod(value_str,NULL);
    if (usec = 0.0) {
      uint64_t x;
      sscanf(value_str,"%"PRIx64,&x);
      usec = (double)x;
    }
  }
  xmlFree(value_str);
  sec = sec + usec/1000000.0;
  rd->run_time_set->values[rd->index] += sec;
  rd->run_time_set->num_per_run++;
  return(sec);
}


static void
process_test_counter(report_data_t *rd,
                     xmlChar       *tid,
                     xmlNodePtr     counter,
                     double         elapsed_time)
{
  xmlNodePtr  cntr_attr;
  char       *name;
  char       *value;
  char       *agg;
  char       *unit;
  char       *type;
  char       *class;
  double      rate;
  uint32_t    rate_attr;

  
  /* read counter information */
  cntr_attr = counter->xmlChildrenNode;

  name      = (char *)xmlGetProp(counter,   (const xmlChar *)"cntr_name");
  value     = (char *)xmlGetProp(counter,   (const xmlChar *)"cntr_value");

  agg       = (char *)xmlGetProp(cntr_attr, (const xmlChar *)"cntr_agg_flag");
  unit      = (char *)xmlGetProp(cntr_attr, (const xmlChar *)"cntr_unit");
  type      = (char *)xmlGetProp(cntr_attr, (const xmlChar *)"cntr_type");
  class     = (char *)xmlGetProp(cntr_attr, (const xmlChar *)"cntr_class");

  if (value) {
    rate = strtod(value,NULL);
    if (rate = 0.0) {
      uint64_t x;
      sscanf(value,"%"PRIx64,&x);
      rate = (double)x;
    }
    rate = rate / elapsed_time;
  }
  if (rd->print_raw_cntrs) {
    fprintf(rd->out_file, "test[%s]  %s = %s %s\t(%s,%s,%s) %f\n",
            tid, name, value, unit, agg, type, class, rate);
  }
        
  /* accumulate rate into proper buckets */
  rate_attr = generate_attributes(agg, unit, type, class);
  accumulate_rate(rd, rate, rate_attr);

  xmlFree(name);
  xmlFree(value);
  xmlFree(agg);
  xmlFree(unit);
  xmlFree(type);
  xmlFree(class);
}


static void
process_test_info(report_data_t *rd,
                  xmlChar       *tid,
                  xmlNodePtr     info)
{
  char   *name;
  char   *value;
  
  /* read test information */
  name  = (char *)xmlGetProp(info, (const xmlChar *)"info_name");
  value = (char *)xmlGetProp(info, (const xmlChar *)"info_value");
  if (rd->print_raw_cntrs) {
    fprintf(rd->out_file, "test[%s]  %s = %s\n", tid, name, value);
  }
  xmlFree(name);
  xmlFree(value);
}
        

static void
process_test_stats(tset_t *test_set, test_t *test, xmlNodePtr test_stats)
{
  report_data_t  *rd;
  xmlNodePtr      counter;
  FILE           *opf;
  double          elapsed_seconds;
  double          time_seconds;
  
  rd    = test_set->report_data;

  /* process test statistics */
  if (test_set->debug) {
    fprintf(test_set->where,"\t%s for test %s\n", __func__, test->id);
    fflush(test_set->where);
  }

  elapsed_seconds = get_elapsed_time(rd, test_stats);
  /* add code for delta time when needed */

  counter = test_stats->xmlChildrenNode;
  while (counter != NULL) {
    if (!xmlStrcmp (counter->name,(const xmlChar *)"counter")) {
      process_test_counter(rd, test->id, counter, elapsed_seconds);
    }
    else if (!xmlStrcmp (counter->name,(const xmlChar *)"test_info")) {
      /* display testinfo */
      process_test_info(rd, test->id, counter);
    }
    else if (!xmlStrcmp (counter->name,(const xmlChar *)"hist_stats")) {
      if (rd->print_hist) {
        /* read & display histograms */
        HIST_REPORT(rd->out_file, counter);
      }
    }
    else if (counter->type == XML_CDATA_SECTION_NODE) {
      /* display PCDATA how do I do this?? */
      fprintf(rd->out_file, "\n%s\n\n", counter->content);
    }
    counter = counter->next;
  }
  if (rd->print_test) {
    print_results_for_test(rd, test->id);
  }
  clear_per_test_results(rd);
}


static void
process_sys_stats(tset_t *test_set, test_t * test, xmlNodePtr stats)
{
  report_data_t *rd;
  FILE          *opf;
  xmlNodePtr     cpu;
  char          *method;
  char          *cpus_str;
  char          *cali_str;
  char          *idle_str;
  char          *user_str;
  char          *sys_str;
  char          *int_str;
  char          *other_str;
  double        *cpu_util;
  double        *run_time;
  double         idle;
  double         busy;
  double         calibration;
  double         num_cpus;
  double         elapsed_seconds;
  int            count;

  rd              = test_set->report_data;
  count           = test_set->confidence.count;
  opf             = rd->out_file;
  elapsed_seconds = get_elapsed_time(rd, stats);

  if (rd->print_raw_cntrs) {
    xmlNodePtr  cpu;
    cpu  = stats->xmlChildrenNode;
    fprintf(opf,"\nRAW PER CPU STATS\n");
    fprintf(opf,"%s ",  "  cpu_id");
    fprintf(opf,"%s ",  " %idle  ");
    fprintf(opf,"%s ",  "calibration");
    fprintf(opf,"%s ",  "idle_count");
    fprintf(opf,"%s ",  "user_count");
    fprintf(opf,"%s ",  "sys_count");
    fprintf(opf,"%s ",  "int_count");
    fprintf(opf,"%s\n", "other_count");
    while (cpu != NULL) {
      if (!xmlStrcmp(cpu->name, (const xmlChar *)"per_cpu_stats")) {
        cpus_str  = (char *)xmlGetProp(cpu, (const xmlChar *)"cpu_id");
        cali_str  = (char *)xmlGetProp(cpu,(const xmlChar *)"calibration");
        idle_str  = (char *)xmlGetProp(cpu, (const xmlChar *)"idle_count");
        user_str  = (char *)xmlGetProp(cpu, (const xmlChar *)"user_count");
        sys_str   = (char *)xmlGetProp(cpu, (const xmlChar *)"sys_count");
        int_str   = (char *)xmlGetProp(cpu, (const xmlChar *)"int_count");
        other_str = (char *)xmlGetProp(cpu, (const xmlChar *)"other_count");

        if (cali_str) {
          calibration  = strtod(cali_str,NULL);
          if (calibration == 0.0) {
            uint64_t x;
            sscanf(cali_str,"%llx", &x);
            calibration  = (double)x;
          }
        }
        if (idle_str) {
          idle  = strtod(idle_str,NULL);
          if (idle == 0.0) {
            uint64_t x;
            sscanf(idle_str,"%llx", &x);
            idle  = (double)x;
          }
        }
        idle = idle / calibration;

        fprintf(opf, "  cpu%3s ",cpus_str);
        fprintf(opf, "%f ",  idle);
        fprintf(opf, "%s ",  cali_str);
        fprintf(opf, "%s ",  idle_str);
        fprintf(opf, "%s ",  user_str);
        fprintf(opf, "%s ",  sys_str);
        fprintf(opf, "%s ",  int_str);
        fprintf(opf, "%s\n", other_str);

        xmlFree(cpus_str);
        xmlFree(cali_str);
        xmlFree(idle_str);
        xmlFree(user_str);
        xmlFree(sys_str);
        xmlFree(int_str);
        xmlFree(other_str);
      }
    }
    fprintf(opf,"\n");
    fflush(opf);
  }

  method     = (char *)xmlGetProp(stats, (const xmlChar *)"method");
  cpus_str   = (char *)xmlGetProp(stats, (const xmlChar *)"number_cpus");
  cali_str   = (char *)xmlGetProp(stats, (const xmlChar *)"calibration");
  idle_str   = (char *)xmlGetProp(stats, (const xmlChar *)"idle_count");
  user_str   = (char *)xmlGetProp(stats, (const xmlChar *)"user_count");
  sys_str    = (char *)xmlGetProp(stats, (const xmlChar *)"sys_count");
  int_str    = (char *)xmlGetProp(stats, (const xmlChar *)"int_count");
  other_str  = (char *)xmlGetProp(stats, (const xmlChar *)"other_count");

  if (cpus_str) {
    num_cpus  = strtod(cpus_str,NULL);
    if (num_cpus == 0.0) {
      uint64_t x;
      sscanf(cpus_str,"%llx", &x);
      num_cpus  = (double)x;
    }
  }
  if (cali_str) {
    calibration  = strtod(cali_str,NULL);
    if (calibration == 0.0) {
      uint64_t x;
      sscanf(cali_str,"%llx", &x);
      calibration  = (double)x;
    }
  }
  if (idle_str) {
    idle  = strtod(idle_str,NULL);
    if (idle == 0.0) {
      uint64_t x;
      sscanf(idle_str,"%llx", &x);
      idle  = (double)x;
    }
  }
  busy = (calibration - idle) / calibration;
  idle = idle / calibration;

  if (rd->print_raw_cntrs) {
    fprintf(opf, "#cpu=%3s ",cpus_str);
    fprintf(opf, "%f ",  idle);
    fprintf(opf, "%s ",  cali_str);
    fprintf(opf, "%s ",  idle_str);
    fprintf(opf, "%s ",  user_str);
    fprintf(opf, "%s ",  sys_str);
    fprintf(opf, "%s ",  int_str);
    fprintf(opf, "%s\n", other_str);
  }

  if ((rd->index > 0) && (rd->num_of_cpus != num_cpus)) {
    /* number of cpus changed between runs report problem */
    fprintf(opf,"\n\n");
    fprintf(opf,"*************************************************\n");
    fprintf(opf,"*************************************************\n");
    fprintf(opf,"****  NUMBER OF CPUS CHANGED between runs!   ****\n");
    fprintf(opf,"****            RESULTS ARE SUSPECT!         ****\n");
    fprintf(opf,"****  prev_cpus = %3d   current_cpus = %3d   ****\n",
                 rd->num_of_cpus, num_cpus);
    fprintf(opf,"*************************************************\n");
    fprintf(opf,"*************************************************\n");
    fprintf(opf,"\n\n");
    fflush(opf);
  }
  rd->num_of_cpus                 = num_cpus;
  rd->cpu_set->values[rd->index]  = busy;
  rd->cpu_set->num_per_run++;

  if (rd->print_test) {
    /* Display sys_stats test results */
    fprintf(opf,"%3d  ",   count);                    /*  0,5 */
    fprintf(opf,"%-6s ",   test->id);                 /*  5,7 */
    fprintf(opf,"%-6.2f ", elapsed_seconds);          /* 12,7 */
    fprintf(opf,"%24s",    "");                       /* 19,24*/
    fprintf(opf,"%7.1e ",  calibration);              /* 43,8 */
    fprintf(opf,"%6.3f ",  idle*100.0);               /* 51,7 */
    fprintf(opf,"%6.3f ",  busy*100.0);               /* 58,7 */
    fprintf(opf,"\n");                                /* 79,1 */
    fflush(opf);
  }
  
  xmlFree(method);
  xmlFree(cpus_str);
  xmlFree(cali_str);
  xmlFree(idle_str);
  xmlFree(user_str);
  xmlFree(sys_str);
  xmlFree(int_str);
  xmlFree(other_str);
}


static void
calculate_service_demand(tset_t *test_set)
{
  report_data_t          *rd;
  report_data_set_t      *ds;
  int                     d;
  double                  time;
  double                  util;
  double                  sd;


  rd  = test_set->report_data;
  NETPERF_DEBUG_ENTRY(test_set->debug,test_set->where);

  /*   Simplified service demand equation is:
       (num_cpu * util * unit * time) / value
       Unit multiplication is done at print time
       and is dependent upon the type of value used
       and the units being printed.                    */

  util  = rd->cpu_set->values[rd->index];
  time  = rd->run_time_set->values[rd->index];
  time  = time / (double)rd->run_time_set->num_per_run;
  sd    = rd->num_of_cpus * util * time;

  /* store back the averaged run time from each of the individual tests */
  rd->run_time_set->values[rd->index]  = time;

  ds  = rd->data_sets;
  for (d = 0; d < rd->num_data_sets; d++) {
    if (ds[d].sd_data_set != NULL)  {
      ds[d].values[rd->index] = sd / ds[d].sd_data_set->values[rd->index];
    }
  }
  NETPERF_DEBUG_EXIT(test_set->debug,test_set->where);
}


static void
process_stats_for_run(tset_t *test_set)
{
  tset_elt_t    *set_elt;
  test_t        *test;
  xmlNodePtr     stats;


  set_elt  = test_set->tests;
  while (set_elt != NULL) {
    test   = set_elt->test;
    stats  = test->received_stats->xmlChildrenNode;
    if (stats != NULL) {
      /* process all the statistics records for this test */
      if (test_set->debug) {
        fprintf(test_set->where,"\tfound some statistics\n");
        fflush(test_set->where);
      }
      if(!xmlStrcmp(stats->name,(const xmlChar *)"sys_stats")) {
        /* process system statistics */
        process_sys_stats(test_set, test, stats);
      }
      if(!xmlStrcmp(stats->name,(const xmlChar *)"test_stats")) {
        /* process test statistics */
        process_test_stats(test_set, test, stats);
      }
    }
    set_elt = set_elt->next;
  }
  calculate_service_demand(test_set);
  NETPERF_DEBUG_EXIT(test_set->debug,test_set->where);
}


static void
update_results_and_confidence(tset_t *test_set)
{
  report_data_t          *rd;
  report_data_set_t      *ds;
  int                     d;
  int                     i;


  NETPERF_DEBUG_ENTRY(test_set->debug,test_set->where);
  rd  = test_set->report_data;
  ds  = rd->data_sets;
  i   = rd->index;
  for (d = 0; d < rd->num_data_sets; d++) {
    if (ds[d].min > ds[d].values[i]) {
      ds[d].min = ds[d].values[i];
    }
    if (ds[d].max < ds[d].values[i]) {
      ds[d].max = ds[d].values[i];
    }
    ds[d].confidence  =  get_confidence(ds[d].values,
                                        &(test_set->confidence),
                                        &(ds[d].mean),
                                        &(ds[d].interval));
  }
  NETPERF_DEBUG_EXIT(test_set->debug,test_set->where);
}


static void
initialize_headers(report_data_t *rd, report_part_t *rp)
{
  report_field_t *rf;
  xmlChar        *units;
  int             i;

  if (rp->hdr_lines == 0) {
    rf = rp->fields;
    while (rf) {
      rf->header[0] = (char *)xmlGetProp(rf->field,(const xmlChar *)"header1");
      rf->header[1] = (char *)xmlGetProp(rf->field,(const xmlChar *)"header2");
      rf->header[2] = (char *)xmlGetProp(rf->field,(const xmlChar *)"header3");
      if (rf->header[2] == NULL) {
        units = xmlGetProp(rf->field,(const xmlChar *)"units");
        if (units != NULL) {
          if (!xmlStrcmp(units, (const xmlChar *)"none")) {
            xmlFree(units);
          }
          else {
            rf->header[2] = (char *)units;
          }
        }
      }
      i = 3;
      if (rf->header[2] == NULL) {
        i = 2;
        if (rf->header[1] == NULL) {
          i = 1;
        }
      }
      if (rp->hdr_lines < i) {
        rp->hdr_lines = i;
      }
      rf = rf->next;
    }
  }
}


static void
print_per_test_headers(report_data_t *rd, report_part_t *rp)
{
  FILE           *opf;
  report_field_t *field;
  int             i;

  opf  = rd->out_file;
  for (i = 0; i < rp->hdr_lines; i++) {
    field = rp->fields;
    while (field) {
      if (field->if_value) {
        if (field->set) {
          if (field->set->num_per_test == 0) {
            field = field->next;
            continue;
          }
        }
      }
      fprintf(opf,
              "%2$*3$s%4$s",
              field->header[i],
              field->width,
              field->separator);
      field = field->next;
    }
    fflush(opf);
  }
}


static void
print_per_run_headers(report_data_t *rd, report_part_t *rp)
{
  FILE           *opf;
  report_field_t *field;
  int             i;

  opf  = rd->out_file;
  for (i = 0; i < rp->hdr_lines; i++) {
    field = rp->fields;
    while (field) {
      if (field->per_run) {
        if (field->if_value) {
          if (field->set) {
            if (field->set->num_per_run == 0) {
              field = field->next;
              continue;
            }
          }
        }
        fprintf(opf,
                "%2$*3$s%4$s",
                field->header[i],
                field->width,
                field->separator);
      }
      field = field->next;
    }
    fflush(opf);
  }
}


static void
print_summary_headers(report_data_t *rd, report_part_t *rp)
{
  FILE           *opf;
  report_field_t *field;
  int             i;

  opf  = rd->out_file;
  for (i = 0; i < rp->hdr_lines; i++) {
    field = rp->fields;
    while (field) {
      if (field->summary) {
        if (field->if_value) {
          if (field->set) {
            if (field->set->min == DBL_MAX) {
              field = field->next;
              continue;
            }
          }
        }
        fprintf(opf,
                "%2$*3$s%4$s",
                field->header[i],
                field->width,
                field->separator);
      }
      field = field->next;
    }
    fflush(opf);
  }
}


static void
print_test_values(report_data_t *rd, report_part_t *rp, xmlChar *tid)
{
  FILE           *opf;
  report_field_t *field;

  rd->tid  =  tid;
  opf      =  rd->out_file;
  field    =  rp->fields;
  while (field) {
    if (field->if_value) {
      if (field->set) {
        if (field->set->num_per_test == 0) {
          field = field->next;
          continue;
        }
      }
    }
    if (field->label) {
      fprintf(opf,
              "%2$*3$.*4$s%5$s",
              field->label,
              field->width,
              field->precision,
              field->separator);
    }
    if (field->value) {
      fprintf(opf,
              "%2$*3$.*4$f%5$s",
              field->value,
              field->width,
              field->precision,
              field->separator);
    }
    field = field->next;
  }
  fprintf(opf,
          "%s",
          rp->separator);
  fflush(opf);
}


static void
print_per_run_values(report_data_t *rd, report_part_t *rp)
{
  FILE           *opf;
  report_field_t *field;
  int             i;
  double          value;

  opf   = rd->out_file;
  field = rp->fields;
  i     = rd->index;
  
  while (field) {
    if (field->per_run) {
      if (field->if_value) {
        if (field->set) {
          if (field->set->num_per_run == 0) {
            field = field->next;
            continue;
          }
        }
      }
      if (field->label) {
        fprintf(opf,
                "%2$*3$.*4$s%5$s",
                field->label,
                field->width,
                field->precision,
                field->separator);
      }
      if (field->value) {
        if (field->summary) {
          value = field->value[0];
        }
        else {
          value = field->value[i];
        }
        fprintf(opf,
                "%2$*3$.*4$f%5$s",
                value,
                field->width,
                field->precision,
                field->separator);
      }
    }
    field = field->next;
  }
  fprintf(opf,
          "%s",
          rp->separator);
  fflush(opf);
}


static void
print_summary_values(report_data_t *rd, report_part_t *rp)
{
  FILE           *opf;
  report_field_t *field;

  opf   = rd->out_file;
  field = rp->fields;
  while (field) {
    if (field->summary) {
      if (field->if_value) {
        if (field->set) {
          if (field->set->min == DBL_MAX) {
            field = field->next;
            continue;
          }
        }
      }
      if (field->label) {
        fprintf(opf,
                "%2$*3$.*4$s%5$s",
                field->label,
                field->width,
                field->precision,
                field->separator);
      }
      if (field->value) {
        fprintf(opf,
                "%2$*3$.*4$f%5$s",
                field->value,
                field->width,
                field->precision,
                field->separator);
      }
    }
    field = field->next;
  }
  fprintf(opf,
          "%s",
          rp->separator);
  fflush(opf);
}


static void
print_results_for_test(report_data_t *rd, xmlChar *tid)
{
  report_part_t  *rp;
  report_field_t *field;

  /* scan for valid parts to print */
  rp  = rd->per_test;
  while (rp) {
    field = rp->fields;
    while (field) {
      if (field->set) {
        if (field->set->num_per_test) {
          initialize_headers(rd, rp);
          print_per_test_headers(rd, rp);
          print_test_values(rd, rp, tid);
          break;
        }
      }
      field = field->next;
    }
    rp = rp->next;
  }
}


static void
print_results_for_run(report_data_t  *rd)
{
  report_part_t  *rp;
  report_field_t *field;
  
  /* scan for valid parts to print */
  rp  = rd->summary;
  while (rp) {
    field = rp->fields;
    while (field) {
      if ((field->set) && (field->per_run)) {
        if (field->set->num_per_run) {
          initialize_headers(rd, rp);
          print_per_run_headers(rd, rp);
          print_per_run_values(rd, rp);
          break;
        }
      }
      field = field->next;
    }
    rp = rp->next;
  }
}


static void
print_results_summary(report_data_t  *rd)
{
  report_part_t  *rp;
  report_field_t *field;
  
  /* scan for valid parts to print */
  rp  = rd->summary;
  while (rp) {
    field = rp->fields;
    while (field) {
      if ((field->set) && (field->summary)) {
        initialize_headers(rd, rp);
        print_summary_headers(rd, rp);
        print_summary_values(rd, rp);
        break;
      }
      field = field->next;
    }
    rp = rp->next;
  }
}


static void 
print_did_not_meet_confidence(tset_t *test_set)
{
  report_data_t     *rd;
  FILE              *opf;
  report_data_set_t *ds;
  int                d;
  
  rd  = test_set->report_data;
  opf = rd->out_file;
  /* print the confidence failed message */
  fprintf(opf, "\n");
  fprintf(opf, "!!! WARNING\n");
  fprintf(opf, "!!! Desired confidence was not achieved within ");
  fprintf(opf, "the specified iterations. (%d)\n",
          test_set->confidence.max_count);
  fprintf(opf, "!!! This implies that there was variability in ");
  fprintf(opf, "the test environment that\n");
  fprintf(opf, "!!! must be investigated before going further.\n");
  ds  = rd->data_sets;
  for (d = 0; d < rd->num_data_sets; d++) {
    if (ds[d].min != DBL_MAX) {
      fprintf(opf,
              "!!! Confidence interval  %s: %6.2f%%\n",
              (char *)ds[d].name,
              100.0 * ds[d].confidence);
    }
  }
  fflush(opf);
}


void
generate_report(tset_t *test_set,
                char *report_flags,
                FILE *opf,
                xmlNodePtr report_desc)
{
  report_data_t  *rd;
  int             count;
  int             max_count;
  int             min_count;

  rd = test_set->report_data;
  if (rd == NULL) {
    rd = parse_report_description(test_set, report_flags, report_desc, opf);
    test_set->report_data = rd;
  }

  /* process statistics for this run */
  rd->index  =  test_set->confidence.count - 1;
  rd->count  =  test_set->confidence.count;
  process_stats_for_run(test_set);

  /* update summary result values and calculate confidence */
  update_results_and_confidence(test_set);

  if (rd->print_run) {
    print_results_for_run(rd);
  }
 
  count        = test_set->confidence.count;
  max_count    = test_set->confidence.max_count;
  min_count    = test_set->confidence.min_count;

  /* always print summary results at end of last call through loop */
  if ((count >= max_count) ||
      ((test_set->confidence.value >= 0) && (count >= min_count))) {
    print_results_summary(rd);
    if ((test_set->confidence.value < 0) && (min_count > 1)) {
      print_did_not_meet_confidence(test_set);
    }
  }
}
