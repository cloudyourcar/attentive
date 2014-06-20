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

/**
 * Create an AT channel instance.
 *
 * @param dev AT modem device.
 * @returns Instance pointer on success, NULL and sets errno on failure.
 */
//struct at *at_alloc(struct at_port *port, struct at_device *dev);
struct at *at_alloc(void);

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
 * Make the parser expect a dataprompt for the next command.
 *
 * Some AT commands, mostly those used for transmitting raw data, return a "> "
 * prompt (without a newline). The parser must be told explicitly to expect it
 * on a per-command basis.
 *
 * @param at AT channel instance.
 */
void at_command_expect_dataprompt(struct at *at);

/**
 * Set per-command response scanner for the next command.
 *
 * Should return AT_RESPONSE_UNKNOWN to fall back to the built-in one.
 * See enum at_response_type for details.
 *
 * @param at AT channel instance.
 * @param scanner Per-command response scanner.
 */
void at_command_set_response_parser(struct at *at, line_scanner_t scanner);

/**
 * Set command timeout in seconds.
 *
 * @param at AT channel instance.
 * @param timeout Timeout in seconds.
 */
void at_command_set_timeout(struct at *at, int timeout);

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

#endif

/* vim: set ts=4 sw=4 et: */
