#ifndef _DUMP_H_
#define _DUMP_H_

#include <complex.h>

void short_dump(char *filename, float complex *buf, int samples, float scale);
void text_dump(char *filename, float complex *buf, int samples);

#endif /* _DUMP_H_ */
