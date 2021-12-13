#include "msc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "coding/time_interleaving.h"
#include "coding/puncture.h"
#include "coding/viterbi_decode.h"
#include "audio/super_frame.h"
#include "utils/log.h"

msc_config *new_msc_config(void (*send_audio)(void *, void *, int),
                           void *send_audio_data)
{
  msc_config *ret = calloc(1, sizeof(msc_config));
  int polys[4] = { 0133, 0171, 0145, 0133 };

  ret->cif_id = -1;

  ret->energy_dispersal = new_energy_dispersal(10000);

  ret->send_audio = send_audio;
  ret->send_audio_data = send_audio_data;

  viterbi_init(&ret->v, 7, polys, 4);

  return ret;
}

/* decode one CIF */
static void decode_subchannel(msc_config *m, sub_channel *c)
{
  unsigned char bits[864*64];
  char data[10000];
  unsigned char *in;
  unsigned char *out;
  int i, j;
  int nbits;
  int nbytes;

  /* unpuncture */
  in = &m->in[m->in_start][c->start * 64];
  out = bits;

  for (j = 0; j < 4; j++) {
    for (i = 0; i < c->l[j] * 4; i++) {
      unpuncture(c->pi[j], in, out);
      in += c->pi[j] + 8;
      out += 32;
    }
  }

  /* last 24 bits, puncturing patter: 1100 1100 1100 1100 1100 1100 */
  for (i = 0; i < 6; i++) {
    *out++ = *in++;
    *out++ = *in++;
    *out++ = 2;
    *out++ = 2;
  }

  nbits = out - bits;

  debug("yoyoyo out bits %d in bits %d\n", (int)(out-bits),
        (int)(in-&m->in[m->in_start][c->start * 64]));

  memset(data, 0, sizeof(data));
  viterbi_decode(&m->v, data, bits, nbits);

  /* convolution-coded data has 4 bits per data bit */
  nbytes = (nbits - 24) / 4 / 8;

  do_energy_dispersal_n(m->energy_dispersal, data, nbytes);

  debug("decoded:");
  for (i = 0; i<nbytes; i++) debug(" %2.2x", (unsigned char)data[i]);
  debug("\n");

  if (m->audio == NULL)
    m->audio = new_super_frame(c->bitrate, m->send_audio, m->send_audio_data);

  audio_receive_frame(m->audio, m->in_start_cif_id,
                      (unsigned char *)data, nbytes);
}

static void msc_decode_subchannels(msc_config *m)
{
  int i;

  for (i = 0; i < 64; i++) {
    sub_channel *c = &m->sc[i];
    if (!c->configured)
      continue;
    if (i == m->subchannel)
      decode_subchannel(m, c);
  }
}

static void reset_msc_data(msc_config *m, int cif_id)
{
  m->in_start = 0;
  m->in_size = 0;
  m->cif_id = cif_id;
  m->in_start_cif_id = m->cif_id;
}

void receive_msc_bits(msc_config *m, int cif_id, unsigned char *bits, int cif_symbol)
{
  int i, j;
  int r;

  debug("receive_msc_bits cif_id %d cif_symbol %d\n", cif_id, cif_symbol);

  /* if some CIF is missing, reset */
  if (cif_id != m->cif_id)
    reset_msc_data(m, cif_id);

  /* put data - do time de-interleaving */
  r = (m->in_start + (m->cif_id - m->in_start_cif_id + 5000) % 5000) % 16;

  /* time interleaving is a per-subchannel thing, but the start of a
   * subchannel is multiple of 64 bits (so of 16 too), so we can do it
   * globally here (time interleaving works by blocks of 16 bits)
   */
  for (i = cif_symbol * 3072, j = 0; j < 3072; i++, j++)
    m->in[TIME_DEINTERLEAVE(r, i)][i] = bits[j];

  if (cif_symbol != 17)
    return;

  m->cif_id = (m->cif_id + 1) % 5000;

  /* all symbols for the CIF received, let's process (if we can) */
  m->in_size++;
  if (m->in_size != 16)
    return;

  /* we can decode data for time r-15 (which is time m->in_start) */
  msc_decode_subchannels(m);

  /* get ready for next CIF */
  m->in_start = (m->in_start + 1) % 16;
  m->in_start_cif_id = (m->in_start_cif_id + 1) % 5000;
  m->in_size--;
}

static void check_or_set_subchannel(sub_channel *new, sub_channel *c)
{
  if (c->configured == 0) {
    *c = *new;
    debug("new subchannel %d\n", c->id);
    return;
  }

  if (memcmp(c, new, sizeof(sub_channel))) {
    error("sub channel %d configuration changed, not handled\n", c->id);
    exit(1);
  }
}

