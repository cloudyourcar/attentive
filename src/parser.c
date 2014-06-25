#include <attentive/parser.h>

#include <stdio.h>
#include <string.h>

enum at_parser_state {
    STATE_IDLE,
    STATE_READLINE,
    STATE_DATAPROMPT,
    STATE_RAWDATA,
    STATE_HEXDATA,
};

struct at_parser {
    const struct at_parser_callbacks *cbs;
    void *priv;

    enum at_parser_state state;
    bool expect_dataprompt;
    size_t data_left;
    int nibble;

    char *buf;
    size_t buf_used;
    size_t buf_size;
    size_t buf_current;
};

static const char *final_ok_responses[] = {
    "OK",
    NULL
};

static const char *final_responses[] = {
    "OK",
    "ERROR",
    "NO CARRIER",
    "+CME ERROR:",
    "+CMS ERROR:",
    NULL
};

static const char *urc_responses[] = {
    "RING",
    NULL
};

struct at_parser *at_parser_alloc(const struct at_parser_callbacks *cbs, size_t bufsize, void *priv)
{
    /* Allocate parser struct. */
    struct at_parser *parser = (struct at_parser *) malloc(sizeof(struct at_parser));
    if (parser == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    /* Allocate response buffer. */
    parser->buf = malloc(bufsize);
    if (parser->buf == NULL) {
        free(parser);
        errno = ENOMEM;
        return NULL;
    }
    parser->cbs = cbs;
    parser->buf_size = bufsize;
    parser->priv = priv;

    /* Prepare instance. */
    at_parser_reset(parser);

    return parser;
}

void at_parser_reset(struct at_parser *parser)
{
    parser->state = STATE_IDLE;
    parser->expect_dataprompt = false;
    parser->buf_used = 0;
    parser->buf_current = 0;
    parser->data_left = 0;
}

void at_parser_expect_dataprompt(struct at_parser *parser)
{
    parser->expect_dataprompt = true;
}

void at_parser_await_response(struct at_parser *parser)
{
    parser->state = (parser->expect_dataprompt ? STATE_DATAPROMPT : STATE_READLINE);
}

bool at_prefix_in_table(const char *line, const char *table[])
{
    for (int i=0; table[i] != NULL; i++)
        if (!strncmp(line, table[i], strlen(table[i])))
            return true;

    return false;
}

static enum at_response_type generic_line_scanner(const char *line, size_t len)
{
    (void) len;

