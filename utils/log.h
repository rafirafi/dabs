#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>

#define DEBUG 0x01
#define ERROR 0x02
#define WARN  0x04
#define INFO  0x08

extern int log_flags;

#define debug(...) do { if (log_flags & DEBUG) printf(__VA_ARGS__); } while (0)
#define info(...) do { if (log_flags & INFO) printf(__VA_ARGS__); } while (0)
#define error(...) \
  do { \
    if (log_flags & ERROR) { \
      printf("error: "); \
      printf(__VA_ARGS__); \
    } \
  } while (0)
#define Error(...) \
  do { \
    if (log_flags & ERROR) { \
      printf(__VA_ARGS__); \
    } \
  } while (0)
#define warn(...) \
  do { \
    if (log_flags & WARN) { \
      printf("warn: "); \
      printf(__VA_ARGS__); \
    } \
  } while (0)

#endif /* _LOG_H_ */