static void msc_update_0_1(msc_config *c, fig0_1 *f)
{
  int i;
  sub_channel sc;

  for (i = 0; i < f->n; i++) {
    if (f->sub_channel[i].is_long && f->sub_channel[i].option == 0) {
      int scdiv[4] = { 12, 8, 6, 4 };
      int n = f->sub_channel[i].sc_size / scdiv[f->sub_channel[i].protection_level];
      if (f->sub_channel[i].sc_size % scdiv[f->sub_channel[i].protection_level]) {
        error("bad size in FIG 0/1, skipping sub channel\n");
        continue;
      }
      sc.configured = 1;
      sc.start = f->sub_channel[i].start_address;
      sc.bitrate = 8 * n;
      sc.id = f->sub_channel[i].sc_id;
      switch (f->sub_channel[i].protection_level) {
      case 0 /* 1-A */:
        sc.l[0]  = 6 * n - 3; sc.l[1]  = 3;
        sc.pi[0] = 24;        sc.pi[1] = 23;
        break;
      case 1 /* 2-A */:
        if (n == 1) {
          sc.l[0]  = 5;  sc.l[1] = 1;
          sc.pi[0] = 13; sc.pi[1] = 12;
        } else {
          sc.l[0]  = 2 * n - 3; sc.l[1]  = 4 * n + 3;
          sc.pi[0] = 14;        sc.pi[1] = 13;
        }
        break;
      case 2 /* 3-A */:
        sc.l[0]  = 6 * n - 3; sc.l[1]  = 3;
        sc.pi[0] = 8;         sc.pi[1] = 7;
        break;
      case 3 /* 4-A */:
        sc.l[0]  = 4 * n - 3; sc.l[1]  = 2 * n + 3;
        sc.pi[0] = 3;         sc.pi[1] = 2;
        break;
      }
      sc.l[2] = sc.l[3] = 0;
      sc.pi[2] = sc.pi[3] = 0;
      check_or_set_subchannel(&sc, &c->sc[f->sub_channel[i].sc_id]);
    } else {
      error("%s:%d:%s: TODO\n", __FILE__, __LINE__, __FUNCTION__);
      exit(1);
    }
  }
}

static void check_or_set_service(service *new, msc_config *c)
{
  int i;
  debug("check_or_set_service called\n");
  /* if already there, has to be same */
  for (i = 0; i < c->service_count; i++) {
    if (c->sr[i].service_reference != new->service_reference) continue;
    if (memcmp(new, &c->sr[i], sizeof(*new))) {
      error("service %d changed (is it possible?), not handled\n",
            new->service_reference);
      exit(1);
    }
    return;
  }
  /* not there, add */
  if (c->service_count == 64) {
    error("more than 64 services configured, not handled\n");
    exit(1);
  }
  c->sr[c->service_count] = *new;
  c->service_count++;
  debug("new service %d\n", new->service_reference);
}

static void msc_update_0_2(msc_config *c, fig0_2 *f)
{
  int i;
  int j;
  service sv;

  for (i = 0; i < f->n; i++) {
    if (f->service[i].ca_id) {
      warn("CAid is set, skip service\n");
      continue;
    }
    memset(&sv, 0, sizeof(sv));
    sv.configured = 1;
    sv.ecc = f->header.pd == 0 ? -1 : f->service[i].ecc;
    sv.country_id = f->service[i].country_id;
    sv.service_reference = f->service[i].service_reference;
    for (j = 0; j < f->service[i].n; j++) {
      sv.sc[j].configured = 1;
      switch (f->service[i].tm[j].tm_id) {
      case 0:
        sv.sc[j].type = AUDIO_STREAM;
        if (!(f->service[i].tm[j].asct == 0 ||
              f->service[i].tm[j].asct == 63)) {
          warn("unhandled audio type (%d), not DAB, not DAB+\n",
               f->service[i].tm[j].asct);
        }
        sv.sc[j].u.audio_stream.type = f->service[i].tm[j].asct == 0 ?
            DAB : f->service[i].tm[j].asct == 63 ? DAB_PLUS : UNKNOWN_AUDIO;
        sv.sc[j].u.audio_stream.subchannel = f->service[i].tm[j].scid;
        sv.sc[j].u.audio_stream.is_primary = f->service[i].tm[j].ps;
        sv.sc[j].u.audio_stream.has_control_access = f->service[i].tm[j].ca_flag;
        break;
      /* for the other cases, we don't deal with them, let's not bother */
      case 1: sv.sc[j].type = DATA_STREAM;  break;
      case 2: sv.sc[j].type = RESERVED;     break;
      case 3: sv.sc[j].type = DATA_PACKET;  break;
      }
    }
    check_or_set_service(&sv, c);
  }
}

static void msc_update_1_1(msc_config *c, fig1_1 *f)
{
  int i;
  /* look for the service to set the name to */
  for (i = 0; c->sr[i].configured; i++) {
    if (c->sr[i].service_reference != f->service_reference) continue;
    if (c->sn[i].configured == 0) {
      c->sn[i].configured = 1;
      memcpy(c->sn[i].name, f->header.charfield, 16);
      c->sn[i].name[16] = 0;
      debug("name for service %d is '%s'\n", f->service_reference,
            c->sn[i].name);
      return;
    }
    /* name should not changed */
    if (memcmp(f->header.charfield, c->sn[i].name, 16)) {
      error("service name changed\n");
      exit(1);
    }
    return;
  }
}

void msc_update_config(msc_config *c, frame_figs *f)
{
  int i;

  for (i = 0; i < f->size; i++) {
    switch (f->f[i].type) {
      case FIG0_1: msc_update_0_1(c, &f->f[i].u.fig0_1); break;
      case FIG0_2: msc_update_0_2(c, &f->f[i].u.fig0_2); break;
      case FIG1_1: msc_update_1_1(c, &f->f[i].u.fig1_1); break;
      default: break;
    }
  }
}

static void clear_msc_config(msc_config *c)
{
  /* clear channels and services */
  memset(&c->sc, 0, sizeof(c->sc));
  memset(&c->sr, 0, sizeof(c->sr));
  memset(&c->sn, 0, sizeof(c->sn));
  c->service_count = 0;
}

void msc_set_channel(msc_config *c, float frequency, int subchannel)
{
  if (frequency == c->frequency && subchannel == c->subchannel) return;

  if (frequency != c->frequency) {
    reset_msc_data(c, -1);
    clear_msc_config(c);
    c->subchannel = -1;
  }

  if (subchannel != c->subchannel) {
    if (c->audio != NULL)
      free_super_frame(c->audio);
    c->audio = NULL;
  }

  c->frequency = frequency;
  c->subchannel = subchannel;
}
