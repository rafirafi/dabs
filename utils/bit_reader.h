#ifndef _BIT_READER_H_
#define _BIT_READER_H_

typedef struct {
  unsigned char *data;
  int nbytes;
  int next_byte;
  int next_bit;
} bit_reader;

void init_bit_reader(bit_reader *b, unsigned char *data, int nbytes);
int get_bits(bit_reader *b, int count);

#endif /* _BIT_READER_H_ */
