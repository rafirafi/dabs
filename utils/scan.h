#ifndef _SCAN_H_
#define _SCAN_H_

#include <stdio.h>

#include "transport/fig.h"
#include "utils/database.h"

typedef struct {
  int sc_id;
  int start_address;
  int is_long;
  int option;               /* valid if is_long == 1 */
  int protection_level;     /* valid if is_long == 1 */
  int sc_size;              /* valid if is_long == 1 */
  int table_switch;         /* valid if is_long == 0 */
  int table_index;          /* valid if is_long == 0 */
  int recv_count;
} _01;

typedef struct {
  int sc_id;
  int service_reference;
  int country_id;
  int recv_count;
} _02;

typedef struct {
  int service_reference;
  int country_id;
  char charfield[17];
  int recv_count;
} _11;

typedef struct {
  _01 _01[256];
 int _01_size;
 int _01_twice;
  _02 _02[256];
 int _02_size;
 int _02_twice;
  _11 _11[256];
 int _11_size;
 int _11_twice;
} scanner_t;

void reset_scanner(scanner_t *s);
void process_f01(scanner_t *s, fig0_1 *f);
void process_f02(scanner_t *s, fig0_2 *f);
void process_f11(scanner_t *s, fig1_1 *f);
void save_config(int chan, scanner_t *s, FILE *out);
void check_config(int chan, scanner_t *s, database_t *d);

#endif /* _SCAN_H_ */
