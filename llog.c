#include "llog.h"
#include <string.h>

static int rotate_file(struct llog_log_file* log_file);
static void write_to_stderr(struct llog_log_event *event);
static void write_to_files(struct llog_log_event *event);

static struct {
  struct llog_log_file *files[LLOG_FILES_LENGTH];
  size_t files_count;
  bool use_utc;
  enum log_level min_log_level;
} llog = { {0}, 0, 1, TRACE };

static const char *log_level_strings[] = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL" };

#ifdef LLOG_USE_COLOR
static const char *log_level_color_codes[] = {"\033[36m", "\033[34m", "\033[32m", "\033[33m", "\033[31m", "\033[31m"};
#endif

void set_use_utc_time(bool use_utc) {
  llog.use_utc = use_utc;
}

void set_minimum_log_level(enum log_level log_level) {
  llog.min_log_level = log_level;
}

void create_rotation_policy(struct llog_rotation_policy *llog_rotation_policy, size_t max_size_in_bytes) {
  if (llog_rotation_policy == NULL) {
    LOG_ERROR("Could not create rotation policy with max size = %d due to llog_rotation_policy being a null pointer", max_size_in_bytes);
    return;
  }

  if (max_size_in_bytes == 0) {
    // TODO I don't like this requiring VA_ARGS. Can't call LOG_ERROR("simple string")
    LOG_ERROR("%s", "max_size_in_mb is 0");
    return;
  }

  llog_rotation_policy->rotation_type |= SIZE;
  llog_rotation_policy->max_size_in_bytes = max_size_in_bytes;
  llog_rotation_policy->suffix = 1;
}

// note that setting a llog_rotation_policy on log_file is cleared before here. llog_rotation_policy must be passed here
// for validation before it is added to the log_file
void add_log_file(const char *name, struct llog_log_file *log_file, struct llog_rotation_policy *llog_rotation_policy) {
  int to_add_idx = 0;
  // TODO handle these better than just returning if either are null.
  if (name == NULL || log_file == NULL) {
    return;
  }

  if (llog.files_count == LLOG_FILES_LENGTH) {
    LOG_ERROR("Couldn't add dest log file to llog.files due to llog.files at LLOG_FILE_LENGTH (%d)", LLOG_FILES_LENGTH);
    return;
  }

  while(to_add_idx < LLOG_FILES_LENGTH && llog.files[to_add_idx] != 0) {
    ++to_add_idx;
  }

  log_file->name = name;
  log_file->file = fopen(name, "ab");
  if (log_file->file == NULL) {
    LOG_ERROR("Could not open %s as log file", name);
    return;
  }
  log_file->rotation_policy = NULL; // don't allow existing policies without any validation.
  log_file->current_size = 0;
  llog.files[to_add_idx] = log_file;
  llog.files_count++;

  // TODO handle these better than just returning. Should LOG_ERROR so a message at least goes to stderr
  if (llog_rotation_policy == NULL || llog_rotation_policy->rotation_type == NONE) {
    return;
  }
  if ((llog_rotation_policy->rotation_type & SIZE) == SIZE && llog_rotation_policy->max_size_in_bytes == 0) {
    return;
  }
  log_file->rotation_policy = llog_rotation_policy;
}

void remove_log_file(const char *name) {
  LOG_ERROR("remove_log_file not implemented yet so log file with name %s not removed.", name);
}

void close_log_files(void) {
  int file_count = 0;
  while(file_count < LLOG_FILES_LENGTH && llog.files[file_count] != NULL) {
    // TODO check if `llog.files[file_count]->file == NULL` first. shouldnt happen but safety first.
    fclose(llog.files[file_count]->file);
    llog.files[file_count]->file = NULL;
    ++file_count;
  }
}

static int rotate_file(struct llog_log_file* log_file) {
  char target[strlen(log_file->name) + LLOG_ROTATION_POLICY_SUFFIX_LENGTH];
  snprintf(target, sizeof(target), "%s-%d", log_file->name, log_file->rotation_policy->suffix++);

  fclose(log_file->file);
  rename(log_file->name, target);

  log_file->file = fopen(log_file->name, "ab");
  if (log_file->file == NULL) {
    return 1;
  }

  return 0;
}


