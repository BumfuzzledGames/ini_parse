#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ini.h"
#include "span.h"

#define ALUP "abcdefghijklmnopqrstuvwxyz"
#define ALDN "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define NUM "0123456789"

typedef struct Token {
    ConstSpan span;
    enum { NONE, ID, NUMBER, STRING, BOOLEAN, CHAR } type;
} Token;

static int skip_whitespace(ConstSpan *span) {
    int length = 0;
    while (span->start < span->end && span->start[0] != '\n' && isspace(span->start[0])) {
        span->start++;
        length++;
    }
    return length;
}

static int skip_comment(ConstSpan *span) {
    if (span->start >= span->end || span->start[0] != '#')
        return 0;
    int length = 0;
    while (span->start < span->end && span->start[0] != '\n') {
        span->start++;
        length++;
    }
    return length;
}

static int scan(ConstSpan *span, const char *accept, const char *reject) {
    int length = 0;
    while (span->start < span->end && 
        (accept == 0 || strchr(accept, span->start[0])) &&
        (reject == 0 || !strchr(reject, span->start[0])))
    {
        span->start++;
        length++;
    }
    return length;
}

static bool span_starts_with_string(ConstSpan span, const char *string, bool case_insensitive) {
    for (; span.start < span.end && string[0]; span.start++, string++) {
        char a = span.start[0];
        char b = *string;
        if (case_insensitive) {
            a = toupper(a);
            b = toupper(b);
        }
        if (a != b)
            return false;
    }
    return string[0] == 0;
}

static int scan_id(ConstSpan *span, Token *token) {
    if (strchr(ALUP ALDN, span->start[0])) {
        *token = (Token){ .span = {.start = span->start}, .type = ID };
        token->span.end = token->span.start + scan(span, ALUP ALDN NUM, 0);
        return 1;
    }
    return 0;
}

static int scan_number(ConstSpan *span, Token *token) {
    ConstSpan restart = *span;
    if (strchr("+-." NUM, span->start[0])) {
        *token = (Token){ .span = {.start = span->start}, .type = NUMBER };
        if(strchr("+-", span->start[0]))
            span->start++;
        if(scan(span, NUM, 0) == 0 && (span->start >= span->end || span->start[0] != '.'))
            goto fail;
        if (span->start[0] == '.') {
            span->start++;
            if (scan(span, NUM, 0) == 0)
                goto fail;
        }
        token->span.end = span->start;
        return 1;
    }
fail:
    *span = restart;
    return 0;
}

static int scan_string(ConstSpan *span, Token *token) {
    ConstSpan restart = *span;
    if (span->start[0] == '"') {
        span->start++;
        *token = (Token){ .span = {.start = span->start}, .type = STRING };
        token->span.end = token->span.start + scan(span, 0, "\n\"");
        if (span->start >= span->end || span->start[0] != '"')
            goto fail;
        span->start++;
        return 1;
    }
fail:
    *span = restart;
    return 0;
}

static int scan_boolean(ConstSpan *span, Token *token) {
    ConstSpan restart = *span;
    if (scan_id(span, token) && (span_starts_with_string(token->span, "true", true) || span_starts_with_string(token->span, "false", true))) {
        token->type = BOOLEAN;
        return 1;
    }
    *span = restart;
    return 0;
}

static int scan_char(ConstSpan *span, Token *token) {
    if (span->start < span->end) {
        *token = (Token){ .span = {.start = span->start, .end = span->start + 1}, .type = CHAR };
        span->start++;
        return 1;
    }
    return 0;
}

static int next_token_restart(ConstSpan *span, Token *token, int restart_type) {
    skip_whitespace(span);
    skip_comment(span);
    if (span->start >= span->end)
        return 0;
    switch (restart_type) {
    case NONE: if (scan_id(span, token)) return 1;
    case ID: if (scan_number(span, token)) return 1;
    case NUMBER: if (scan_string(span, token)) return 1;
    case STRING: if (scan_boolean(span, token)) return 1;
    case BOOLEAN: scan_char(span, token); return 1;
    }
    return 0;
}

static int next_token(ConstSpan *span, Token *token) {
    return next_token_restart(span, token, NONE);
}

static size_t scan_tokens(ConstSpan *span, Token *tokens, size_t num_tokens) {
    for (int i = 0; i < num_tokens; i++)
        if (!next_token(span, &tokens[i]))
            return i;
    return num_tokens;
}

static IniValue get_token_value(Token *token) {
    switch (token->type) {
    case STRING:
    case ID: return (IniValue) { INI_VALUE_CONST_SPAN, .const_span_value = token->span };
    case NUMBER: return (IniValue) { INI_VALUE_DOUBLE, .double_value = atof(token->span.start) };
    case BOOLEAN: return (IniValue) { INI_VALUE_BOOLEAN, .boolean_value = span_starts_with_string(token->span, "true", true) };
    default:
    case NONE: return (IniValue) { INI_VALUE_NONE };
    }
}

static int parse_property(ConstSpan *span, ConstSpan section, void *user_data, IniPropertyCallback callback) {
    ConstSpan restart = *span;
    Token tokens[3] = { 0 };
    if (scan_tokens(span, tokens, 3) != 3 ||
        tokens[0].type != ID || tokens[1].type != CHAR || tokens[1].span.start[0] != '=')
        goto fail;
    do {
        if (tokens[2].type == NUMBER || tokens[2].type == STRING || tokens[2].type == BOOLEAN)
            break;
        span->start = tokens[2].span.start;
    } while (next_token_restart(span, &tokens[2], tokens[2].type));
    if (section.start == 0 || section.start >= section.end) {
        fprintf(stderr, "Error: property found before any sections\n");
        goto fail;
    }
    callback(user_data, section, tokens[1].span, get_token_value(&tokens[2]));
    return 1;
fail:   
    *span = restart;
    return 0;
}

static int parse_section(ConstSpan *span, ConstSpan *current_section) {
    ConstSpan restart = *span;
    Token tokens[3] = { 0 };
    if (scan_tokens(span, tokens, 3) != 3)
        goto fail;
    if (tokens[0].type != CHAR || tokens[0].span.start[0] != '[' ||
        tokens[1].type != ID ||
        tokens[2].type != CHAR || tokens[2].span.start[0] != ']')
        goto fail;
    *current_section = tokens[1].span;
    return 1;
fail:
    *span = restart;
    return 0;
}

static int parse_empty_line(ConstSpan *span) {
    ConstSpan restart = *span;
    Token token = { 0 };
    if (!next_token(span, &token))
        goto fail;
    if (token.type == CHAR && token.span.start[0] == '\n')
        return 1;
fail:
    *span = restart;
    return 0;
}

int parse_ini(ConstSpan *span, void *user_data, IniPropertyCallback property_callback) {
    ConstSpan section = { 0 };
    int line = 0;
    while (span->start < span->end) {
        line++;
        if (parse_empty_line(span)) continue;
        if (parse_section(span, &section)) continue;
        if (parse_property(span, section, user_data, property_callback)) continue;
        break;
    }
    if (span->start < span->end) {
        printf("Syntax error on line %d\n", line);
        for (const char *s = span->start;
            s < span->end && (s == span->start || s[-1] != '\n');
            s++)
            putchar(*s);
        return 0;
    }
    return 1;
}