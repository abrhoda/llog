#include "../llog.h"
#include <stdlib.h>
#include <time.h>


int generate_in_range(int lower, int upper);
int add_numbers(int first, int second);

int generate_in_range(int lower, int upper) {
  return (rand() % (upper - lower + 1)) + lower;
}

int add_numbers(int first, int second) {
  return first + second;
}

// NOTE: ALL ERROR CHECKING AND HANDLING IS SKIPPED FOR BREVITY IN THIS EXAMPLE

int main(void) {
  const int upper = 100;
  const int lower = 1;
  int first = 0;
  int second = 0;
  int res = 0;
  int iter = 0;
  int max_iters = 10;
  const char *fmt = "iteration %d: %d + %d = %d";
  const char *log_file_name = "example/logs/example.log";
  struct llog_log_file log_file;
  int max_size = 200;
  struct llog_rotation_policy rotation_policy;

  srand(time(NULL));

  first = generate_in_range(lower, upper);
  second = generate_in_range(lower, upper);
  res = add_numbers(first, second);

  // setting llog to use UTC timestamps instead of local time.
  set_use_utc_time(1);

  create_rotation_policy(&rotation_policy, max_size);
  // setting a log file which will be written to in addition to stdout/stderr
  add_log_file(log_file_name, &log_file, &rotation_policy);

  for (iter = 0; iter < max_iters; ++iter) {
    LOG_FATAL(fmt, iter, first, second, res);
    LOG_INFO(fmt, iter, first, second, res);
  }

  // manually closing the log files at the end
  close_log_files();
  return 0;
}
