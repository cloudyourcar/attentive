# Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
# This program is free software. It comes without any warranty, to the extent
# permitted by applicable law. You can redistribute it and/or modify it under
# the terms of the Do What The Fuck You Want To Public License, Version 2, as
# published by Sam Hocevar. See the COPYING file for more details.

CFLAGS = $(shell pkg-config --cflags $(LIBRARIES)) -std=c99 -g -Wall -Wextra -Werror -Iinclude
LDLIBS = $(shell pkg-config --libs $(LIBRARIES))

LIBRARIES = check glib-2.0

all: test example
	@echo "+++ All good."""

test: tests/test-parser
	@echo "+++ Running parser test suite."
	tests/test-parser

clean:
	$(RM) tests example *.o
	$(RM) -r *.dSYM/

PARSER = include/attentive/parser.h
AT = include/attentive/at.h include/attentive/at-unix.h $(PARSER)
CELLULAR = include/attentive/cellular.h $(AT)
MODEM = src/modem/common.h $(CELLULAR)

src/parser.o: src/parser.c $(PARSER)
src/at-unix.o: src/at-unix.c $(AT)
src/cellular.o: src/cellular.c $(CELLULAR)
src/modem/common.o: src/modem/common.c $(MODEM)
src/modem/generic.o: src/modem/generic.c $(MODEM)
src/modem/sim800.o: src/modem/sim800.c $(MODEM)
src/modem/telit2.o: src/modem/telit2.c $(MODEM)
tests/test-parser.o: tests/test-parser.c $(MODEM)
src/example-at.o: src/example-at.c $(AT)
src/example-sim800.o: src/example-sim800.c $(CELLULAR)

tests/test-parser: tests/test-parser.o src/parser.o

src/example-at: src/example-at.o src/parser.o src/at-unix.o
src/example-sim800: src/example-sim800.o src/modem/sim800.o src/modem/common.o src/cellular.o src/at-unix.o src/parser.o

.PHONY: all test clean
