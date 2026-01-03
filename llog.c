#include "llog.h"
#include <string.h>

static struct {
  struct llog_log_file *files[LLOG_FILES_LENGTH];
  bool use_utc;
  enum log_level log_level;
} llog = { {0}, 1, TRACE };

static const char *log_level_strings[] = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL" };

#ifdef LLOG_USE_COLOR
static const char *log_level_color_codes[] = {"\033[36m", "\033[34m", "\033[32m", "\033[33m", "\033[31m", "\033[31m"};
#endif

void set_use_utc_time(bool use_utc) {
  llog.use_utc = use_utc;
}

void set_minimum_log_level(enum log_level log_level) {
  llog.log_level = log_level;
}

void create_rotation_policy(struct llog_rotation_policy *llog_rotation_policy, int max_size_in_mb) {
  if (llog_rotation_policy == NULL) {
    LOG_ERROR("Could not create rotation policy with max size = %d due to llog_rotation_policy being a null pointer", max_size_in_mb);
    return;
  }

  if (max_size_in_mb < 1) {
    // TODO I don't like this requiring VA_ARGS. Can't call LOG_ERROR("simple string")
    LOG_ERROR("%s", "max_size_in_mb is 0");
    return;
  }

  if (max_size_in_mb > 0) {
    llog_rotation_policy->rotation_type |= SIZE;
    llog_rotation_policy->max_size_in_mb = max_size_in_mb;
  }
  llog_rotation_policy->suffix = 1;
}

void add_log_file(const char *name, struct llog_log_file *log_file, struct llog_rotation_policy *llog_rotation_policy) {
  int file_count = 0;
  // TODO handle these better than just returning if either are null.
  if (name == NULL || log_file == NULL) {
    return;
  }

  while(file_count < LLOG_FILES_LENGTH && llog.files[file_count] != 0) {
    ++file_count;
  }

  if (file_count == LLOG_FILES_LENGTH) {
    LOG_ERROR("Couldn't add dest log file to llog.files due to llog.files at LLOG_FILE_LENGTH (%d)", LLOG_FILES_LENGTH);
    return;
  }

  log_file->name = name;
  log_file->file = fopen(name, "ab");
  if (log_file->file == NULL) {
    LOG_ERROR("Could not open %s as log file", name);
    return;
  }
  llog.files[file_count] = log_file;

  // TODO handle these better than just returning. Should LOG_ERROR so a message at least goes to stderr
  if (llog_rotation_policy == NULL || llog_rotation_policy->rotation_type == NONE) {
    return;
  }

  if ((llog_rotation_policy->rotation_type & SIZE) == SIZE && llog_rotation_policy->max_size_in_mb == 0) {
    return;
  }
  log_file->rotation_policy = llog_rotation_policy;
}

void close_log_files(void) {
  int file_count = 0;
  while(file_count < LLOG_FILES_LENGTH && llog.files[file_count] != 0) {
    fclose(llog.files[file_count]->file);
    llog.files[file_count]->file = NULL;
    ++file_count;
  }
}

// unix convention is to write all logs to stderr to avoid interfering with programs general output.
static void write_to_stderr(struct llog_log_event *event) {
  size_t cursor = 0;
  char log_stmt_buffer[LLOG_STMT_LENGTH] = {0};
  cursor += strftime(log_stmt_buffer, sizeof(log_stmt_buffer), "%H:%M:%S ", event->time);
#ifdef LLOG_USE_COLOR
  cursor += sprintf((log_stmt_buffer+cursor), "%s%-5s\033[0m %s:%d ", log_level_color_codes[event->level], log_level_strings[event->level], event->file, event->line);
#else
  cursor += sprintf((log_stmt_buffer+cursor), "%-5s %s:%d ", log_level_strings[event->level], event->file, event->line);
#endif

  fprintf(stderr, "%s", log_stmt_buffer);
  vfprintf(stderr, event->format, event->args);
  fprintf(stderr, "\n");
  fflush(stderr);
}

static void write_to_file(struct llog_log_event *event, FILE *file) {
  size_t cursor = 0;
  char log_stmt_buffer[LLOG_STMT_LENGTH];
  cursor += strftime(log_stmt_buffer, sizeof(log_stmt_buffer), "%Y-%m-%d %H:%M:%S ", event->time);
  cursor += sprintf((log_stmt_buffer+cursor), "%-5s %s:%d ", log_level_strings[event->level], event->file, event->line);

  // TODO this is 2 writes to the file. Could add event->format to the end of the log_stmt_buffer string and then use vfprintf(file, log_stmt_buffer,, event->args)
  // However, writing to the file isnt actually done until the `fflush` call so 3 writes to the stream isn't a big deal.
  fprintf(file, "%s", log_stmt_buffer);
  vfprintf(file, event->format, event->args);
  fprintf(file, "\n");
  fflush(file);
}

//static int rotate_file(struct llog_log_file* log_file) {
//  printf("check and rotate file if needed: %s\n", log_file->name);
  /* TODO rotate steps
   *  1. add current_size (size_t or int) and suffix (just int for now) to struct llog_log_file
   *  2. whenever any count > 0 of bytes are wrriten to a file, add to the current_size of that file
   *  3. for each file, check if there's a policy on it. if no, skip rotation step for that file
   *  4. check the policy' size limit and if its less than current_size + size_to_write, just write bytes to it
   *  5. if current_size + size_to_write > limit, rotate, then write bytes to it.
   *
   *  rotation steps:
   *  1. close current FILE * in llog_log_file->file
   *  2. rename to filename.log-<suffix>
   *  3. increment suffix on llog_log_file
   *  4. open a new filename.log file in `append + binary` ("ab") mode
   *  5. put new filename.log file's FILE * into llog_log_file->file field.
   */
//  return 0;
//}

void llog_log(enum log_level log_level, const char* file, int line, const char *format, ...) {
  int count = 0;
  time_t now;
  struct llog_log_event event = { .level = log_level, .file = file, .line = line, .format = format };
  struct llog_log_file *llog_log_file = NULL;

  // immediately return if a filtering is set above this log_level
  if(llog.log_level > log_level) {
    return;
  }

  // TODO lock here
  now = time(NULL);
  if (llog.use_utc) {
    event.time = gmtime(&now);
  } else {
    event.time = localtime(&now);
  }

  // need to reparse varargs each `write_to_...` fn because vprintf and family can invalidate the va_list
  va_start(event.args, format);
  write_to_stderr(&event);
  va_end(event.args);

  while (count < LLOG_FILES_LENGTH && llog.files[count] != 0) {
    llog_log_file = llog.files[count];
    va_start(event.args, format);
    write_to_file(&event, llog_log_file->file); 
    va_end(event.args);
    ++count;
  }
  // TODO unlock here
}
