NAME := llog
BIN_DIR := bin
DEBUG ?= 0
SANITIZE ?= 0
COLOR ?= 0
ENABLE_COLOR :=
FORMATTER := clang-format
LINTER := clang-tidy

CFLAGS += -std=c99
CFLAGS += -Wpedantic -pedantic-errors -Werror -Wall -Wextra
CFLAGS += -Waggregate-return
CFLAGS += -Wbad-function-cast
CFLAGS += -Wcast-align
CFLAGS += -Wcast-qual
CFLAGS += -Wdeclaration-after-statement
CFLAGS += -Wfloat-equal
CFLAGS += -Wformat=2
#CFLAGS += -Wlogical-op
CFLAGS += -Wmissing-declarations
CFLAGS += -Wmissing-include-dirs
CFLAGS += -Wmissing-prototypes
CFLAGS += -Wnested-externs
CFLAGS += -Wpointer-arith
CFLAGS += -Wredundant-decls
CFLAGS += -Wsequence-point
CFLAGS += -Wshadow
CFLAGS += -Wstrict-prototypes
CFLAGS += -Wswitch
CFLAGS += -Wundef
CFLAGS += -Wunreachable-code
CFLAGS += -Wunused-but-set-parameter
CFLAGS += -Wwrite-strings

ifeq ($(DEBUG), 1)
	CFLAGS := $(CFLAGS) -g -O0
else
	CFLAGS := $(CFLAGS) -O3 -DNDEBUG
endif

ifeq ($(SANITIZE), 1)
	CFLAGS += -fsanitize=address,leak,undefined
endif

ifeq ($(COLOR), 1)
	ENABLE_COLOR = "-DLLOG_USE_COLOR"
endif

$(NAME): example/main.c llog.c llog.h
	mkdir -p $(BIN_DIR)
	$(CC) $(ENABLE_COLOR) $(CFLAGS) -o $(BIN_DIR)/$@ $?

.PHONY: format
format:
	$(FORMATTER) --style=file -i llog.c llog.h

.PHONY: lint
lint:
	$(LINTER) --config-file=.clang-tidy llog.c llog.h -- $(CFLAGS) -Wmost

