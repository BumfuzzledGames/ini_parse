#ifndef SPAN_H
#define SPAN_H

typedef struct Span {
	char *start;
	char *end;
} Span;

typedef struct ConstSpan {
	const char *start;
	const char *end;
} ConstSpan;

#endif