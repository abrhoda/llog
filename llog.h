#ifndef LLOG_H
#define LLOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

enum {
  LLOG_LINE_LENGTH = 512,
  LLOG_FILES_LENGTH = 8,
  LLOG_ROTATION_POLICY_SUFFIX_LENGTH = 11
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
void create_rotation_policy(struct llog_rotation_policy* llog_rotation_policy,
                            size_t max_size_in_bytes);
void add_log_file(const char* name, struct llog_log_file* llog_log_file,
                  struct llog_rotation_policy* llog_rotation_policy);
void remove_log_file(const char* name);
void close_log_files(void);
void set_use_utc_time(bool use_utc);
void set_minimum_log_level(enum log_level log_level);
#endif
