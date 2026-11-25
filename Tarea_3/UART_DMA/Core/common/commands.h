/* Core/common/commands.h — what each command does */
#ifndef COMMANDS_H
#define COMMANDS_H

#include <stddef.h>
#include "cmd_parser.h"

/* Runs 'cmd' and writes the answer (one line, no line ending) into 'reply'. */
void commands_execute(const cmd_t *cmd, char *reply, size_t reply_size);

#endif
