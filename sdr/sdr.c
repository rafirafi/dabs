#include "sdr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rtlsdr.h"
#include "usrp.h"
#include "utils/log.h"

io *new_sdr(char *type)
{
#ifdef HAVE_UHD_SDR
  if (!strcmp(type, "usrp"))
    return new_usrp();
#endif
#ifdef HAVE_RTL_SDR
  if (!strcmp(type, "rtlsdr"))
    return new_rtlsdr();
#endif
  error("unknown SDR %s (maybe it was disabled at compile time?)\n", type);
  exit(1);
}

