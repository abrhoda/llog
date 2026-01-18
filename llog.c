#include "llog.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int rotate_file(struct llog_log_file* log_file);
static int write_to_stderr(struct llog_log_event* event);
static int write_to_files(struct llog_log_event* event);

static struct {
  struct llog_log_file* files[LLOG_FILES_LENGTH];
  size_t files_count;
  bool use_utc;
  enum log_level min_log_level;
  lock_fn lock_fn;
} llog = {{0}, 0, 1, TRACE, NULL};

static const char* log_level_strings[] = {"TRACE", "DEBUG", "INFO",
                                          "WARN",  "ERROR", "FATAL"};

#ifdef LLOG_USE_COLOR
static const char* log_level_color_codes[] = {
    "\033[36m", "\033[34m", "\033[32m", "\033[33m", "\033[31m", "\033[31m"};
#endif

void set_use_utc_time(bool use_utc) { llog.use_utc = use_utc; }

void set_minimum_log_level(enum log_level log_level) {
  llog.min_log_level = log_level;
}

int create_rotation_policy(struct llog_rotation_policy* llog_rotation_policy,
                            size_t max_size_in_bytes) {
  if (llog_rotation_policy == NULL) {
    LOG_ERROR(
        "Could not create rotation policy with max size = %d due to "
        "llog_rotation_policy being a null pointer",
        max_size_in_bytes);
    return EC_INVALID_PARAM;
  }

  if (max_size_in_bytes == 0) {
    // TODO I don't like this requiring VA_ARGS. Can't call LOG_ERROR("simple
    // string")
    LOG_ERROR("%s", "max_size_in_mb is 0");
    return EC_INVALID_PARAM;
  }

  llog_rotation_policy->rotation_type |= SIZE;
  llog_rotation_policy->max_size_in_bytes = max_size_in_bytes;
  llog_rotation_policy->suffix = 1;
  return EC_NONE;
}

// note that setting a llog_rotation_policy on log_file is cleared before here.
// llog_rotation_policy must be passed here for validation before it is added to
// the log_file
int add_log_file(const char* name, struct llog_log_file* log_file,
                  struct llog_rotation_policy* llog_rotation_policy) {
  int to_add_idx = 0;
  if (name == NULL || log_file == NULL) {
    LOG_ERROR("%s", "name or log_file where null.");
    return EC_INVALID_PARAM;
  }

  if (llog.files_count == LLOG_FILES_LENGTH) {
    LOG_ERROR(
        "Couldn't add dest log file to llog.files due to llog.files at "
        "LLOG_FILE_LENGTH (%d)",
        LLOG_FILES_LENGTH);
    return EC_MAX_FILES;
  }

  while (to_add_idx < LLOG_FILES_LENGTH && llog.files[to_add_idx] != 0) {
    ++to_add_idx;
  }

  log_file->name = name;
  log_file->file = fopen(name, "ab");
  if (log_file->file == NULL) {
    LOG_ERROR("Could not open %s as log file", name);
    return EC_CANNOT_OPEN_FILE;
  }
  // don't allow existing policies without any validation.
  log_file->rotation_policy = NULL;
  log_file->current_size = 0;
  llog.files[to_add_idx] = log_file;
  llog.files_count++;

  // TODO handle these better than just returning. Should LOG_ERROR so a message
  // at least goes to stderr
  if (llog_rotation_policy == NULL ||
      llog_rotation_policy->rotation_type == NONE) {
    return EC_NONE;
  }
  if ((llog_rotation_policy->rotation_type & SIZE) == SIZE &&
      llog_rotation_policy->max_size_in_bytes == 0) {
    return EC_INVALID_PARAM;
  }
  llog_rotation_policy->suffix = 1;
  log_file->rotation_policy = llog_rotation_policy;
  return EC_NONE;
}

void remove_log_file(const char* name) {
  LOG_ERROR(
      "remove_log_file not implemented yet so log file with name %s not "
      "removed.",
      name);
}

int close_log_files(void) {
  int file_count = 0;
  while (file_count < LLOG_FILES_LENGTH && llog.files[file_count] != NULL && llog.files[file_count]->file != NULL) {
    if (fclose(llog.files[file_count]->file) != 0) {
      return EC_CANNOT_CLOSE_FILE;
    }
    llog.files[file_count]->file = NULL;
    ++file_count;
  }
  return EC_NONE;
}

// NOTE: this could possible fail "mid rotation"
static int rotate_file(struct llog_log_file* log_file) {
  int written = 0;
  size_t max_length = strlen(log_file->name) + LLOG_ROTATION_POLICY_SUFFIX_LENGTH;
  char target[max_length];
  written = snprintf(target, sizeof(target), "%s-%d", log_file->name,
           log_file->rotation_policy->suffix++);
  if (written < 0) {
    return EC_FORMAT;
  }
  if ((size_t) written > max_length) {
    return EC_BUFFER_OVERFLOW;
  }

  if (fclose(log_file->file) != 0) {
    return EC_CANNOT_CLOSE_FILE;
  }

  if (rename(log_file->name, target) != 0) {
    return EC_CANNOT_RENAME_FILE;
  }

  log_file->file = fopen(log_file->name, "ab");
  if (log_file->file == NULL) {
    return EC_CANNOT_OPEN_FILE;
  }

  log_file->current_size = 0;
  return EC_NONE;
}

