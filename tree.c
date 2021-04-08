/* $Id: tree.c,v 1.3 2019/04/24 18:38:02 schwarze Exp $ */
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
#include <stdio.h>

#include "node.h"
#include "format.h"

/*
 * The implementation of the parse tree dumper.
 */

void
print_node(struct pnode *n, int indent)
{
	struct pnode	*nc;
	struct pattr	*a;

	printf("%*s%c%s", indent, "",
	    (n->flags & NFLAG_LINE) ? '*' :
	    (n->flags & NFLAG_SPC) ? ' ' : '-',
	    pnode_name(n->node));
	if (n->b != NULL) {
		putchar(' ');
		fputs(n->b, stdout);
	}
	TAILQ_FOREACH(a, &n->attrq, child)
		printf(" %s='%s'", attrkey_name(a->key), attr_getval(a));
	putchar('\n');
	TAILQ_FOREACH(nc, &n->childq, child)
		print_node(nc, indent + 2);
}

void
ptree_print_tree(struct ptree *tree)
{
	print_node(tree->root, 0);
}
