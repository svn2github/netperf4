
typedef struct confidence {
  double value;
  int    count;
  int    min_count;
  int    max_count;
  int    level;
  double interval;
} confidence_t;

extern double get_confidence( double *values, confidence_t *conf, double *mean );

double set_confidence_interval( char *desired_interval );

int    set_confidence_level( char *desired_level );



