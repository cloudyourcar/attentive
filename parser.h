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

/** Line scanner. Should return one of the AT_RESPONSE_* values if the line is
 *  identified or AT_RESPONSE_UNKNOWN to fall back to the default scanner. */
typedef enum at_response_type (*line_scanner_t)(const void *line, size_t len, void *priv);

struct at_parser_callbacks {
    line_scanner_t scan_line;
    void (*handle_response)(const void *line, size_t len, void *priv);
    void (*handle_urc)(const void *line, size_t len, void *priv);
};

struct at_parser *at_parser_alloc(const struct at_parser_callbacks *cbs, size_t bufsize, void *priv);

void at_parser_reset(struct at_parser *parser);

void at_parser_await_response(struct at_parser *parser, bool dataprompt, line_scanner_t scanner);

void at_parser_feed(struct at_parser *parser, const void *data, size_t len);

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
