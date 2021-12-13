#include "scan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <complex.h>
#include <sys/stat.h>
#include <errno.h>

#include "phy/signal.h"
#include "utils/file.h"
#include "phy/frequency_offset.h"
#include "utils/fft.h"
#include "phy/channel.h"
#include "phy/qpsk.h"
#include "transport/fic.h"
#include "transport/fig.h"
#include "coding/frequency_interleaving.h"
#include "transport/msc.h"
#include "sdr/sdr.h"
#include "utils/io.h"
#include "audio/alsa.h"
#include "utils/log.h"
#include "utils/band3.h"

void save_config(int chan, scanner_t *s, FILE *out)
{
  int i;
  int j;
  int k;

  for (i = 0; i < s->_02_size; i++) {
    /* get corresponding _01 (same sc_id) */
    for (j = 0; j < s->_01_size; j++) {
      if (s->_02[i].sc_id == s->_01[j].sc_id)
        break;
    }
    if (j == s->_01_size) {
      error("subchannel config not found, skip service %d\n",
            s->_02[i].service_reference);
      continue;
    }

    /* get name */
    for (k = 0; k < s->_11_size; k++) {
      if (s->_02[i].service_reference == s->_11[k].service_reference
          && s->_02[i].country_id == s->_11[k].country_id)
        break;
    }
    if (k == s->_11_size) {
      error("name not found, skip service %d\n",
            s->_02[i].service_reference);
      continue;
    }

    /* save */
    info("CHAN %s [%d]%s scid %d\n",
         band3[chan].name,
         (int)strlen(s->_11[k].charfield),
         s->_11[k].charfield,
         s->_02[i].sc_id);
    fprintf(out, "CHAN %s [%d]%s scid %d\n",
            band3[chan].name,
            (int)strlen(s->_11[k].charfield),
            s->_11[k].charfield,
            s->_02[i].sc_id);
  }
}

void check_config(int chan, scanner_t *s, database_t *d)
{
  int i;
  int j;
  int k;
  int dd;

  for (i = 0; i < s->_02_size; i++) {
    /* get corresponding _01 (same sc_id) */
    for (j = 0; j < s->_01_size; j++) {
      if (s->_02[i].sc_id == s->_01[j].sc_id)
        break;
    }
    if (j == s->_01_size) {
      error("subchannel config not found, skip service %d\n",
            s->_02[i].service_reference);
      continue;
    }

    /* get name */
    for (k = 0; k < s->_11_size; k++) {
      if (s->_02[i].service_reference == s->_11[k].service_reference
          && s->_02[i].country_id == s->_11[k].country_id)
        break;
    }
    if (k == s->_11_size) {
      error("name not found, skip service %d\n",
            s->_02[i].service_reference);
      continue;
    }

    /* look for channel in database */
    for (dd = 0; dd < d->count; dd++) {
      if (d->c[dd].freq_index == chan && d->c[dd].scid == s->_02[i].sc_id)
        break;
    }
    if (dd == d->count) {
      error("not found channel %s [%d]%s scid %d\n",
            band3[chan].name,
            (int)strlen(s->_11[k].charfield),
            s->_11[k].charfield,
            s->_02[i].sc_id);
      goto error;
    }
    if (strcmp(d->c[dd].name, s->_11[k].charfield)) {
      error("bad name, database has [%d]'%s' but ota says %s [%d]'%s' scid %d\n",
            (int)strlen(d->c[dd].name),
            d->c[dd].name,
            band3[chan].name,
            (int)strlen(s->_11[k].charfield),
            s->_11[k].charfield,
            s->_02[i].sc_id);
      goto error;
    }
  }

  return;

error:
  error("database is outdated, rescan\n");
  exit(1);
}

void process_f01(scanner_t *s, fig0_1 *f)
{
  int i;
  int j;
  for (j = 0; j < f->n; j++) {
    for (i = 0; i < s->_01_size; i++) {
      if (s->_01[i].sc_id == f->sub_channel[j].sc_id)
        break;
    }
    /* if already there, increase recv_count and update _twice if needed */
    if (i != s->_01_size) {
      s->_01[i].recv_count++;
      if (s->_01[i].recv_count == 2)
        s->_01_twice++;
      continue;
    }
    /* not there, insert if free space */
    if (s->_01_size == 256) { error("f01 full\n"); exit(1); }
    s->_01_size++;
    s->_01[i].sc_id            = f->sub_channel[j].sc_id;
    s->_01[i].start_address    = f->sub_channel[j].start_address;
    s->_01[i].is_long          = f->sub_channel[j].is_long;
    s->_01[i].option           = f->sub_channel[j].option;
    s->_01[i].protection_level = f->sub_channel[j].protection_level;
    s->_01[i].sc_size          = f->sub_channel[j].sc_size;
    s->_01[i].table_switch     = f->sub_channel[j].table_switch;
    s->_01[i].table_index      = f->sub_channel[j].table_index;
    s->_01[i].recv_count = 1;
  }
}

void process_f02(scanner_t *s, fig0_2 *f)
{
  int i, j, k;

  for (j = 0; j < f->n; j++) {
    for (k = 0; k < f->service[j].n; k++) {
      if (f->service[j].tm[k].ps == 0) {
        warn("skip non primary service\n");
        continue;
      }
      if (f->service[j].tm[k].tm_id != 0) {
        warn("skip non audio service\n");
        continue;
      }
      if (f->service[j].tm[k].asct != 63) {
        warn("skip non DAB+ audio service\n");
        continue;
      }
      if (f->service[j].tm[k].ca_flag != 0) {
        warn("skip DAB+ audio service with Access Control\n");
        continue;
      }
      for (i = 0; i < s->_02_size; i++) {
        if (s->_02[i].service_reference == f->service[j].service_reference
            && s->_02[i].country_id == f->service[j].country_id)
          break;
      }
      /* if already there, increase recv_count and update _twice if needed */
      if (i != s->_02_size) {
        s->_02[i].recv_count++;
        if (s->_02[i].recv_count == 2)
          s->_02_twice++;
        continue;
      }
      /* not there, insert if free space */
      if (s->_02_size == 256) { error("f02 full\n"); exit(1); }
      s->_02_size++;
      s->_02[i].sc_id             = f->service[j].tm[k].scid;
      s->_02[i].service_reference = f->service[j].service_reference;
      s->_02[i].country_id        = f->service[j].country_id;
      s->_02[i].recv_count = 1;
    }
  }
}

void process_f11(scanner_t *s, fig1_1 *f)
{
  int i;

  for (i = 0; i < s->_11_size; i++) {
    if (s->_11[i].service_reference == f->service_reference
        && s->_11[i].country_id == f->country_id)
      break;
  }
  if (i != s->_11_size) {
    s->_11[i].recv_count++;
    if (s->_11[i].recv_count == 2)
      s->_11_twice++;
    return;
  }
  if (s->_11_size == 256) { error("f11 full\n"); exit(1); }
  s->_11_size++;
  s->_11[i].service_reference = f->service_reference;
  s->_11[i].country_id = f->country_id;
  memcpy(s->_11[i].charfield, f->header.charfield, 17);
  s->_11[i].recv_count = 1;
}

void reset_scanner(scanner_t *s)
{
  s->_01_size = s->_01_twice = 0;
  s->_02_size = s->_02_twice = 0;
  s->_11_size = s->_11_twice = 0;
}
