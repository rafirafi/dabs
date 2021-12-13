#include "usrp_low.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include <uhd/usrp/multi_usrp.hpp>

#include "utils/log.h"

typedef struct {
  shared_buffer *b;
  uhd::usrp::multi_usrp::sptr usrp;
  uhd::rx_streamer::sptr rx_stream;
  volatile int stop;
  pthread_t t;
  volatile int freq_has_changed;
} usrp_low;

extern "C" {

void *new_usrp_low(shared_buffer *b)
{
  usrp_low *ret;
  std::string args = ""; //type=b200";
  uhd::device_addrs_t device_adds = uhd::device::find(args);

  ret = (usrp_low *)calloc(1, sizeof(usrp_low));
  if (ret == NULL) { error("out of memory\n"); exit(1); }

  ret->b = b;

  if(device_adds.size() == 0) { error("no device?\n"); abort(); }
  args += ",num_recv_frames=256" ; 
  ret->usrp = uhd::usrp::multi_usrp::make(args);
  ret->usrp->set_rx_rate(2.048e6, 0);

  uhd::stream_args_t stream_args_rx("sc16", "sc16");
  stream_args_rx.channels.push_back(0);
  ret->rx_stream = ret->usrp->get_rx_stream(stream_args_rx);

  ret->usrp->set_rx_bandwidth(2.048e6, 0);
  ret->usrp->set_time_now(uhd::time_spec_t(0.0));

  return (void *)ret;
}

void usrp_low_set_gain(void *_f, float gain)
{
  usrp_low *f = (usrp_low *)_f;
  ::uhd::gain_range_t gain_range = f->usrp->get_rx_gain_range(0);
  f->usrp->set_rx_gain(gain_range.stop() * gain, 0);
  //f->usrp->set_rx_gain(40, 0);
  if (pthread_mutex_lock(&f->b->m)) abort();
  f->b->start = 0;
  f->b->size = 0;
  if (pthread_mutex_unlock(&f->b->m)) abort();
}

void usrp_low_set_freq(void *_f, float freq)
{
  usrp_low *f = (usrp_low *)_f;
  f->usrp->set_rx_freq(freq, 0);
  f->freq_has_changed = 1;
  debug("rx frequency set %g\n", freq);
}

static unsigned char buf[1024*4];
static void *rx(void *_f)
{
  usrp_low *f = (usrp_low *)_f;
  uhd::rx_metadata_t rx_md;
  int pos;

  uhd::stream_cmd_t cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
  cmd.time_spec = f->usrp->get_time_now() + uhd::time_spec_t(0.05);
  cmd.stream_now = false; // start at constant delay
  f->rx_stream->issue_stream_cmd(cmd);

  while (!f->stop) {
    int n = f->rx_stream->recv(buf, 1024, rx_md);
    if (n != 1024) error("got %d samples instead of 1024\n", n);
    switch(rx_md.error_code){
    case uhd::rx_metadata_t::ERROR_CODE_NONE:
      break;
    case uhd::rx_metadata_t::ERROR_CODE_OVERFLOW:
      error("[recv] USRP RX OVERFLOW!\n");
      break;
    case uhd::rx_metadata_t::ERROR_CODE_TIMEOUT:
      error("[recv] USRP RX TIMEOUT!\n");
      break;
    default:
      error("[recv] Unexpected error on RX, Error code: 0x%x\n",
            rx_md.error_code);
      break;
    }

if (0) {
  static FILE *f = NULL;
  if (f==NULL) f = fopen("/tmp/rec.raw", "w");
  if (f==NULL) abort();
  fwrite(buf, 1024*4, 1, f);
}

    if (pthread_mutex_lock(&f->b->m)) abort();

    if (f->b->size > 2 * 1024 * 1024 * 4 - 1024 * 4) {
      error("shared buffer full, dropping data\n");
      goto drop;
    }

    if (f->freq_has_changed) {
      f->freq_has_changed = 0;
      f->b->start = 0;
      f->b->size = 0;
      goto drop;
    }

    pos = f->b->start + f->b->size;
    pos %= sizeof(f->b->buf);
    memcpy(f->b->buf + pos, buf, 1024*4);
    f->b->size += 1024 * 4;

drop:
    if (pthread_cond_signal(&f->b->c)) abort();
    if (pthread_mutex_unlock(&f->b->m)) abort();
  }

  uhd::stream_cmd_t cmd2(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
  cmd2.time_spec = f->usrp->get_time_now();
  cmd2.stream_now = true;
  f->rx_stream->issue_stream_cmd(cmd2);

  return NULL;
}

void usrp_low_start(void *_f)
{
  usrp_low *f = (usrp_low *)_f;

  if (pthread_create(&f->t, NULL, rx, f)) { error("pthread_create failed\n"); exit(1); }
}

void usrp_low_stop(void *_f)
{
  usrp_low *f = (usrp_low *)_f;
  void *ret;
  f->stop = 1;
  if (pthread_join(f->t, &ret)) { error("pthread_join failed\n"); exit(1); }
}

void usrp_low_free(void *_f)
{
}

} // extern "C"
