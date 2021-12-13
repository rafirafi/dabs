#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <complex.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>

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
#include "utils/scan.h"
#include "utils/database.h"
#include "utils/band3.h"

int time_sync(io *in)
{
  float complex ts[T_F + T_NULL];
  int c;
  float energy;
  float min_energy;
  int min_pos;
  int i;

  c = in->get_samples(in, ts, T_F + T_NULL);
  if (c != T_F + T_NULL)
    return -1;

  energy = 0;

  for (i = 0; i < T_NULL; i++)
    energy += cabsf(ts[i]);

  min_pos = 0;
  min_energy = energy;

  for (i = T_NULL; i < T_F + T_NULL; i++) {
    energy -= cabsf(ts[i - T_NULL]);
    energy += cabsf(ts[i]);
    if (energy < min_energy) {
      min_energy = energy;
      min_pos = i - T_NULL + 1;
    }
  }

  if (min_pos > T_F)
    min_pos -= T_F;

  in->put_back_samples(in, &ts[min_pos], T_F + T_NULL - min_pos);

  return min_pos;
}

int fast_time_sync(io *in)
{
  float complex ts[T_NULL + 200];
  int c;
  float energy;
  float min_energy;
  int min_pos;
  int i;

  c = in->get_samples(in, ts, T_NULL + 200);
  if (c != T_NULL + 200)
    return -1;

  energy = 0;

  for (i = 0; i < T_NULL; i++)
    energy += cabsf(ts[i]);

  min_pos = 0;
  min_energy = energy;

  for (i = T_NULL; i < T_NULL + 200; i++) {
    energy -= cabsf(ts[i - T_NULL]);
    energy += cabsf(ts[i]);
    if (energy < min_energy) {
      min_energy = energy;
      min_pos = i - T_NULL + 1;
    }
  }

  if (min_pos > 200)
    min_pos -= 200;

  in->put_back_samples(in, &ts[min_pos], T_NULL + 200 - min_pos);

  return min_pos;
}

#include <sys/select.h>

/* return radio selected by user */
int user_input(database_t *d, int current_radio)
{
  char buf[128];
  int len;
  int i;
  int c;
  fd_set r;
  int s;
  struct timeval tout;
  int do_dump = 0;

  while (1) {
    tout.tv_sec = tout.tv_usec = 0;
    FD_ZERO(&r);
    FD_SET(0, &r);
    s = select(1, &r, NULL, NULL, &tout);
    if (s == -1) { error("select failed\n"); exit(1); }
    if (!FD_ISSET(0, &r)) break;
    len = read(1, buf, 128);
    if (len <= 0) break;
    for (i = 0; i < len; i++) {
      c = buf[i];
      if (c == 'n') {
        current_radio++;
        if (current_radio > d->count - 1) current_radio = 0;
      }
      if (c == 'p') {
        current_radio--;
        if (current_radio < 0) current_radio = d->count - 1;
      }
      if (c == 'c') {
        int next_radio = 0;
        i++;
        /* the parsing here may be weird. If you type "c 1 c 2" then channel
         * 2 is selected. If you type "c 1c 2" then channel 1 is selected.
         * Live with that.
         */
        while (i < len && isspace(buf[i])) i++;
        if (i >= len || !isdigit(buf[i])) break;
        while (i < len && isdigit(buf[i])) {
          next_radio = next_radio * 10 + buf[i] - '0';
          /* the weirdness is because of this i++ because after it if it's
           * not digit then the for(;;) does i++ again and we skip 1 char.
           * Did I say live with that already?
           */
          i++;
        }
        if (next_radio < 0) next_radio = 0;
        if (next_radio > d->count - 1) next_radio = d->count - 1;
        current_radio = next_radio;
      }
      if (c == 'l') do_dump = 1;
    }
  }

  if (do_dump) dump_database(d);

  return current_radio;
}

