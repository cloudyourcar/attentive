/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#include <assert.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#include "attentive.h"



static enum at_response_type response_parser(const char *line, size_t len)
{
    (void) line;
    (void) len;

    return AT_RESPONSE_UNKNOWN;
}

static void urc_callback(const char *line, size_t len)
{
    (void) len;

    printf("URC: %s\n", line);
}

int main(int argc, char *argv[])
{
    assert(argc == 2);
    const char *port = argv[1];

    sleep(1);

    /* open serial port */
    printf("opening port...\n");
    int fd = open(port, O_RDWR);
    struct termios attr;
    tcgetattr(fd, &attr);
    cfsetspeed(&attr, B115200);
    tcsetattr(fd, TCSANOW, &attr);

    FILE *istream = fdopen(fd, "rb");
    FILE *ostream = fdopen(fd, "wb");
    setvbuf(istream, NULL, _IONBF, 0);
    setvbuf(ostream, NULL, _IONBF, 0);

    /* create AT channel instance */
    printf("starting parser...\n");
    struct at *at = at_open(istream, ostream, response_parser, urc_callback);

    printf("sending commands...\n");
    at_command(at, "AT+BLAH");
    at_command(at, "AT+CGSN");
    at_command(at, "AT+CCID");
    at_command(at, "AT+CGN");
    at_command(at, "AT+CCID");

    printf("closing port...\n");
    at_close(at);

    printf("finished\n");

    return 0;
}

/* vim: set ts=4 sw=4 et: */
