char netconfidence_id[]="\
@(#)netconfidence.c (c) Copyright 2005, Hewlett-Packard Company, $Id$";

/*

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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "netperf.h"

extern int debug;
extern FILE * where;

const char *netperf_conf_level[]  = {
   "df",
   "50",
   "70",
   "80",
   "90",
   "95",
   "98",
   "99",
   "99.8",
   "99.9"
};

/*	Upper Tail Probability table	*/

static double t_dist[][10] = {
  {    0,  0.25,  0.15,   0.1,  0.05,  0.025,   0.01,  0.005,  0.001, 0.0005},
  {    1, 1.376, 1.963, 3.078, 6.314, 12.706, 31.821, 63.657, 318.31, 636.62},
  {    2, 1.061, 1.386, 1.886, 2.920,  4.303,  6.965,  9.925, 22.327, 31.599},
  {    3, 0.978, 1.250, 1.638, 2.353,  3.182,  4.541,  5.841, 10.215, 12.924},
  {    4, 0.941, 1.190, 1.533, 2.132,  2.776,  3.747,  4.604,  7.173,  8.610},
  {    5, 0.920, 1.156, 1.476, 2.015,  2.571,  3.365,  4.032,  5.893,  6.869},
  {    6, 0.906, 1.134, 1.440, 1.943,  2.447,  3.143,  3.707,  5.208,  5.959},
  {    7, 0.896, 1.119, 1.415, 1.895,  2.365,  2.998,  3.499,  4.785,  5.408},
  {    8, 0.889, 1.108, 1.397, 1.860,  2.306,  2.896,  3.355,  4.501,  5.041},
  {    9, 0.883, 1.100, 1.383, 1.833,  2.262,  2.821,  3.250,  4.297,  4.781},
  {   10, 0.879, 1.093, 1.372, 1.812,  2.228,  2.764,  3.169,  4.144,  4.587},
  {   11, 0.876, 1.088, 1.363, 1.796,  2.201,  2.718,  3.106,  4.025,  4.437},
  {   12, 0.873, 1.083, 1.356, 1.782,  2.179,  2.681,  3.055,  3.930,  4.318},
  {   13, 0.870, 1.079, 1.350, 1.771,  2.160,  2.650,  3.012,  3.852,  4.221},
  {   14, 0.868, 1.076, 1.345, 1.761,  2.145,  2.624,  2.977,  3.787,  4.140},
  {   15, 0.866, 1.074, 1.341, 1.753,  2.131,  2.602,  2.947,  3.733,  4.073},
  {   16, 0.865, 1.071, 1.337, 1.746,  2.120,  2.583,  2.921,  3.686,  4.015},
  {   17, 0.863, 1.069, 1.333, 1.740,  2.110,  2.567,  2.898,  3.646,  3.965},
  {   18, 0.862, 1.067, 1.330, 1.734,  2.101,  2.552,  2.878,  3.610,  3.922},
  {   19, 0.861, 1.066, 1.328, 1.729,  2.093,  2.539,  2.861,  3.579,  3.883},
  {   20, 0.860, 1.064, 1.325, 1.725,  2.086,  2.528,  2.845,  3.552,  3.850},
  {   21, 0.859, 1.063, 1.323, 1.721,  2.080,  2.518,  2.831,  3.527,  3.819},
  {   22, 0.858, 1.061, 1.321, 1.717,  2.074,  2.508,  2.819,  3.505,  3.792},
  {   23, 0.858, 1.060, 1.319, 1.714,  2.069,  2.500,  2.807,  3.485,  3.768},
  {   24, 0.857, 1.059, 1.318, 1.711,  2.064,  2.492,  2.797,  3.467,  3.745},
  {   25, 0.856, 1.058, 1.316, 1.708,  2.060,  2.485,  2.787,  3.450,  3.725},
  {   26, 0.856, 1.058, 1.315, 1.706,  2.056,  2.479,  2.779,  3.435,  3.707},
  {   27, 0.855, 1.057, 1.314, 1.703,  2.052,  2.473,  2.771,  3.421,  3.690},
  {   28, 0.855, 1.056, 1.313, 1.701,  2.048,  2.467,  2.763,  3.408,  3.674},
  {   29, 0.854, 1.055, 1.311, 1.699,  2.045,  2.462,  2.756,  3.396,  3.659},
  {   30, 0.854, 1.055, 1.310, 1.697,  2.042,  2.457,  2.750,  3.385,  3.646},
  {   40, 0.851, 1.050, 1.303, 1.684,  2.021,  2.423,  2.704,  3.307,  3.551},
  {   50, 0.849, 1.047, 1.299, 1.676,  2.009,  2.403,  2.678,  3.261,  3.496},
  {   75, 0.846, 1.044, 1.293, 1.665,  1.992,  2.377,  2.643,  3.202,  3.425},
  {  100, 0.845, 1.042, 1.290, 1.660,  1.984,  2.364,  2.626,  3.174,  3.390},
  {  200, 0.843, 1.039, 1.286, 1.653,  1.972,  2.345,  2.601,  3.131,  3.340},
  { 1000, 0.842, 1.037, 1.282, 1.646,  1.962,  2.330,  2.581,  3.098,  3.300},
  {  -1 , 0.841, 1.036, 1.282, 1.645,  1.960,  2.326,  2.576,  3.091,  3.291}
/*    df,   50%,   70%,   80%,   90%,    95%,    98%,    99%,  99.8%,  99.9% */
/*				Confidence	Level			     */
};