void usage(void)
{
  int i;

  printf("usage 1: [options] <channels file>\n");
  printf("    play radio, starting from first channel in <channels file>\n");
  printf("usage 2: [options] -scan <channels file>\n");
  printf("    scan and save found channels to <channels file>\n");
  printf("usage 3: [options] -record <channel> <output file>\n");
  printf("    record IQ data of the given channel to <output file>\n");
  printf("options:\n");
  printf("    -f <file>\n");
  printf("        load samples from file instead of SDR\n");
  printf("    -loop\n");
  printf("        loop input file\n");
  printf("    -d\n");
  printf("        debug logs\n");
  printf("    -w\n");
  printf("        warning logs\n");
  printf("    -scan\n");
  printf("        scan channels of DAB band 3, save into file, then exit\n");
  printf("    -record <channel> <output file>\n");
  printf("        channel is one of:");
  for (i = 0; i < band3_size; i++) printf(" %s", band3[i].name);
  printf("\n");
  printf("    -dev <device>\n");
  printf("        device is one of: usrp rtlsdr (default usrp)\n");
  printf("    -no-audio-drop\n");
  printf("        useful when replaying a recording where the audio buffer\n");
  printf("        may be full in which case some audio data may be dropped\n");
  exit(0);
}

void record(char *record_channel, char *record_file, char *dev)
{
  int i;
  float freq;
  io *in;
  FILE *out;
  char buf[1024*4];

  for (i = 0; i < band3_size; i++)
    if (!strcmp(band3[i].name, record_channel))
      break;
  if (i == band3_size) {
    error("unknown channel %s\n", record_channel);
    exit(1);
  }

  freq = band3[i].freq * 1000 * 1000;

  out = fopen(record_file, "w");
  if (out == NULL) {
    error("%s: %s\n", record_file, strerror(errno));
    exit(1);
  }

  in = new_sdr(dev); if (in == NULL) exit(1);

  in->set_gain(in, 1);
  in->set_freq(in, freq);
  in->start(in);

  while (1) {
    if (in->get_raw_samples(in, buf, 1024) != 1024) {
      error("SDR I/O failed\n");
      break;
    }
    if (fwrite(buf, 1024*4, 1, out) != 1) {
      error("%s: %s\n", record_file, strerror(errno));
      break;
    }
  }

  in->stop(in);
  in->free(in);

  if (fclose(out)) {
    error("%s: %s\n", record_file, strerror(errno));
    exit(1);
  }
}

