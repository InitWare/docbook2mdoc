/* $Id: macro.c,v 1.20 2019/05/02 12:40:42 schwarze Exp $ */
/*
 * Copyright (c) 2019 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "node.h"
#include "macro.h"

/*
 * The implementation of the macro line formatter,
 * a part of the mdoc(7) formatter.
 */

void
para_check(struct format *f)
{
	if (f->parastate != PARA_WANT)
		return;
	if (f->linestate != LINE_NEW) {
		putchar('\n');
		f->linestate = LINE_NEW;
	}
	puts(".Pp");
	f->parastate = PARA_HAVE;
}

void
macro_open(struct format *f, const char *name)
{
	para_check(f);
	switch (f->linestate) {
	case LINE_MACRO:
		if (f->flags & FMT_NOSPC) {
			fputs(" Ns ", stdout);
			break;
		}
		if (f->nofill || f->flags & (FMT_CHILD | FMT_IMPL)) {
			putchar(' ');
			break;
		}
		/* FALLTHROUGH */
	case LINE_TEXT:
		if (f->nofill && f->linestate == LINE_TEXT)
			fputs(" \\c", stdout);
		putchar('\n');
		/* FALLTHROUGH */
	case LINE_NEW:
		putchar('.');
		f->linestate = LINE_MACRO;
		f->flags = 0;
		break;
	}
	fputs(name, stdout);
	f->flags &= FMT_IMPL;
	f->flags |= FMT_ARG;
	f->parastate = PARA_MID;
}

void
macro_close(struct format *f)
{
	if (f->linestate != LINE_NEW)
		putchar('\n');
	f->linestate = LINE_NEW;
	f->flags = 0;
}

void
macro_line(struct format *f, const char *name)
{
	macro_close(f);
	macro_open(f, name);
	macro_close(f);
}

/*
 * Print an argument string on a macro line, collapsing whitespace.
 */
void
macro_addarg(struct format *f, const char *arg, int flags)
{
	const char	*cp;
	int		 quote_now;

	assert(f->linestate == LINE_MACRO);

	/* Quote if requested and necessary. */

	quote_now = 0;
	if ((flags & (ARG_SINGLE | ARG_QUOTED)) == ARG_SINGLE) {
		for (cp = arg; *cp != '\0'; cp++)
			if (isspace((unsigned char)*cp))
				break;
		if (*cp != '\0') {
			if (flags & ARG_SPACE) {
				putchar(' ');
				flags &= ~ ARG_SPACE;
			}
			putchar('"');
			flags = ARG_QUOTED;
			quote_now = 1;
		}
	}

	for (cp = arg; *cp != '\0'; cp++) {

		/* Collapse whitespace. */

		if (isspace((unsigned char)*cp)) {
			flags |= ARG_SPACE;
			continue;
		} else if (flags & ARG_SPACE) {
			putchar(' ');
			flags &= ~ ARG_SPACE;
		}

		/* Escape us if we look like a macro. */

		if ((flags & (ARG_QUOTED | ARG_UPPER)) == 0 &&
		    (cp == arg || isspace((unsigned char)cp[-1])) &&
		    isupper((unsigned char)cp[0]) &&
		    islower((unsigned char)cp[1]) &&
		    (cp[2] == '\0' || cp[2] == ' ' ||
		     ((cp[3] == '\0' || cp[3] == ' ') &&
		      (strncmp(cp, "Brq", 3) == 0 ||
		       strncmp(cp, "Bro", 3) == 0 ||
		       strncmp(cp, "Brc", 3) == 0 ||
		       strncmp(cp, "Bsx", 3) == 0))))
			fputs("\\&", stdout);

		if (*cp == '"')
			fputs("\\(dq", stdout);
		else if (flags & ARG_UPPER)
			putchar(toupper((unsigned char)*cp));
		else
			putchar(*cp);
		if (*cp == '\\')
			putchar('e');
	}
	if (quote_now)
		putchar('"');
	f->parastate = PARA_MID;
}

void
macro_argline(struct format *f, const char *name, const char *arg)
{
	macro_open(f, name);
	macro_addarg(f, arg, ARG_SPACE);
	macro_close(f);
}

/*
 * Recursively append text from the children of a node to a macro line.
 */
