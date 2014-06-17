# Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
# This program is free software. It comes without any warranty, to the extent
# permitted by applicable law. You can redistribute it and/or modify it under
# the terms of the Do What The Fuck You Want To Public License, Version 2, as
# published by Sam Hocevar. See the COPYING file for more details.

CFLAGS = -std=c99 -O2 -g -Wall -Wextra -Werror $(shell pkg-config --cflags $(LIBRARIES))
LDLIBS = $(shell pkg-config --libs $(LIBRARIES))

LIBRARIES = check glib-2.0

all: test example
	@echo "+++ All good."""

test: tests
	@echo "+++ Running Check test suite..."
	./tests

clean:
	$(RM) tests example *.o
	$(RM) -r *.dSYM/

tests: tests.o at_parser.o
example: example.o attentive.o
tests.o: tests.c at_parser.h
attentive.o: attentive.c attentive.h
at_parser.o: at_parser.c at_parser.h

.PHONY: all test clean
