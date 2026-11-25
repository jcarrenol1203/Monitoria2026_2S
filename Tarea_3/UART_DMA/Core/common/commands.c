/* Core/common/commands.c — the application layer: it knows no UART, only modules */
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "commands.h"
#include "heartbeat.h"
#include "pwm.h"

static void cmd_help(char *reply, size_t size);
static void cmd_info(char *reply, size_t size);
static void cmd_blink(const cmd_t *cmd, char *reply, size_t size);
static void cmd_echo(const cmd_t *cmd, char *reply, size_t size);
static void cmd_pwm(const cmd_t *cmd, char *reply, size_t size);
static bool parse_number(const char *text, uint32_t *value);

void commands_execute(const cmd_t *cmd, char *reply, size_t reply_size)
{
    if (cmd->too_many_args) {
        (void)snprintf(reply, reply_size, "error: too many arguments");
        return;
    }

    switch (cmd->id) {
    case CMD_HELP:
        cmd_help(reply, reply_size);
        break;
    case CMD_INFO:
        cmd_info(reply, reply_size);
        break;
    case CMD_BLINK:
        cmd_blink(cmd, reply, reply_size);
        break;
    case CMD_ECHO:
        cmd_echo(cmd, reply, reply_size);
        break;
    case CMD_PWM:
        cmd_pwm(cmd, reply, reply_size);
        break;
    case CMD_UNKNOWN:
    default:
        (void)snprintf(reply, reply_size, "error: unknown command '%s' (type help)", cmd->name);
        break;
    }
}

/* Only digits are accepted, at most 9 of them, so the value never overflows. */
static bool parse_number(const char *text, uint32_t *value)
{
    uint32_t result = 0U;
    size_t   length = strlen(text);

    if ((length == 0U) || (length > 9U)) {
        return false;
    }
    for (size_t i = 0U; i < length; i++) {
        if ((text[i] < '0') || (text[i] > '9')) {
            return false;
        }
        result = (result * 10U) + (uint32_t)(text[i] - '0');
    }
    *value = result;
    return true;
}

static void cmd_help(char *reply, size_t size)
{
    (void)snprintf(reply, size, "commands: help, info, blink <ms>, echo <text>, pwm <0..100>");
}

static void cmd_info(char *reply, size_t size)
{
    (void)snprintf(reply, size, "uptime %lu ms, blink %lu ms",
                   (unsigned long)HAL_GetTick(),
                   (unsigned long)heartbeat_get_period());
}

static void cmd_blink(const cmd_t *cmd, char *reply, size_t size)
{
    uint32_t ms;

    if (cmd->argc != 1U) {
        (void)snprintf(reply, size, "usage: blink <ms>");
        return;
    }
    if (!parse_number(cmd->argv[0], &ms)) {
        (void)snprintf(reply, size, "error: '%s' is not a number", cmd->argv[0]);
        return;
    }
    if (!heartbeat_set_period(ms)) {
        (void)snprintf(reply, size, "error: blink accepts %u to %u ms",
                       (unsigned)HEARTBEAT_MIN_MS, (unsigned)HEARTBEAT_MAX_MS);
        return;
    }
    (void)snprintf(reply, size, "ok: blink %lu ms", (unsigned long)ms);
}

/* Builds the answer one word at a time: each piece starts where the last one ended. */
static void cmd_echo(const cmd_t *cmd, char *reply, size_t size)
{
    size_t used = 0U;

    reply[0] = '\0';
    for (uint8_t i = 0U; i < cmd->argc; i++) {
        int written = snprintf(&reply[used], size - used, "%s%s",
                               (i == 0U) ? "" : " ", cmd->argv[i]);
        if (written < 0) {
            break;
        }
        used += (size_t)written;
        if (used >= size) {                 /* did not fit: reply is still a complete, shorter string */
            break;
        }
    }
}

static void cmd_pwm(const cmd_t *cmd, char *reply, size_t size)
{
    uint32_t percent;

    if (cmd->argc != 1U) {
        (void)snprintf(reply, size, "usage: pwm <0..100>");
        return;
    }
    if (!parse_number(cmd->argv[0], &percent)) {
        (void)snprintf(reply, size, "error: '%s' is not a number", cmd->argv[0]);
        return;
    }
    if (percent > 100U) {
        (void)snprintf(reply, size, "error: pwm accepts 0 to 100 %%");
        return;
    }
    if (!pwm_set_duty(percent)) {
        (void)snprintf(reply, size, "error: pwm is not started in this application");
        return;
    }
    (void)snprintf(reply, size, "ok: pwm %lu %%", (unsigned long)percent);
}
