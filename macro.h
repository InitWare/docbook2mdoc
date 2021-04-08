/* $Id: macro.h,v 1.7 2019/05/01 17:20:47 schwarze Exp $ */
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

/*
 * The interface of the macro line formatter,
 * a part of the mdoc(7) formatter.
 */

enum	linestate {
	LINE_NEW = 0,	/* At the beginning of a new line. */
	LINE_TEXT,	/* In the middle of a text line. */
	LINE_MACRO	/* In the middle of a macro line. */
};

enum	parastate {
	PARA_HAVE,	/* Just printed .Pp or equivalent. */
	PARA_MID,	/* In the middle of a paragraph. */
	PARA_WANT 	/* Need .Pp before printing anything else. */
};

struct	format {
	int		 level;      /* Header level, starting at 1. */
	int		 nofill;     /* Level of no-fill block nesting. */
	int		 flags;
#define	FMT_NOSPC	 (1 << 0)    /* Suppress space before next node. */
#define	FMT_ARG		 (1 << 1)    /* May add argument to current macro. */
#define	FMT_CHILD	 (1 << 2)    /* Expect a single child macro. */
#define	FMT_IMPL	 (1 << 3)    /* Partial implicit block is open. */
	enum linestate	 linestate;
	enum parastate	 parastate;
};

#define	ARG_SPACE	1  /* Insert whitespace before this argument. */
#define	ARG_SINGLE	2  /* Quote argument if it contains whitespace. */
#define	ARG_QUOTED	4  /* We are already in a quoted argument. */
#define	ARG_UPPER	8  /* Covert argument to upper case. */


void	 macro_open(struct format *, const char *);
void	 macro_close(struct format *);
void	 macro_line(struct format *, const char *);

void	 macro_addarg(struct format *, const char *, int);
void	 macro_argline(struct format *, const char *, const char *);
void	 macro_addnode(struct format *, struct pnode *, int);
void	 macro_nodeline(struct format *, const char *, struct pnode *, int);

void	 para_check(struct format *);
void	 print_text(struct format *, const char *, int);
void	 print_textnode(struct format *, struct pnode *);
