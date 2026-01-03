## llog
a little log (llog) library for c99 with configurable rotation policies.

## Overview
### Fatures
### API



## Building

### Example Project
Use the simple makefile. It will build `example/main.c`, `llog.c`, `llog.h` and output to `bin/llog`.

Flags for building:
- `debug`: setting `debug=1` sets the `-g -O0` cflags. Defaults to `debug=0` which sets `-O3 -DNDEBUG` cflags.
- `sanitize`: setting `sanitize=1` sets the `-fsanitize=address,leak,undefined` cflags. Defaults to `sanitize=0` which sets no additional cflags.
- `color`: setting `color=1` passes the `-DLLOG_USE_COLOR` preprocessor option to the compiler and will result in color output to stdout/stderr. Defaults to `color=0` which doesn't set the preprocessor flag and results in no color output.

### In An Actual Project
All other configuration is done via the `static struct llog` struct which should not be accessed directly. Use the provided `set_XXX` functions from `llog.h` to set values on the static llog instance. This is done as recommended by the "GNU Coding Standards" on [Conditional Compilation](https://www.gnu.org/prep/standards/html_node/Conditional-Compilation.html). This is do as a clearer, more type safe choice with the trade off being that there is an execution of an `if/else` in some places based on the configuration. If this trade off is not one you want to make, feel free to modify the code to rely more heavily on macros instead.

I am of the opinion that conditiional compilation with macros should be reserved for cases such as handling different versions of a program (like a `test` vs `prod` build where `test` might have more heavy logging), handling different hardware/os in the same codebase (like the `-DLLOG_USE_COLOR` macro used to compile with or without color output for systems that don't support color output), or handling compiler specific behavior. I tend to avoid them otherwise without good reason.

In some cases, macros and conditional compilation need to be done though. So, what is a rule without an exception?:
`-DLLOG_USE_COLOR` is the only configurable macro that is used when compiling. This defaults to off to account for systems that don't support colorized output. Pretty colors are an optional feature. 
