# Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
# This program is free software. It comes without any warranty, to the extent
# permitted by applicable law. You can redistribute it and/or modify it under
# the terms of the Do What The Fuck You Want To Public License, Version 2, as
# published by Sam Hocevar. See the COPYING file for more details.

CFLAGS = -g -Wall -Wextra -Werror -O2 $(shell pkg-config --cflags $(LIBRARIES))
LDFLAGS = $(shell pkg-config --libs $(LIBRARIES))

LIBRARIES = check glib-2.0

all: test example
	@echo "+++ All good."""

test: tests
	@echo "+++ Running Check test suite..."
	./tests

clean:
	$(RM) tests *.o

tests: tests.o attentive.o
example: example.o attentive.o
tests.o: tests.c attentive.h
attentive.o: attentive.c attentive.h
parser.o: parser.c parser.h

.PHONY: all test scan-build clean
