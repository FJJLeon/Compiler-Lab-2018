#ifndef ESCAPE_H
#define ESCAPE_H

typedef struct escapeEntey_ *escapeEntry;

struct escapeEntey_ {
	int depth;
	bool *escape;
};

escapeEntry EscapeEntry(int d, bool *e);

void Esc_findEscape(A_exp exp);

#endif
