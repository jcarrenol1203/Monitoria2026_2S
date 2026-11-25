/* Core/common/cmd_parser.c — no hardware here: it compiles with gcc on a PC too */
#include <string.h>
#include "cmd_parser.h"

typedef struct {
    const char *word;
    cmd_id_t    id;
} cmd_entry_t;

static const cmd_entry_t cmd_table[] = {
    { "help",  CMD_HELP  },
    { "info",  CMD_INFO  },
    { "blink", CMD_BLINK },
    { "echo",  CMD_ECHO  },
    { "pwm",   CMD_PWM   },
};

#define CMD_TABLE_SIZE  (sizeof cmd_table / sizeof cmd_table[0])

static bool is_separator(char c)
{
    return (c == ' ') || (c == '\t');
}

/* Returns the start of the next word and ends it with '\0'; *p moves past it. */
static char *next_word(char **p)
{
    char *start = *p;

    while (is_separator(*start)) {          /* skip leading separators */
        start++;
    }
    if (*start == '\0') {
        *p = start;
        return NULL;                        /* no more words */
    }

    char *end = start;
    while ((*end != '\0') && !is_separator(*end)) {
        end++;
    }
    if (*end != '\0') {
        *end = '\0';                        /* cut the word in place */
        end++;
    }
    *p = end;
    return start;
}

bool cmd_parse(char *line, cmd_t *cmd)
{
    char *cursor = line;
    char *word;

    cmd->id            = CMD_UNKNOWN;
    cmd->name          = "";
    cmd->argc          = 0U;
    cmd->too_many_args = false;
    for (uint8_t i = 0U; i < CMD_MAX_ARGS; i++) {
        cmd->argv[i] = "";
    }

    word = next_word(&cursor);
    if (word == NULL) {
        return false;                       /* empty line */
    }
    cmd->name = word;

    for (size_t i = 0U; i < CMD_TABLE_SIZE; i++) {
        if (strcmp(word, cmd_table[i].word) == 0) {
            cmd->id = cmd_table[i].id;
            break;
        }
    }

    while ((word = next_word(&cursor)) != NULL) {
        if (cmd->argc < CMD_MAX_ARGS) {
            cmd->argv[cmd->argc] = word;
            cmd->argc++;
        } else {
            cmd->too_many_args = true;      /* keep counting nothing: just flag it */
        }
    }
    return true;
}
