#include "parser.h"

#include <string.h>

enum at_parser_state {
    STATE_IDLE,
    STATE_READLINE,
    STATE_DATAPROMPT,
    STATE_RAWDATA,
};

struct at_parser {
    const struct at_parser_callbacks *cbs;
    void *priv;

    enum at_parser_state state;
    line_scanner_t per_command_scanner;

    char *response;
    size_t response_length;
    size_t response_bufsize;
    size_t current_line_start;

    size_t data_left;
};

static const char *ok_responses[] = {
    "OK",
    "> ",
    NULL
};

static const char *error_responses[] = {
    "ERROR",
    "NO CARRIER",
    "+CME ERROR:",
    "+CMS ERROR:",
    NULL,
};

static const char *urc_responses[] = {
    "RING",
    NULL,
};

struct at_parser *at_parser_alloc(const struct at_parser_callbacks *cbs, size_t bufsize, void *priv)
{
    /* Allocate parser struct. */
    struct at_parser *parser = (struct at_parser *) malloc(sizeof(struct at_parser));
    if (!parser) {
        errno = ENOMEM;
        return NULL;
    }

    /* Allocate response buffer. */
    parser->response = malloc(bufsize);
    if (!parser->response) {
        free(parser);
        errno = ENOMEM;
        return NULL;
    }
    parser->cbs = cbs;
    parser->response_bufsize = bufsize;
    parser->priv = priv;

    /* Prepare instance. */
    at_parser_reset(parser);

    return parser;
}

void at_parser_reset(struct at_parser *parser)
{
    parser->state = STATE_IDLE;
    parser->per_command_scanner = NULL;
    parser->response_length = 0;
    parser->current_line_start = 0;
    parser->data_left = 0;
}

void at_parser_await_response(struct at_parser *parser, bool dataprompt, line_scanner_t scanner)
{
    parser->per_command_scanner = scanner;
    parser->state = (dataprompt ? STATE_DATAPROMPT : STATE_READLINE);
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
    else if (at_prefix_in_table(line, error_responses))
        return AT_RESPONSE_FINAL;
    else if (at_prefix_in_table(line, ok_responses))
        return AT_RESPONSE_FINAL_OK;
    else
        return AT_RESPONSE_INTERMEDIATE;
}

void at_parser_feed(struct at_parser *parser, const void *data, size_t len)
{
    const uint8_t *buf = data;

    while (len > 0)
    {
        switch (parser->state)
        {
            case STATE_IDLE:
            case STATE_READLINE:
            case STATE_DATAPROMPT:
            {
                uint8_t ch = *buf++;

                /* Accept any line ending convention. */
                if (ch == '\r')
                    ch = '\n';

                /* Append the character if there's any space left. */
                if (parser->response_length < parser->response_bufsize-1)
                    parser->response[parser->response_length++] = ch;

                if (ch == '\n' || (parser->state == STATE_DATAPROMPT &&
                                   parser->response_length == 2 &&
                                   !memcmp(parser->response, "> ", 2)))
                {
                    /* Remove the last newline character. */
                    parser->response_length--;

                    /* NULL-terminate the response .*/
                    parser->response[parser->response_length] = '\0';

                    /* Determine response type. */
                    enum at_response_type type = AT_RESPONSE_UNKNOWN;
                    if (parser->per_command_scanner)
                        type = parser->per_command_scanner(parser->response, parser->response_length, parser->priv);
                    if (!type && parser->cbs->scan_line)
                        type = parser->cbs->scan_line(parser->response, parser->response_length, parser->priv);
                    if (!type)
                        type = generic_line_scanner(parser->response, parser->response_length);

                    /* Expected URCs and all unexpected lines go here. */
                    if (type == AT_RESPONSE_URC || parser->state == STATE_IDLE)
                    {
                        /* Fire the callback on the URC line. */
                        parser->cbs->handle_urc(parser->response + parser->current_line_start,
                                                parser->response_length - parser->current_line_start,
                                                parser->priv);

                        /* Discard the URC line from the buffer. */
                        parser->response_length = parser->current_line_start;

                        continue;
                    }

                    if (type == AT_RESPONSE_FINAL_OK || type == AT_RESPONSE_FINAL)
                    {
                        /* Fire the response callback. */
                        parser->cbs->handle_response(parser->response, parser->response_length, parser->priv);

                        /* Go back to idle state. */
                        at_parser_reset(parser);

                        continue;
                    }
                }
            }
            break;

            case STATE_RAWDATA: {
                uint8_t ch = *buf++;

                if (parser->data_left > 0 &&
                    parser->response_length < parser->response_bufsize-1)
                {
                    parser->data_left--;
                    parser->response[parser->response_length++] = ch;
                }
            } break;
        }
    }
}

void at_parser_free(struct at_parser *parser)
{
    free(parser->response);
    free(parser);
}

/* vim: set ts=4 sw=4 et: */