int main(int n, char **v)
{
  char *channels_file = NULL;
  char *in_name = NULL;
  io *in;
  int i;
  int j;
  int start;
  float fine_freq_offset;
  float coarse_freq_offset;
  float complex *in_time;
  float complex *in_time_symbol_1;
  float complex *_cur_freq;
  float complex *_prev_freq;
  float complex *cur_freq;
  float complex *prev_freq;
  float complex *channel;
  float complex *z_ref;
  float complex *z_ref_fft;
  fft_data *fft_data;
  int freq_offset_cur_pos;
  float complex qpsk[K];
  unsigned char p[K*2];
  fic *fic;
  frequency_interleaver *fl;
  frame_figs figs;
  int frame_cif_id = -1;  /* set because of gcc warning */
  int prev_frame_cif_id;
  alsa_out *alsa = NULL;  /* set because of gcc warning */
  msc_config *msc_config = NULL;  /* set because of gcc warning */
  int fig0_0_errors;
  int do_loop = 0;
  int do_scan = 0;
  int do_record = 0;
  char *record_file = NULL;
  char *record_channel = NULL;
  database_t d;
  int current_radio = 0;
  int next_radio;
  int fig0_0_present;
  scanner_t s;
  int dont_process_fig;
  FILE *scan_outfile = NULL;  /* set because of gcc warning */
  char *dev = "usrp";
  int no_audio_drop = 0;

  for (i = 1; i < n; i++) {
    if (!strcmp(v[i], "-h") || !strcmp(v[i], "--help")) usage();
    if (!strcmp(v[i], "-loop")) { do_loop = 1; continue; }
    if (!strcmp(v[i], "-f")) {if(i>n-2)usage(); in_name = v[++i]; continue; }
    if (!strcmp(v[i], "-d")) { log_flags |= DEBUG; continue; }
    if (!strcmp(v[i], "-w")) { log_flags |= WARN; continue; }
    if (!strcmp(v[i], "-scan")) { do_scan = 1; continue; }
    if (!strcmp(v[i], "-record")) {if(i>n-3)usage(); do_record = 1;
                                   record_channel = v[++i];
                                   record_file = v[++i]; continue; }
    if (!strcmp(v[i], "-dev")) {if(i>n-2)usage(); dev = v[++i]; continue; }
    if (!strcmp(v[i], "-no-audio-drop")) { no_audio_drop = 1; continue; }
    if (channels_file == NULL) { channels_file = v[i]; continue; }
    usage();
  }

  if (!do_record && channels_file == NULL) usage();

  /* record */
  if (do_record) {
    record(record_channel, record_file, dev);
    return 0;
  }

  /* scan mode: prepare output file */
  if (do_scan) {
    /* do not overwrite file */
    struct stat sbuf;
    if (stat(channels_file, &sbuf) == 0 || errno != ENOENT) {
      error("%s: cannot create, file already exist\n", channels_file);
      exit(1);
    }
    scan_outfile = fopen(channels_file, "w");
    if (scan_outfile == NULL) {
      error("%s: %s\n", channels_file, strerror(errno));
      exit(1);
    }
  }

  /* play mode: load database */
  if (!do_scan) {
    if (init_database(&d, channels_file)) exit(1);
    if (d.count == 0) { error("no channels?\n"); exit(1); }
  }

  float start_frequency;
  int start_subchannel;

  if (!do_scan) {
    start_frequency = band3[d.c[current_radio].freq_index].freq * 1000000;
    start_subchannel = d.c[current_radio].scid;
  } else {
    start_frequency = 100000000; /* dumb value (100MHz) for start */
  }

  fic = new_fic();

  if (!do_scan) {
    alsa = new_alsa_out(no_audio_drop);
    msc_config = new_msc_config(alsa->receive_audio, alsa);

    msc_set_channel(msc_config, start_frequency, start_subchannel);
  }

  fl = new_frequency_interleaver();

  init_frequency_offset();

  in_time = malloc(sizeof(float complex) * T_S);
  in_time_symbol_1 = malloc(sizeof(float complex) * T_S);
  _cur_freq = malloc(sizeof(float complex) * T_U);
  _prev_freq = malloc(sizeof(float complex) * T_U);
  channel = malloc(sizeof(float complex) * T_U);
  z_ref = malloc(sizeof(float complex) * T_U);
  z_ref_fft = malloc(sizeof(float complex) * T_U);
  if (in_time == NULL || in_time_symbol_1 == NULL ||_cur_freq == NULL
      || _prev_freq == NULL || channel == NULL || z_ref == NULL
      || z_ref_fft == NULL) abort();

  cur_freq = _cur_freq;
  prev_freq = _prev_freq;

  fft_data = init_fft(T_U);

  compute_phase_ref_full(fft_data, z_ref);
  fft(fft_data, z_ref, z_ref_fft);
  /* we need the conjugate of z_ref_fft for coarse offset computation,
   * let's precompute
   */
  for (i = 0; i < T_U; i++)
    z_ref_fft[i] = conjf(z_ref_fft[i]);

  if (in_name != NULL)
    in = new_input_file(in_name, do_loop);
  else
    in = new_sdr(dev);
  if (in == NULL) exit(1);

  in->set_gain(in, 1);
  in->set_freq(in, start_frequency);
  in->start(in);

  int fff = -1;
  if (!do_scan) fff = d.c[current_radio].freq_index;

next_freq:
  if (do_scan) {
    fff++;
    if (fff == band3_size) goto over;
    info("digging %s (%g)\n", band3[fff].name, band3[fff].freq);
    start_frequency = band3[fff].freq * 1000000;
    debug("freq %g %g\n", start_frequency, band3[fff].freq * 1000000);
    in->reset(in);
    in->set_freq(in, start_frequency);
  }
  prev_frame_cif_id = -1;
  fig0_0_present = 0;

restart:
  if (do_scan) debug("RESET LOOOPPP\n");
  reset_scanner(&s);
  fig0_0_errors = 0;

  start = time_sync(in);
  if (start == -1) {
    if (do_scan) { warn("no time sync\n"); goto next_freq; }
    goto over;
  }

next_frame:
  if (!do_scan) {
    next_radio = user_input(&d, current_radio);

    if (next_radio != current_radio) {
      alsa->reset(alsa);
      current_radio = next_radio;
      float freq = band3[d.c[current_radio].freq_index].freq * 1000000;
      start_subchannel = d.c[current_radio].scid;
      info("playing %s\n", d.c[current_radio].name);
      msc_set_channel(msc_config, freq, start_subchannel);
      if (freq != start_frequency) {
        start_frequency = freq;
        in->reset(in);
        in->set_freq(in, start_frequency);
        fff = d.c[current_radio].freq_index;
        goto restart;
      }
    }
  }

  debug("#start %d\n", start);

  if (in->get_samples(in, NULL, T_NULL) != T_NULL) goto over;

  figs.size = 0;

  /* symbol 1 */
  /* first we find the fine frequency offset using signal in time domain
   * then the coarse frequency offset using signal in frequency domain
   * (after removal of fine frequency offset)
   * for next symbols, only the fine frequency offset is computed and
   * we update the previous fine frequency offset within some limited
   * varations (not sure if this logic is good or bad, to be fixed if
   * needed)
   */
  if (in->get_samples(in, in_time, T_S) != T_S) goto over;
  memcpy(in_time_symbol_1, in_time, sizeof(float complex) * T_S);
  fine_freq_offset = frequency_offset_fine(in_time);
  debug("# %g ", fine_freq_offset);
  freq_offset_cur_pos = remove_frequency_offset(0, in_time,
                            fine_freq_offset);
  debug("%g\n", frequency_offset_fine(in_time));

  if (do_scan) {
    int i;
    float e = 0;
    for (i= 0; i < T_S; i++) e += cabsf(in_time[i]);
    info("energy %g\n", e / T_S);
  }

  fft(fft_data, &in_time[Delta], prev_freq);

  coarse_freq_offset = frequency_offset_coarse(prev_freq, z_ref_fft, fft_data);
if (coarse_freq_offset)
printf("coarse %g fine %g both %g\n", coarse_freq_offset, fine_freq_offset, coarse_freq_offset + fine_freq_offset);
//coarse_freq_offset = 0;
  freq_offset_cur_pos = remove_frequency_offset(0, in_time_symbol_1,
                            coarse_freq_offset + fine_freq_offset);

  fft(fft_data, &in_time_symbol_1[Delta], prev_freq);

  //channel_estimate(z_ref, prev_freq, channel);
  //channel_compensate(prev_freq, channel, prev_freq);

  dont_process_fig = 0;
  for (i = 2; i < 77; i++) {
    float delta;
    if (in->get_samples(in, in_time, T_S) != T_S) goto over;
    delta = frequency_offset_fine(in_time) - fine_freq_offset;
    if (delta > 20) delta = 20;
    if (delta < -20) delta = -20;
    fine_freq_offset += delta;
    debug("#%g ", fine_freq_offset);
    freq_offset_cur_pos = remove_frequency_offset(freq_offset_cur_pos,
                              in_time, coarse_freq_offset + fine_freq_offset);
    debug("%g\n", frequency_offset_fine(in_time));

    fft(fft_data, &in_time[Delta], cur_freq);

    //channel_compensate(cur_freq, channel, cur_freq);

    /* frequency de-interleaving done here (using k2n) */
    /* positive frequencies [1..768] */
    for (j = 0; j < K/2; j++)
      qpsk[fl->k2n[FK(j+1)]] = cur_freq[j+1] * conjf(prev_freq[j+1]);
    /* negative frequencies [-768..-1] */
    for (j = 0; j < K/2; j++)
      qpsk[fl->k2n[FK(j-K/2)]] = cur_freq[j+T_U-K/2] * conjf(prev_freq[j+T_U-K/2]);

    symbol_unmap(p, qpsk);

    /* symbols 2, 3 and 4 are for fic */
    if (i < 5) {
      for (j = 0; j < 4; j++) {
        /* FIB = 256 bits, 30 bytes of data, 2 bytes of crc
         * 4 FIBs per symbol, 12 FIBs for the 3 symbols of the FIC
         * 3 FIBs make for 1 FIC and are used for energy dispersal (768 bits)
         * FIC (768 bits) is convolution encoded, giving 3072+24 bits
         * 3072+24 bits are punctured, giving 2304 bits
         * puncturing is:
         *     21 blocks of 128 bits are punctured by PI=16 (rate 8/24)
         *     (we remove 8 bits over 32)
         *     3 blocks of 128 bits are punctured by PI=15 (rate 9/24)
         *     (we remove 9 bits over 32)
         *     last 24 bits are punctured by 1100 1100 1100 1100 .. 1100
         *     (we remove 12 bits over 24)
         * each symbol gets 3072 bits, so:
         * 1st symbol: fic 0 (2304 bits) + fic 1 (768 bits)
         * 2nd symbol: fic 1 (1536 bits) + fic 2 (1536 bits)
         * 3rd symbol: fic 2 (768 bits)  + fic 3 (2304 bits)
         * we send to the fic unit blocks of 768 bits
         * when the fic unit has enough bits it does the decoding
         */
        for (j = 0; j < 4; j++) {
          if (receive_fic_bits(&figs, fic, &p[768*j], (i-2)*4+j)) {
            /* error, clear scanner, skip all figs */
            if (do_scan) warn("some FIC not received correctly, reset scan\n");
            reset_scanner(&s);
            dont_process_fig = 1;
          }
        }
      }
    }

    if (i == 4) {
      int f;
      if (dont_process_fig == 0) for (f = 0; f < figs.size; f++) {
        switch (figs.f[f].type) {
        default: break;
        case FIG0_1: process_f01(&s, &figs.f[f].u.fig0_1); break;
        case FIG0_2: process_f02(&s, &figs.f[f].u.fig0_2); break;
        case FIG1_1: process_f11(&s, &figs.f[f].u.fig1_1); break;
        }
      }
      if (figs.size) debug("got some fig\n");
      /* check that we have FIG 0/0 */
      if (figs.size == 0 || figs.f[0].type != FIG0_0) {
        debug("error; FIG 0/0 not received, skip frame\n");
        fig0_0_errors++;
        int max_errors = do_scan ? 10 : 5;
        if (fig0_0_errors == max_errors) {
          debug("error: too much missed FIG 0/0, full resync\n");
          if (fig0_0_present) goto restart;
          goto next_freq;
        }
        i++;
        goto frame_over;
      }
      fig0_0_errors = 0;
      frame_cif_id = figs.f[0].u.fig0_0.cif_count;
      fig0_0_present++;
      if (frame_cif_id != prev_frame_cif_id + 4) {
        if (do_scan) warn("some missing frames detected, reset scan\n");
        reset_scanner(&s);
      }
      prev_frame_cif_id = frame_cif_id;
      /* we got it all twice? next freq (or check if not in scan mode) */
      if (s._01_size && s._01_size == s._01_twice
          && s._02_size && s._02_size == s._02_twice
          && s._11_size && s._11_size == s._11_twice) {
        if (do_scan) {
          save_config(fff, &s, scan_outfile);
          goto next_freq;
        } else {
          check_config(fff, &s, &d);
          reset_scanner(&s);
        }
      }
      if (do_scan) {
        /* scan mode: skip MSC */
        i++;
        goto frame_over;
      }
      /* get sub channels configuration */
      msc_update_config(msc_config, &figs);
    }

    /* symbols 5..76 are for MSC */
    if (i >= 5)
      receive_msc_bits(msc_config, frame_cif_id + (i-5)/18, p, (i-5) % 18);

#define  SWAP(a, b) do { float complex *x = a; a = b; b = x; } while (0)
    SWAP(cur_freq, prev_freq);
  }

frame_over:
  /* we may have to skip some symbols in case of error */
  for (; i < 77; i++) {
    if (in->get_samples(in, in_time, T_S) != T_S) goto over;
  }

  /* put back last 100 samples of last symbol, for fast time sync */
  in->put_back_samples(in, &in_time[T_S-100], 100);
  start = fast_time_sync(in);
  if (start == -1) goto restart;

  goto next_frame;

over:
  if (do_scan) {
    if (fclose(scan_outfile)) {
      error("%s: %s\n", channels_file, strerror(errno));
      error("you should rescan, an error occured with the file %s\n",
            channels_file);
    }
  }
  in->stop(in);
  in->free(in);

  return 0;
}
