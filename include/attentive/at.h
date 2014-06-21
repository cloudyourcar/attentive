/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#ifndef ATTENTIVE_AT_H
#define ATTENTIVE_AT_H

#include <attentive/parser.h>

struct at {
    struct at_parser *parser;
};

struct at_port {
    const struct at_port_operations *ops;
};

/**
 * Create an AT channel instance.
 *
 * NOTE: In most cases, you should be using the platform-specific at_alloc_*()
 *       function instead.
 *
 * @param port AT port instance.
 * @param parser AT parser instance.
 * @returns Instance pointer on success, NULL and sets errno on failure.
 */
struct at *at_alloc(struct at_parser *parser);

/**
 * Open the AT channel.
 *
 * @param at AT channel instance.
 * @returns Zero on success, -1 and sets errno on failure.
 */
int at_open(struct at *at);

/**
 * Close the AT channel.
 *
 * @param at AT channel instance.
 * @returns Zero on success, -1 and sets errno on failure.
 */
int at_close(struct at *at);

/**
 * Close and free an AT channel instance.
 *
 * @param at AT channel instance.
 */
void at_free(struct at *at);

/**
 * Set command timeout in seconds.
 *
 * @param at AT channel instance.
 * @param timeout Timeout in seconds.
 */
void at_set_timeout(struct at *at, int timeout);

/**
 * Send an AT command and receive a response. Accepts printf-compatible
 * format and arguments.
 *
 * @param at AT channel instance.
 * @param format printf-comaptible format.
 * @returns Pointer to response (valid until next at_command) or NULL
 *          if a timeout occurs. Response is newline-delimited and does
 *          not include the final "OK".
 */
const char *at_command(struct at *at, const char *format, ...);

/**
 * Send raw data over the AT channel.
 *
 * @param at AT channel instance.
 * @param data Raw data to send.
 * @param size Data size in bytes.
 * @returns Pointer to response (valid until next at_command) or NULL
 *          if a timeout occurs.
 */
const char *at_command_raw(struct at *at, const void *data, size_t size);

/**
 * Send an AT command and return -1 if it doesn't return OK.
 */
#define at_command_simple(at, cmd...)                                       \
    do {                                                                    \
        const char *_response = at_command(at, cmd);                        \
        if (!_response)                                                     \
            return -1; /* timeout */                                        \
        if (strcmp(_response, "")) {                                        \
            errno = EINVAL;                                                 \
            return -1;                                                      \
        }                                                                   \
    } while (0)

/**
 * Count macro arguments. Source:
 * http://stackoverflow.com/questions/2124339/c-preprocessor-va-args-number-of-arguments
 */
#define _NUMARGS(...) (sizeof((void *[]){0, ##__VA_ARGS__})/sizeof(void *)-1)

/**
 * Scanf a response and return -1 if it fails.
 */
#define at_simple_scanf(_response, format, ...)                             \
    do {                                                                    \
        if (!_response)                                                     \
            return -1; /* timeout */                                        \
        if (sscanf(_response, format, __VA_ARGS__) != _NUMARGS(__VA_ARGS__)) { \
            errno = EINVAL;                                                 \
            return -1;                                                      \
        }                                                                   \
    } while (0)

#endif

/* vim: set ts=4 sw=4 et: */
