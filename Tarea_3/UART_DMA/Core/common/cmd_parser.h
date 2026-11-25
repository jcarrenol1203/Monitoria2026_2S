/* Core/common/cmd_parser.h — split a text line into a command and its arguments */
#ifndef CMD_PARSER_H
#define CMD_PARSER_H

#include <stdbool.h>
#include <stdint.h>

#define CMD_MAX_ARGS  4U

typedef enum {
    CMD_HELP,
    CMD_INFO,
    CMD_BLINK,
    CMD_ECHO,
    CMD_PWM,        /* Stage 5 */
    CMD_UNKNOWN
} cmd_id_t;

typedef struct {
    cmd_id_t    id;                    /* which command; CMD_UNKNOWN if not in the table */
    const char *name;                  /* the first word, as typed */
    uint8_t     argc;                  /* number of arguments after the name */
    const char *argv[CMD_MAX_ARGS];    /* the arguments, each one ending in '\0' */
    bool        too_many_args;
} cmd_t;

/* Splits 'line' in place: separators become '\0'. Returns false for an empty line. */
bool cmd_parse(char *line, cmd_t *cmd);

#endif
