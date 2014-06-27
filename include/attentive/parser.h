/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#ifndef ATTENTIVE_PARSER_H
#define ATTENTIVE_PARSER_H

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

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
    _AT_RESPONSE_RAWDATA_FOLLOWS,   /**< @internal (see AT_RESPONSE_RAWDATA_FOLLOWS) */
    _AT_RESPONSE_HEXDATA_FOLLOWS,   /**< @internal (see AT_RESPONSE_HEXDATA_FOLLOWS) */

    _AT_RESPONSE_ENUM_SIZE_SHOULD_BE_INT32 = INT32_MAX
};
/** The line is followed by a newline and a block of raw data. */
#define AT_RESPONSE_RAWDATA_FOLLOWS(amount) \
    (_AT_RESPONSE_RAWDATA_FOLLOWS | ((amount) << 8))
/** The line is followed by a newline and a block of hex-escaped data. */
#define AT_RESPONSE_HEXDATA_FOLLOWS(amount) \
    (_AT_RESPONSE_HEXDATA_FOLLOWS | ((amount) << 8))
/** @internal */
#define _AT_RESPONSE_TYPE_MASK 0xff

/** Line scanner. Should return one of the AT_RESPONSE_* values if the line is
 *  identified or AT_RESPONSE_UNKNOWN to fall back to the default scanner. */
typedef enum at_response_type (*at_line_scanner_t)(const char *line, size_t len, void *priv);

/** Response handler. */
typedef void (*at_response_handler_t)(const char *line, size_t len, void *priv);

struct at_parser_callbacks {
    at_line_scanner_t scan_line;
    at_response_handler_t handle_response;
    at_response_handler_t handle_urc;
};

/**
 * Allocate a parser instance.
 *
 * @param cbs Parser callbacks. Structure is not copied; must persist for
 *            the lifetime of the parser.
 * @param bufsize Response buffer size on bytes.
 * @param priv Private argument; passed to callbacks.
 * @returns Parser instance pointer.
 */
struct at_parser *at_parser_alloc(const struct at_parser_callbacks *cbs, size_t bufsize, void *priv);

/**
 * Reset parser instance to initial state.
 *
 * @param parser Parser instance.
 */
void at_parser_reset(struct at_parser *parser);

/**
 * Make the parser expect a dataprompt for the next command.
 *
 * Some AT commands, mostly those used for transmitting raw data, return a "> "
 * prompt (without a newline). The parser must be told explicitly to expect it
 * on a per-command basis.
 *
 * @param parser Parser instance.
 */
void at_parser_expect_dataprompt(struct at_parser *parser);

/**
 * Inform the parser that a command will be invoked. Causes a response callback
 * at the next command completion.
 *
 * @param parser Parser instance.
 */
void at_parser_await_response(struct at_parser *parser);

/**
 * Feed parser. Callbacks are always called from this function's context.
 *
 * @param parser Parser instance.
 * @param data Bytes to feed.
 * @param len Number of bytes in data.
 */
void at_parser_feed(struct at_parser *parser, const void *data, size_t len);

/**
 * Deallocate a parser instance.
 *
 * @param parser Parser instance allocated with at_parser_alloc.
 */
void at_parser_free(struct at_parser *parser);

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
