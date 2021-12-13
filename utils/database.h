#ifndef _DATABASE_H_
#define _DATABASE_H_

typedef struct {
  int freq_index;
  char name[17];
  int scid;
} database_channel_t;

typedef struct {
  database_channel_t *c;
  int count;
} database_t;

int init_database(database_t *d, char *channels_file);
void free_database(database_t *d);
void dump_database(database_t *d);

#endif /* _DATABASE_H_ */
