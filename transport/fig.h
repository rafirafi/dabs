#ifndef _FIG_H_
#define _FIG_H_

typedef enum {
  FIG0, FIG0_0, FIG0_1, FIG0_2,
  FIG1, FIG1_0, FIG1_1, FIG1_4
} fig_type;

typedef struct {
  int cn;
  int oe;
  int pd;
  int extension;
} fig0;

typedef struct {
  fig0 header;
  int country_id;
  int ensemble_reference;
  int change_flags;
  int al_flag;
  int cif_count;
  int occurence_change;  /* valid if change_flags != 0 */
} fig0_0;

typedef struct {
  fig0 header;
  int n;
  struct {
    int sc_id;
    int start_address;
    int is_long;
    int table_switch;         /* valid if is_long = 0 */
    int table_index;          /* valid if is_long = 0 */
    int option;               /* valid if is_long = 1 */
    int protection_level;     /* valid if is_long = 1 */
    int sc_size;              /* valid if is_long = 1 */
  } sub_channel[10];
} fig0_1;

typedef struct {
  fig0 header;
  int n;
  struct {
    int ecc;                     /* valid if header.pd = 1 */
    int country_id;
    int service_reference;
    int ca_id;
    int n;
    struct {
      int tm_id;
      int asct;       /* valid if tm_id = 0 */
      int dsct;       /* valid if tm_id = 1 */
      int scid;
      int ps;
      int ca_flag;
    } tm[16];
  } service[10];
} fig0_2;

typedef struct {
  int charset;
  int extension;
  char charfield[17];
  int char_flag_field;
} fig1;

typedef struct {
  fig1 header;
  //int ensemble_identifier;
  int country_id;
  int ensemble_reference;
} fig1_0;

typedef struct {
  fig1 header;
  //int service_identifier;
  int country_id;
  int service_reference;
} fig1_1;

typedef struct {
  fig1 header;
  int pd;
  int scids;
  int ecc;                     /* valid if header.pd = 1 */
  int country_id;
  int service_reference;
} fig1_4;

typedef struct {
  int fib_id;
  fig_type type;
  union {
    fig0   fig0;
    fig0_0 fig0_0;
    fig0_1 fig0_1;
    fig0_2 fig0_2;
    fig1   fig1;
    fig1_0 fig1_0;
    fig1_1 fig1_1;
    fig1_4 fig1_4;
  } u;
} fig;

typedef struct {
  fig f[30*12];       /* no more than 30 figs per fib and 12 fibs per frame */
  int size;
} frame_figs;

#endif /* _FIG_H_ */
