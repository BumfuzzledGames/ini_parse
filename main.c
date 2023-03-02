#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ini.h"

void property(void *user_data, ConstSpan section, ConstSpan key, IniValue value) {
    printf("[");
    fwrite(section.start, 1, section.end - section.start, stdout);
    printf("] ");
    fwrite(key.start, 1, key.end - key.start, stdout);
    switch (value.type) {
    case INI_VALUE_CONST_SPAN:
        printf(" (string) \"");
        fwrite(value.const_span_value.start, 1, value.const_span_value.end - value.const_span_value.start, stdout);
        printf("\"");
        break;
    case INI_VALUE_DOUBLE:
        printf(" (double) %f", value.double_value);
        break;
    case INI_VALUE_BOOLEAN:
        printf(" (boolean) %s", value.boolean_value ? "true" : "false");
        break;
    case INI_VALUE_NONE:
        printf(" (none) This shouldn't happen, what happened?");
        break;
    }
    printf("\n");
}


int main() {
    const char ini_test[] =
        "[foo]\n"
        "bar = 10 # This line has spaces and a comment\n"
        "\n" // An empty line
        "[cheese]    \n"
        "foo      =      \"This is a string\"\n"
        "bar=true\n"
        "bar=TrUe\n"
        "bar=TRUE\n"
        "bar=false\n"
        "baz=10\n"
        "baz=.3\n"
        "qux=-.75\n"
        "baz=+100.0\n"
        "bar=FaLsE\n"
        "bar=10.";
    size_t ini_length = sizeof(ini_test) - 1; // Do not include nul
    char *ini = malloc(ini_length);
    assert(ini);
    memcpy(ini, ini_test, ini_length);
    if (!parse_ini(&(ConstSpan) { ini, ini + ini_length }, 0, property)) {
        printf("Parse failed\n");
    }
}