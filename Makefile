# Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
# This program is free software. It comes without any warranty, to the extent
# permitted by applicable law. You can redistribute it and/or modify it under
# the terms of the Do What The Fuck You Want To Public License, Version 2, as
# published by Sam Hocevar. See the COPYING file for more details.

CFLAGS = $(shell pkg-config --cflags $(LIBRARIES)) -std=c99 -O2 -g -Wall -Wextra -Werror -Iinclude
LDLIBS = $(shell pkg-config --libs $(LIBRARIES))

LIBRARIES = check glib-2.0

all: test example
	@echo "+++ All good."""

test: src/tests
	@echo "+++ Running Check test suite..."
	src/tests

clean:
	$(RM) tests example *.o
	$(RM) -r *.dSYM/

src/parser.o: src/parser.c include/attentive/parser.h
src/at-unix.o: src/at-unix.c include/attentive/at.h include/attentive/parser.h
src/modem/sim800.o: src/modem/sim800.c include/attentive/cellular.h include/attentive/at.h include/attentive/parser.h
src/modem/telit2.o: src/modem/telit2.c include/attentive/cellular.h include/attentive/at.h include/attentive/parser.h
src/tests.o: src/tests.c include/attentive/parser.h
src/example.o: src/example.c include/attentive/at.h include/attentive/parser.h

src/tests: src/tests.o src/parser.o
src/example: src/example.o src/parser.o src/at-unix.o src/sim800.o

.PHONY: all test clean
