#ifndef INI_H
#define INI_H
#include "span.h"
#include <stdbool.h>

enum {
    INI_VALUE_NONE,
    INI_VALUE_CONST_SPAN,
    INI_VALUE_DOUBLE,
    INI_VALUE_BOOLEAN
};

typedef struct IniValue {
    int type;
    union {
        ConstSpan const_span_value;
        double double_value;
        bool boolean_value;
    };
} IniValue;

typedef void (*IniPropertyCallback)(void *user_data, ConstSpan section, ConstSpan key, IniValue value);

int parse_ini(ConstSpan *span, void *user_data, IniPropertyCallback property_callback);

#endif