// unix convention is to write all logs to stderr to avoid interfering with programs general output.
static void write_to_stderr(struct llog_log_event *event) {
  int offset = 0;
  char log_line_buffer[LLOG_LINE_LENGTH] = {0};
  offset += (int) strftime(log_line_buffer, sizeof(log_line_buffer), "%H:%M:%S ", event->time);
  if (offset == 0) {
    // TODO properly handle this error.
    return;
  }
#ifdef LLOG_USE_COLOR
  offset += snprintf(log_line_buffer+offset, sizeof(log_line_buffer) - offset, "%s%-5s\033[0m %s:%d ", log_level_color_codes[event->level], log_level_strings[event->level], event->file, event->line);
#else
  offset += snprintf(log_line_buffer+offset, sizeof(log_line_buffer) - offset, "%-5s %s:%d ", log_level_strings[event->level], event->file, event->line);
#endif
  if (offset < 0) {
    // TODO properly handle this error.
    return;
  }
  if (offset >= LLOG_LINE_LENGTH) {
    // TODO properly handle this error, although it should only be possible if log_line_buffer size gets set very small or filename is near linux filename max length and near line number 1M+ of a file.
    return;
  }
  offset += vsnprintf(log_line_buffer+offset, sizeof(log_line_buffer) - offset, event->format, event->args);
  if (offset < 0) {
    // TODO properly handle this error.
    return;
  }
  if (offset >= LLOG_LINE_LENGTH) {
    // TODO properly handle this error. This means that the log message was trunctated!
    return;
  }

  fprintf(stderr, "%s\n", log_line_buffer);
  fflush(stderr);
}

static void write_to_files(struct llog_log_event *event) {
  int offset = 0;
  size_t iter = 0;
  char log_line_buffer[LLOG_LINE_LENGTH] = {0};

  offset += (int) strftime(log_line_buffer, sizeof(log_line_buffer), "%Y-%m-%d %H:%M:%S ", event->time);
  if (offset == 0) {
    // TODO properly handle this error, although this hsould be impossible if log_line_buffer size gets set very small.
    return;
  }
  offset += snprintf(log_line_buffer+offset, sizeof(log_line_buffer) - offset, "%-5s %s:%d ", log_level_strings[event->level], event->file, event->line);
  if (offset < 0) {
    // TODO properly handle this error.
    return;
  }
  if (offset >= LLOG_LINE_LENGTH) {
    // TODO properly handle this error, although it should only be possible if log_line_buffer size gets set very small or filename is near linux filename max length and near line number 1M+ of a file.
    return;
  }
  offset += vsnprintf(log_line_buffer+offset, sizeof(log_line_buffer) - offset, event->format, event->args);
  if (offset < 0) {
    // TODO properly handle this error.
    return;
  }
  if (offset >= LLOG_LINE_LENGTH) {
    // TODO properly handle this error. This means that the log message was trunctated!
    return;
  }

  // write to all files
  for (iter = 0; iter < llog.files_count; ++iter) {
    if (llog.files[iter] == NULL) {
      continue;
    }
    // check if the file needs to rotate and rotate if needed. +1 byte size accounts for \n
    if (llog.files[iter]->rotation_policy != NULL && (offset + 1 + llog.files[iter]->current_size >= llog.files[iter]->rotation_policy->max_size_in_bytes)) {
      rotate_file(llog.files[iter]);
    }

    fprintf(llog.files[iter]->file, "%s\n", log_line_buffer);
    fflush(llog.files[iter]->file);
    llog.files[iter]->current_size += offset+1;
  }
}

void llog_log(enum log_level log_level, const char* file, int line, const char *format, ...) {
  time_t now;
  struct llog_log_event event = { .level = log_level, .file = file, .line = line, .format = format };

  // immediately return if a filtering is set above this log_level
  if(llog.min_log_level > log_level) {
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

  if (llog.files_count > 0) {
    va_start(event.args, format);
    write_to_files(&event); 
    va_end(event.args);
  }

  // TODO unlock here
}
