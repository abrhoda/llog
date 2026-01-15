1. [ ] decide how to handle errors properly. Error's cannot call the `llog_log(args)` function because if `write_to_file` or `write_to_stderr` fails, this creates a recursive loop that might continually fail. This includes handling warnings about ignoring error handling, introduction of an error enum and returning int instead of void.
2. [ ] time based rotation policy to rotate either at an interval (aka every hour/day) or at a specific datetime. However, because this is written in a single threaded way, the rotation would have to happen _at the time of trying to write to a a log file_ and not actually at the exact interval/datetime. That is because the rotation policy is checked and files are rotated right before a write happens.
3. [ ] better README.md and documentation of the public api, macros that control color + buffer size, etc.
4. [ ] support unicode and control it with `ENABLE_UNICODE` macro.
5. [ ] example + unit test.
6. [ ] application provided lock + unlock functions so that this library can be used in a multithreaded applcation.