    if (at_prefix_in_table(line, urc_responses))
        return AT_RESPONSE_URC;
    else if (at_prefix_in_table(line, final_ok_responses))
        return AT_RESPONSE_FINAL_OK;
    else if (at_prefix_in_table(line, final_responses))
        return AT_RESPONSE_FINAL;
    else
        return AT_RESPONSE_INTERMEDIATE;
}

static void parser_append(struct at_parser *parser, char ch)
{
    if (parser->buf_used < parser->buf_size-1)
        parser->buf[parser->buf_used++] = ch;
}

static void parser_include_line(struct at_parser *parser)
{
    /* Append a newline. */
    parser_append(parser, '\n');

    /* Advance the current command pointer to the new position. */
    parser->buf_current = parser->buf_used;
}

static void parser_discard_line(struct at_parser *parser)
{
    /* Rewind the end pointer back to the previous position. */
    parser->buf_used = parser->buf_current;
}

static void parser_finalize(struct at_parser *parser)
{
    /* Remove the last newline, if any. */
    if (parser->buf_used > 0)
        parser->buf_used--;

    /* NULL-terminate the response. */
    parser->buf[parser->buf_used] = '\0';
}

/**
 * Helper, called whenever a full response line is collected.
 */
static void parser_handle_line(struct at_parser *parser)
{
    /* Skip empty lines. */
    if (parser->buf_used == parser->buf_current)
        return;

    /* NULL-terminate the response .*/
    parser->buf[parser->buf_used] = '\0';

    /* Extract line address & length for later use. */
    const char *line = parser->buf + parser->buf_current;
    size_t len = parser->buf_used - parser->buf_current;

    /* Log the received line. */
    printf("< '%.*s'\n", (int) len, line);

    /* Determine response type. */
    enum at_response_type type = AT_RESPONSE_UNKNOWN;
    if (parser->cbs->scan_line)
        type = parser->cbs->scan_line(line, len, parser->priv);
    if (!type)
        type = generic_line_scanner(line, len);

    /* Expected URCs and all unexpected lines are sent to URC handler. */
    if (type == AT_RESPONSE_URC || parser->state == STATE_IDLE)
    {
        /* Fire the callback on the URC line. */
        parser->cbs->handle_urc(parser->buf + parser->buf_current,
                                parser->buf_used - parser->buf_current,
                                parser->priv);

        /* Discard the URC line from the buffer. */
        parser_discard_line(parser);

        return;
    }

    /* Accumulate everything that's not a final OK. */
    if (type != AT_RESPONSE_FINAL_OK) {
        /* Include the line in the buffer. */
        parser_include_line(parser);
    } else {
        /* Discard the line from the buffer. */
        parser_discard_line(parser);
    }

    /* Act on the response type. */
    switch (type & _AT_RESPONSE_TYPE_MASK) {
        case AT_RESPONSE_FINAL_OK:
        case AT_RESPONSE_FINAL:
        {
            /* Fire the response callback. */
            parser_finalize(parser);
            parser->cbs->handle_response(parser->buf, parser->buf_used, parser->priv);

            /* Go back to idle state. */
            at_parser_reset(parser);
        }
        break;

        case _AT_RESPONSE_RAWDATA_FOLLOWS:
        {
            /* Switch parser state to rawdata mode. */
            parser->data_left = (int)type >> 8;
            parser->state = STATE_RAWDATA;
        }
        break;

        case _AT_RESPONSE_HEXDATA_FOLLOWS:
        {
            /* Switch parser state to hexdata mode. */
            parser->data_left = (int)type >> 8;
            parser->nibble = -1;
            parser->state = STATE_HEXDATA;
        }
        break;

        default:
        {
            /* Keep calm and carry on. */
        }
        break;
    }
}

static int hex2int(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

void at_parser_feed(struct at_parser *parser, const void *data, size_t len)
{
    const uint8_t *buf = data;

    while (len > 0)
    {
        /* Fetch next character. */
        uint8_t ch = *buf++; len--;

        switch (parser->state)
        {
            case STATE_IDLE:
            case STATE_READLINE:
            case STATE_DATAPROMPT:
            {
                if ((ch != '\r') && (ch != '\n')) {
                    /* Append the character if it's not a newline. */
                    parser_append(parser, ch);
                }

                /* Handle full lines. */
                if ((ch == '\n') ||
                    (parser->state == STATE_DATAPROMPT &&
                     parser->buf_used == 2 &&
                     !memcmp(parser->buf, "> ", 2)))
                {
                    parser_handle_line(parser);
                }
            }
            break;

            case STATE_RAWDATA: {
                if (parser->data_left > 0) {
                    parser_append(parser, ch);
                    parser->data_left--;
                }

                if (parser->data_left == 0) {
                    parser_include_line(parser);
                    parser->state = STATE_READLINE;
                }
            } break;

            case STATE_HEXDATA: {
                if (parser->data_left > 0) {
                    int value = hex2int(ch);
                    if (value != -1) {
                        if (parser->nibble == -1) {
                            parser->nibble = value;
                        } else {
                            value |= (parser->nibble << 4);
                            parser->nibble = -1;
                            parser_append(parser, value);
                            parser->data_left--;
                        }
                    }
                }

                if (parser->data_left == 0) {
                    parser_include_line(parser);
                    parser->state = STATE_READLINE;
                }
            } break;
        }
    }
}

void at_parser_free(struct at_parser *parser)
{
    free(parser->buf);
    free(parser);
}

/* vim: set ts=4 sw=4 et: */
