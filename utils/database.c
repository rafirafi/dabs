#include "database.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "utils/log.h"
#include "utils/band3.h"

int init_database(database_t *d, char *channels_file)
{
#define CL if (l - line >= ll) goto skip_line;
  database_channel_t dc;
  char line[256];
  int pos;
  char *l;
  int ll;
  int bl;
  int i;
  int name_len;
  FILE *f = fopen(channels_file, "r");
  if (f == NULL) {
    error("%s: %s\n", channels_file, strerror(errno));
    return 1;
  }
  d->c = NULL;
  d->count = 0;

  while (1) {
    /* read a line (if possible) */
    memset(&dc, 0, sizeof(dc));
    pos = 0;
    while (1) {
      int c = fgetc(f);
      if (pos == 0 && c == EOF) goto over;
      if (c == EOF || c == '\n') break;
      if (pos == 255) {
        error("%s: line too long\n", channels_file);
        goto error;
      }
      line[pos] = c;
      pos++;
    }
    line[pos] = 0;
    ll = strlen(line);
    /* parse it (yes, here, barbarian style!) */
    l = line; if (strncmp(l, "CHAN ", 5)) goto skip_line;
    l += 5; CL;
    /* sooooo slow but who cares */
    bl = 0;   /* useless but gcc 10.2.1 warns... */
    for (i = 0; i < band3_size; i++) {
      bl = strlen(band3[i].name);
      if (!strncmp(l, band3[i].name, bl)) break;
    }
    if (i == band3_size) goto skip_line;
    dc.freq_index = i;
    l += bl + 2; CL;
    if (sscanf(l, "%d", &name_len) != 1) goto skip_line;
    if (name_len < 1 || name_len > 16) goto skip_line;
    while (*l != ']') { l++; CL; }
    l++; if (l - line + name_len > ll) goto skip_line;
    memcpy(dc.name, l, name_len);
    l += name_len + 1; CL;
    if (strncmp(l, "scid ", 5)) goto skip_line;
    l += 5; CL;
    if (sscanf(l, "%d", &dc.scid) != 1) goto skip_line;

    d->count++;
    d->c = realloc(d->c, d->count * sizeof(database_channel_t));
    if (d->c == NULL) { error("out of memory\n"); exit(1); }
    d->c[d->count-1] = dc;

    continue;

skip_line:
    warn("%s: skip line '%s'\n", channels_file, line);
  }

over:
  fclose(f);
  return 0;

error:
  free(d->c);
  d->c = NULL;
  d->count = 0;
  fclose(f);
  return 1;
}

void free_database(database_t *d)
{
  free(d->c);
  d->c = NULL;
  d->count = 0;
}

void dump_database(database_t *d)
{
  int i;
  for (i = 0; i < d->count; i++)
    info("%d\t%s\n", i, d->c[i].name);
}