// unix convention is to write all logs to stderr to avoid interfering with
// programs general output.
static int write_to_stderr(struct llog_log_event* event) {
  int offset = 0;
  char log_line_buffer[LLOG_LINE_LENGTH] = {0};
  offset += (int)strftime(log_line_buffer, sizeof(log_line_buffer), "%H:%M:%S ",
                          event->time);
  if (offset == 0) {
    return EC_FORMAT;
  }
#ifdef LLOG_USE_COLOR
  offset +=
      snprintf(log_line_buffer + offset, sizeof(log_line_buffer) - offset,
               "%s%-5s\033[0m %s:%d ", log_level_color_codes[event->level],
               log_level_strings[event->level], event->file, event->line);
#else
  offset += snprintf(log_line_buffer + offset, sizeof(log_line_buffer) - offset,
                     "%-5s %s:%d ", log_level_strings[event->level],
                     event->file, event->line);
#endif
  if (offset < 0) {
    return EC_FORMAT;
  }
  if (offset >= LLOG_LINE_LENGTH) {
    return EC_BUFFER_OVERFLOW;
  }
  offset +=
      vsnprintf(log_line_buffer + offset, sizeof(log_line_buffer) - offset,
                event->format, event->args);
  if (offset < 0) {
    return EC_FORMAT;
  }
  if (offset >= LLOG_LINE_LENGTH) {
    return EC_BUFFER_OVERFLOW;
  }

  if (fprintf(stderr, "%s\n", log_line_buffer) < 0) {
    return EC_OUTSTREAM;
  }
  if (fflush(stderr) != 0) {
    return EC_STREAM_FLUSH;
  }

  return EC_NONE;
}

static int write_to_files(struct llog_log_event* event) {
  int offset = 0;
  size_t iter = 0;
  int rotation_result = 0;
  char log_line_buffer[LLOG_LINE_LENGTH] = {0};

  offset += (int)strftime(log_line_buffer, sizeof(log_line_buffer),
                          "%Y-%m-%d %H:%M:%S ", event->time);
  if (offset == 0) {
    return EC_FORMAT;
  }
  offset += snprintf(log_line_buffer + offset, sizeof(log_line_buffer) - offset,
                     "%-5s %s:%d ", log_level_strings[event->level],
                     event->file, event->line);
  if (offset < 0) {
    return EC_FORMAT;
  }
  if (offset >= LLOG_LINE_LENGTH) {
    return EC_BUFFER_OVERFLOW;
  }
  offset +=
      vsnprintf(log_line_buffer + offset, sizeof(log_line_buffer) - offset,
                event->format, event->args);
  if (offset < 0) {
    return EC_FORMAT;
  }
  if (offset >= LLOG_LINE_LENGTH) {
    return EC_BUFFER_OVERFLOW;
  }

  // write to all files
  for (iter = 0; iter < llog.files_count; ++iter) {
    if (llog.files[iter] == NULL) {
      continue;
    }
    // check if the file needs to rotate and rotate if needed.
    // +1 byte size accounts for \n
    if (llog.files[iter]->rotation_policy != NULL &&
        (offset + 1 + llog.files[iter]->current_size >=
         llog.files[iter]->rotation_policy->max_size_in_bytes)) {
      printf("rotating file. current size = %lu, offset = %d, max size: %lu\n",
          llog.files[iter]->current_size, (offset+1), llog.files[iter]->rotation_policy->max_size_in_bytes);
      rotation_result = rotate_file(llog.files[iter]);
      if (rotation_result != EC_NONE) {
        return rotation_result;
      }
    }

    if (fprintf(llog.files[iter]->file, "%s\n", log_line_buffer) < 0) {
      return EC_OUTSTREAM;
    }
    if (fflush(llog.files[iter]->file) != 0) {
      return EC_STREAM_FLUSH;
    }
    llog.files[iter]->current_size += offset + 1;
  }
  return EC_NONE;
}

void llog_log(enum log_level log_level, const char* file, int line,
              const char* format, ...) {
  time_t now;
  struct llog_log_event event = {
      .level = log_level, .file = file, .line = line, .format = format};

  if (llog.min_log_level > log_level) {
    return;
  }

  if (llog.lock_fn != NULL) {
    llog.lock_fn(true);
  }
  now = time(NULL);
  if (llog.use_utc) {
    event.time = gmtime(&now);
  } else {
    event.time = localtime(&now);
  }

  va_start(event.args, format);
  write_to_stderr(&event);
  va_end(event.args);

  if (llog.files_count > 0) {
    va_start(event.args, format);
    write_to_files(&event);
    va_end(event.args);
  }

  if (llog.lock_fn != NULL) {
    llog.lock_fn(false);
  }
}
