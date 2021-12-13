#include "fic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "coding/puncture.h"
#include "coding/viterbi_decode.h"
#include "coding/energy_dispersal.h"
#include "utils/crc.h"
#include "fib.h"
#include "utils/log.h"

fic *new_fic(void)
{
  int polys[4] = { 0133, 0171, 0145, 0133 };
  fic *ret = calloc(1, sizeof(fic));
  if (ret == NULL) abort();

  viterbi_init(&ret->v, 7, polys, 4);
  ret->energy = new_energy_dispersal(768/8);

  return ret;
}

void fic_unpuncture(unsigned char *in, unsigned char *out)
{
  int i;

  /* first 21 block of 128 bits: puncturing pattern 16 */
  for (i = 0; i < 21 * 4; i++) {
    unpuncture(16, in, out);
    in += 24;
    out += 32;
  }

  /* next 3 block of 128 bits: puncturing pattern 15 */
  for (i = 0; i < 3 * 4; i++) {
    unpuncture(15, in, out);
    in += 23;
    out += 32;
  }

  /* last 24 bits, puncturing patter: 1100 1100 1100 1100 1100 1100 */
  for (i = 0; i < 6; i++) {
    *out++ = *in++;
    *out++ = *in++;
    *out++ = 2;
    *out++ = 2;
  }
}

int receive_fic_bits(frame_figs *figs, fic *fic, unsigned char *bits, int fib_id)
{
  char curfic[768/8+1];
  char *cur;
  int c;
  int c2;
  int i;
  int bad_crc = 0;
  int fib_error = 0;

  debug("fic bits for fib %d\n", fib_id);

  memcpy(&fic->bits[fic->pos], bits, 768);
  fic->pos += 768;
  if (fic->pos != 2304)
    return 0;
  fic->pos = 0;
  fic_unpuncture(fic->bits, fic->unpunctured);
  memset(curfic, 0, sizeof(curfic));
  viterbi_decode(&fic->v, curfic, fic->unpunctured, 3072+24);
  do_energy_dispersal(fic->energy, curfic);

#if 0
  int i;
debug("YO ");
  for (i = 0; i < 768/8+1; i++) { debug(" %2.2x", (unsigned char)curfic[i]); if (i % 32 == 31) debug("|"); }
  debug("\n");
#endif

  cur = curfic;

  for (i = 0; i < 3; i++, cur += 32) {
    c = crc(cur, 30,  0x11021, 16, 0xffff, CRC_COMPLEMENT);
    c2 = ((unsigned char)cur[30] << 8) | (unsigned char)cur[31];
    if (c != c2) {
      debug("bad crc (computed %x, received %x)\n", c, c2);
      bad_crc++;
      continue;
    }
    if (receive_fib(figs, (unsigned char *)cur, fib_id - 2 + i))
      fib_error++;
  }

  return bad_crc + fib_error;
}
