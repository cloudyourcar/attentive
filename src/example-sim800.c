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

#include <attentive/at-unix.h>
#include <attentive/cellular.h>

int main(int argc, char *argv[])
{
    assert(argc-1 == 1);
    const char *devpath = argv[1];

    struct at *at = at_alloc_unix(devpath, B115200);
    struct cellular *modem = cellular_sim800_alloc(at);

    assert(at_open(at) == 0);

    at_command(at, "AT");
    at_command(at, "ATE0");
    at_command(at, "AT+CGMR");
    at_command(at, "AT+CGSN");

    printf("* getting modem time\n");
    struct timespec ts;
    if (modem->ops->clock->gettime(modem, &ts) == 0) {
        printf("gettime: %s", ctime(&ts.tv_sec));
    } else {
        perror("gettime");
    }

    printf("* setting modem time\n");
    if (modem->ops->clock->settime(modem, &ts) != 0) {
        perror("settime");
    }

    char imei[CELLULAR_IMEI_LENGTH+1];
    modem->ops->device->imei(modem, imei, sizeof(imei));
    printf("imei: %s\n", imei);

    assert(at_close(at) == 0);

    cellular_sim800_free(modem);
    at_free(at);

    return 0;
}

/* vim: set ts=4 sw=4 et: */