int
set_confidence_level(char *desired_level)
{
  int i=0;
  int level = 0;
  
  for (i=9;i>0;i--) {
    if (strcmp(desired_level,netperf_conf_level[i]) == 0) {
      level = i;
      break;
    }
  }
  if (level == 0) {
    fprintf(where,"Invalid confidence_level of %s", desired_level);
    fprintf(where," was specified! Using default confidence_level of 99%%\n");
    fflush(where);
    level = 7;
  }
  return(level);
}


double
set_confidence_interval(char *desired_interval)
{
  int interval;

  interval = atof(desired_interval);
  if ((interval > 0.5) || (interval <= 0.0)) {
    fprintf(where,"Using default confidence_interval of 0.05 instead of %s\n",
            desired_interval);
    fflush(where);
    interval = 0.05;
  }
  return(interval);
}


static double
tdist(int level, int df)
{
  double value = 0.0;
  int i;
  i = level;
  if (df <=  30 ) {
    value = t_dist[df][i];
  } else if (df >   30 ) {
    value = t_dist[31][i];
  } else if (df >   40 ) {
    value = t_dist[32][i];
  } else if (df >   50 ) {
    value = t_dist[33][i];
  } else if (df >   75 ) {
    value = t_dist[34][i];
  } else if (df >  100 ) {
    value = t_dist[35][i];
  } else if (df >  200 ) {
    value = t_dist[36][i];
  } else if (df > 1000 ) {
    value = t_dist[37][i];
  }
  if (debug) {
    fprintf(where,"tdist: t_dist[%d][%d] = %7.3f\n",df,i,value);
    fflush(where);
  }
  return( value );
}


static double
sample_mean(double *values, int count)
{
  double total=0.0;
  int i;

  for (i=0; i < count; i++) {
    total=total+values[i];
  }
  return(total/count);
}


static double 
sample_stddev(double *values, int count, double *avg) 
{
  double variance = 0.0;
  int i;
  
  *avg = sample_mean(values,count);

  for (i=0; i < count; i++) {
    variance = variance + pow((values[i] - *avg),2);
  }
 
  if (count > 1) {
    variance = variance / (count - 1.0);
  };

  return(sqrt(variance));
}

double
get_confidence(double *values, confidence_t *confidence, double *avg)
{
  double mean;
  double sigma;
  double interval;
  int    count = confidence->count;
  double delta = -1.0/0.0;

  /* _                          */
  /* X +/- t[a/2] * (s/sqrt(n)) */
  /*                            */

  sigma    = sample_stddev(values, count, &mean);
  if (confidence->count > 1) {
    interval = tdist(confidence->level, count-1) * sigma / sqrt(count);
    delta    = (mean * confidence->interval) - (2.0 * interval);
  }
  *avg     = mean;

  return(delta);
}

 /* display_confidence() is called when we could not achieve the */
 /* desired confidence in the results. it will print the achieved */
 /* confidence to "where" raj 11/94 */
void
display_confidence(double confidence_result, double confidence_local_cpu)

{
  fprintf(where,
          "!!! WARNING\n");
  fprintf(where,
          "!!! Desired confidence was not achieved within ");
  fprintf(where,
          "the specified iterations.\n");
  fprintf(where,
          "!!! This implies that there was variability in ");
  fprintf(where,
          "the test environment that\n");
  fprintf(where,
          "!!! must be investigated before going further.\n");
  fprintf(where,
          "!!! Confidence intervals: Throughput      : %4.1f%%\n",
           confidence_result *100.0);
  fprintf(where,
          "!!!                       Local CPU util  : %4.1f%%\n",
           confidence_local_cpu *100.0);
  /* remote cpu is not supported for netperf4 */
}
