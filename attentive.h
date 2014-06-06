/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#ifndef ATTENTIVE_H
#define ATTENTIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Since the AT parser can't (and shouldn't) know about every single misbehaving
 * modem out there, we use a pluggable architecture where there are three parsers
 * involved whenever a line is received: command-specific, modem-specific and
 * generic. Each parser should return one of the below enum values to indicate
 * the semantics of the line received (or AT_RESPONSE_UNKNOWN to let others
 * decide).
 */


/**
 * AT response type.
 *
 * Describes response lines that can be received from the modem. See V.25ter
 * and 3GPP TS 27.007 for the vocabulary.
 */
enum at_response_type {
    AT_RESPONSE_UNEXPECTED = -1,    /**< Unexpected line; usually an unhandled URC. */
    AT_RESPONSE_UNKNOWN = 0,        /**< Pass the response to next parser in chain. */
    AT_RESPONSE_INTERMEDIATE,       /**< Intermediate response. Stored. */
    AT_RESPONSE_FINAL_OK,           /**< Final response. NOT stored. */
    AT_RESPONSE_FINAL,              /**< Final response. Stored. */
    AT_RESPONSE_URC,                /**< Unsolicited Result Code. Passed to URC handler. */
    _AT_RESPONSE_RAWDATA_FOLLOWS,   /**< @internal (see AT_RESPONSE_RAWDATA_FOLLOWS)*/
    _AT_RESPONSE_HEXDATA_FOLLOWS,   /**< @internal (see AT_RESPONSE_HEXDATA_FOLLOWS) */

    _AT_RESPONSE_ENUM_SIZE_SHOULD_BE_INT32 = INT32_MAX
};
/** The line is followed by a newline and a block of raw data. */
#define AT_RESPONSE_RAWDATA_FOLLOWS(amount) \
    (_AT_RESPONSE_RAWDATA_FOLLOWS | ((amount) << 8))
/** The line is followed by a newline and a block of hex-escaped data. */
#define AT_RESPONSE_HEXDATA_FOLLOWS(amount) \
    (_AT_RESPONSE_HEXDATA_FOLLOWS | ((amount) << 8))

struct at;

typedef enum at_response_type (*response_parser_t)(const char *line, size_t len);

typedef void (*urc_callback_t)(const char *line, size_t len);

/**
 * Create an AT channel instance.
 *
 * @param stream File stream connected to the modem.
 * @param modem_parser Modem-specific response line parser.
 * @param urc_callback Modem-specific URC callback.
 * @returns Instance pointer on success, NULL on failure.
 */
struct at *at_open(FILE *stream, response_parser_t modem_parser, urc_callback_t urc_callback);

/**
 * Close and free an AT channel instance.
 *
 * @param at AT channel instance.
 */
void at_close(struct at *at);

/**
 * Make the parser expect a dataprompt for the next command.
 *
 * Some AT commands, mostly those used for transmitting raw data, return a "> "
 * prompt (without a newline). The parser must be told explicitly to expect it
 * on a per-command basis.
 *
 * @param at AT channel instance.
 */
void at_expect_dataprompt(struct at *at);

/**
 * Set per-command response parser for the next command.
 *
 * Should return AT_RESPONSE_UNKNOWN to fall back to the built-in one.
 * See description of enum at_response_type for details.
 *
 * @param at AT channel instance.
 * @param parser Per-command response parser.
 */
void at_set_command_response_parser(struct at *at, response_parser_t parser);

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
 * Check if a response starts with one of the prefixes in a table.
 *
 * @param line AT response line.
 * @param table List of prefixes.
 * @returns True if found, false otherwise.
 */
bool at_prefix_in_table(const char *line, const char *table[]);

#endif

/* vim: set ts=4 sw=4 et: */
