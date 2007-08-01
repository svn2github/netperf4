
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


#define MAX_HDR_LINES  3


/* Bit map definitions for counter accumulation */


#define CNTR_AGG_FLAG        0x00000001
#define CNTR_UNIT_BYTES      0x00000002
#define CNTR_UNIT_CALLS      0x00000004
#define CNTR_UNIT_TRANS      0x00000008
#define CNTR_UNITS           0x000000fe
#define CNTR_TYPE_IN         0x00000100
#define CNTR_TYPE_OUT        0x00000200
#define CNTR_TYPE_BIDIR      0x00000400
#define CNTR_TYPE_OTHER      0x00000800
#define CNTR_CLASS_LAN       0x00010000
#define CNTR_CLASS_DISK      0x00020000
#define CNTR_CLASS_OTHER     0x00040000


typedef struct report_data_set {
  xmlNodePtr               data_set;
  xmlChar                 *name;
  struct report_data_set  *sd_data_set;
  double                  *values;
  double                   min;
  double                   max;
  double                   mean;
  double                   interval;
  double                   confidence;
  double                   per_test;
  uint32_t                 attr;
  uint32_t                 num_per_run;
  uint32_t                 num_per_test;
} report_data_set_t;


typedef struct report_field {
  xmlNodePtr           field;
  report_data_set_t   *set;
  double              *value;
  char                *label;
  char                *header[MAX_HDR_LINES];
  double               units;
  struct report_field *next;
  char                *separator;
  int                  width;
  int                  precision;
  int                  per_run;
  int                  summary;
  int                  if_value;
} report_field_t;



typedef struct report_part {
  xmlNodePtr           part;
  report_field_t      *fields;
  struct report_part  *next;
  char                *separator;
  int                  hdr_lines;
} report_part_t;


typedef struct report_data {
  FILE                *out_file;
  xmlNodePtr           report_desc;
  xmlChar             *tset_id;
  xmlChar             *tid;
  char                *field_sep;
  char                *part_sep;
  int                  print_run;
  int                  print_test;
  int                  print_raw_cntrs;
  int                  print_hist;
  int                  num_parts;
  int                  num_fields;
  int                  num_data_sets;
  int                  num_sd_sets;
  int                  max_ds_values;
  int                  sd_computed_count;
  report_part_t       *summary;
  report_part_t       *per_test;
  report_field_t      *fields;
  report_data_set_t   *data_sets;
  report_data_set_t   *cpu_set;
  report_data_set_t   *run_time_set;
  double              *values;
  double               num_of_cpus;
  double               count;
  int                  index;
} report_data_t;
 
