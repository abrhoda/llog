#ifndef LLOG_H
#define LLOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

typedef void (*lock_fn)(bool);

enum {
  LLOG_LINE_LENGTH = 512,
  LLOG_FILES_LENGTH = 8,
  LLOG_ROTATION_POLICY_SUFFIX_LENGTH = 11
};

enum ERROR_CODE {
  EC_NONE = 0,
  EC_INVALID_PARAM = 1,
  EC_MAX_FILES = 2,
  EC_FILE_NOT_FOUND = 3,
  EC_CANNOT_OPEN_FILE = 4,
  EC_CANNOT_CLOSE_FILE = 5,
  EC_CANNOT_RENAME_FILE = 6,
  EC_SIZE_OP = 7,
  EC_BUFFER_OVERFLOW = 8,
  EC_FORMAT = 9,
  EC_STREAM_FLUSH = 11,
  EC_OUTSTREAM = 12,  // unable to write to os.
};

enum log_level {
  TRACE = 0,
  DEBUG = 1,
  INFO = 2,
  WARN = 3,
  ERROR = 4,
  FATAL = 5
};

enum llog_rotation_type { NONE = 0, SIZE = (1 << 1) };

struct llog_log_event {
  enum log_level level;
  struct tm* time;
  const char* file;
  int line;
  const char* format;
  va_list args;
};

struct llog_rotation_policy {
  enum llog_rotation_type rotation_type;
  size_t max_size_in_bytes;
  int suffix;
};

struct llog_log_file {
  const char* name;
  FILE* file;
  size_t current_size;
  struct llog_rotation_policy* rotation_policy;
};

#define LOG_TRACE(fmt, ...) \
  llog_log(TRACE, __FILE__, __LINE__, (fmt), __VA_ARGS__)
#define LOG_DEBUG(fmt, ...) \
  llog_log(DEBUG, __FILE__, __LINE__, (fmt), __VA_ARGS__)
#define LOG_INFO(fmt, ...) \
  llog_log(INFO, __FILE__, __LINE__, (fmt), __VA_ARGS__)
#define LOG_WARN(fmt, ...) \
  llog_log(WARN, __FILE__, __LINE__, (fmt), __VA_ARGS__)
#define LOG_ERROR(fmt, ...) \
  llog_log(ERROR, __FILE__, __LINE__, (fmt), __VA_ARGS__)
#define LOG_FATAL(fmt, ...) \
  llog_log(FATAL, __FILE__, __LINE__, (fmt), __VA_ARGS__)

void llog_log(enum log_level log_level, const char* file, int line,
              const char* format, ...);
int create_rotation_policy(struct llog_rotation_policy* llog_rotation_policy,
                           size_t max_size_in_bytes);
int add_log_file(const char* name, struct llog_log_file* llog_log_file,
                 struct llog_rotation_policy* llog_rotation_policy);
int remove_log_file(const char* name);
int close_log_files(void);
void set_use_utc_time(bool use_utc);
void set_minimum_log_level(enum log_level log_level);
#endif
