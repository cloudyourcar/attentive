/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <attentive/at.h>
#include <attentive/at-unix.h>

static void generic_modem_handle_urc(const void *line, size_t len, void *arg)
{
    printf("[%p] URC: %.*s\n", arg, (int) len, (char *) line);
}

static const struct at_callbacks generic_modem_callbacks = {
    .handle_urc = generic_modem_handle_urc,
};

int main(int argc, char *argv[])
{
    assert(argc-1 == 1);
    const char *devpath = argv[1];

    printf("allocating channel...\n");
    struct at *at = at_alloc_unix(devpath, B115200);

    printf("opening port...\n");
    assert(at_open(at) == 0);

    printf("attaching callbacks\n");
    at_set_callbacks(at, &generic_modem_callbacks, NULL);

    const char *commands[] = {
        "AT",
        "ATE0",
        "AT+CGSN",
        "AT+CCID",
        "AT+CGN",
        "AT+CMEE=0",
        "AT+BLAH",
        "AT+CMEE=2",
        "AT+BLAH",
        NULL
    };

    printf("sending commands...\n");
    at_set_timeout(at, 10);
    for (const char **command=commands; *command; command++) {
        const char *result = at_command(at, *command);
        printf("%s => %s\n", *command, result ? result : strerror(errno));
    }

    printf("freeing resources...\n");
    at_free(at);

    return 0;
}

/* vim: set ts=4 sw=4 et: */
