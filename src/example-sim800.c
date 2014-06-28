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
#include <unistd.h>

#include <attentive/at-unix.h>
#include <attentive/cellular.h>


int main(int argc, char *argv[])
{
    assert(argc-1 == 2);
    const char *devpath = argv[1];
    const char *apn = argv[2];

    struct at *at = at_alloc_unix(devpath, B115200);
    struct cellular *modem = cellular_sim800_alloc();

    assert(at_open(at) == 0);
    assert(cellular_attach(modem, at, apn) == 0);

    printf("* getting network status\n");
    int creg, rssi;
    if ((creg = modem->ops->creg(modem)) != -1) {
        printf("registration status: %d\n", creg);
    } else {
        perror("creg");
    }

    if ((rssi = modem->ops->rssi(modem)) != -1) {
        printf("signal strength: %d\n", rssi);
    } else {
        perror("rssi");
    }

    printf("* getting modem time\n");
    struct timespec ts;
    if (modem->ops->clock_gettime(modem, &ts) == 0) {
        printf("gettime: %s", ctime(&ts.tv_sec));
    } else {
        perror("gettime");
    }

    printf("* setting modem time\n");
    if (modem->ops->clock_settime(modem, &ts) != 0) {
        perror("settime");
    }

    char imei[CELLULAR_IMEI_LENGTH+1];
    modem->ops->imei(modem, imei, sizeof(imei));
    printf("imei: %s\n", imei);

    /* network stuff. */
    int socket = 2;

    if (modem->ops->socket_connect(modem, socket, "google.com", 80) == 0) {
        printf("connect successful\n");
    } else {
        perror("connect");
    }

    const char *request = "GET / HTTP/1.0\r\n\r\n";
    if (modem->ops->socket_send(modem, socket, request, strlen(request), 0) == (int) strlen(request)) {
        printf("send successful\n");
    } else {
        perror("send");
    }

    int len;
    char buf[32];
    while ((len = modem->ops->socket_recv(modem, socket, buf, sizeof(buf), 0)) >= 0) {
        if (len > 0)
            printf("Received: >\x1b[0;1;33m%.*s\x1b[0m<\n", len, buf);
        else
            sleep(1);
    }

    if (modem->ops->socket_close(modem, socket) == 0) {
        printf("close successful\n");
    } else {
        perror("close");
    }

    assert(cellular_detach(modem) == 0);
    assert(at_close(at) == 0);

    cellular_sim800_free(modem);
    at_free(at);

    return 0;
}

/* vim: set ts=4 sw=4 et: */