void
macro_addnode(struct format *f, struct pnode *n, int flags)
{
	struct pnode	*nc;
	int		 is_text, quote_now;

	assert(f->linestate == LINE_MACRO);

	/*
	 * If this node or its only child is a text node, just add
	 * that text, letting macro_addarg() decide about quoting.
	 */

	while ((nc = TAILQ_FIRST(&n->childq)) != NULL &&
	    TAILQ_NEXT(nc, child) == NULL)
		n = nc;

	if (n->node == NODE_TEXT || n->node == NODE_ESCAPE) {
		macro_addarg(f, n->b, flags);
		f->parastate = PARA_MID;
		return;
	}

	/*
	 * If we want the argument quoted and are not already
	 * in a quoted context, quote now.
	 */

	quote_now = 0;
	if (flags & ARG_SINGLE) {
		if ((flags & ARG_QUOTED) == 0) {
			if (flags & ARG_SPACE) {
				putchar(' ');
				flags &= ~ARG_SPACE;
			}
			putchar('"');
			flags |= ARG_QUOTED;
			quote_now = 1;
		}
		flags &= ~ARG_SINGLE;
	}

	/*
	 * Iterate to child and sibling nodes,
	 * inserting whitespace between nodes.
	 */

	while (nc != NULL) {
		macro_addnode(f, nc, flags);
		is_text = pnode_class(nc->node) == CLASS_TEXT;
		nc = TAILQ_NEXT(nc, child);
		if (nc == NULL || pnode_class(nc->node) != CLASS_TEXT)
			is_text = 0;
		if (is_text && (nc->flags & NFLAG_SPC) == 0)
			flags &= ~ARG_SPACE;
		else
			flags |= ARG_SPACE;
	}
	if (quote_now)
		putchar('"');
	f->parastate = PARA_MID;
}

void
macro_nodeline(struct format *f, const char *name, struct pnode *n, int flags)
{
	macro_open(f, name);
	macro_addnode(f, n, ARG_SPACE | flags);
	macro_close(f);
}


/*
 * Print a word on the current text line if one is open, or on a new text
 * line otherwise.  The flag ARG_SPACE inserts spaces between words.
 */
void
print_text(struct format *f, const char *word, int flags)
{
	int	 ateos, inword;

	para_check(f);
	switch (f->linestate) {
	case LINE_NEW:
		break;
	case LINE_TEXT:
		if (flags & ARG_SPACE)
			putchar(' ');
		break;
	case LINE_MACRO:
		macro_close(f);
		break;
	}
	if (f->linestate == LINE_NEW && (*word == '.' || *word == '\''))
		fputs("\\&", stdout);
	ateos = inword = 0;
	while (*word != '\0') {
		if (f->nofill == 0) {
			switch (*word) {
			case ' ':
				if (ateos == 0) {
					inword = 0;
					break;
				}
				ateos = inword = 0;
				/* Handle the end of a sentence. */
				while (*word == ' ')
					word++;
				switch (*word) {
				case '\0':
					break;
				case '\'':
				case '.':
					fputs("\n\\&", stdout);
					break;
				default:
					putchar('\n');
					break;
				}
				continue;
			/* Detect the end of a sentence. */
			case '!':
			case '.':
			case '?':
				if (inword > 1 &&
				    (word[-2] != 'n' || word[-1] != 'c') &&
				    (word[-2] != 'v' || word[-1] != 's'))
					ateos = 1;
				/* FALLTHROUGH */
			case '"':
			case '\'':
			case ')':
			case ']':
				inword = 0;
				break;
			default:
				if (isalnum((unsigned char)*word))
					inword++;
				ateos = 0;
				break;
			}
		}
		putchar(*word);
		if (*word++ == '\\')
			putchar('e');
	}
	f->linestate = LINE_TEXT;
	f->parastate = PARA_MID;
	f->flags = 0;
}

/*
 * Recursively print the content of a node on a text line.
 */
void
print_textnode(struct format *f, struct pnode *n)
{
	struct pnode	*nc;

	if (n->node == NODE_TEXT || n->node == NODE_ESCAPE)
		print_text(f, n->b, ARG_SPACE);
	else
		TAILQ_FOREACH(nc, &n->childq, child)
			print_textnode(f, nc);
}
