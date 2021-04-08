/* $Id: main.c,v 1.10 2019/05/01 09:02:25 schwarze Exp $ */
/*
 * Copyright (c) 2014 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "node.h"
#include "parse.h"
#include "reorg.h"
#include "format.h"

/*
 * The steering function of the docbook2mdoc(1) program.
 */

enum	outt {
	OUTT_MDOC = 0,
	OUTT_TREE,
	OUTT_LINT
};

int
main(int argc, char *argv[])
{
	struct parse	*parser;
	struct ptree	*tree;
	const char	*progname;
	const char	*bname, *fname, *sec;
	int		 ch, fd, rc, warn;
	enum outt	 outtype;

	if ((progname = strrchr(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		progname++;

	sec = NULL;
	warn = 0;
	outtype = OUTT_MDOC;
	while ((ch = getopt(argc, argv, "s:T:W")) != -1) {
		switch (ch) {
		case 's':
			sec = optarg;
			break;
		case 'T':
			if (strcmp(optarg, "mdoc") == 0)
				outtype = OUTT_MDOC;
			else if (strcmp(optarg, "tree") == 0)
				outtype = OUTT_TREE;
			else if (strcmp(optarg, "lint") == 0)
				outtype = OUTT_LINT;
			else {
				fprintf(stderr, "%s: Bad argument\n",
				    optarg);
				goto usage;
			}
			break;
		case 'W':
			warn = 1;
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;

	/*
	 * Argument processing:
	 * Open file or use standard input.
	 */

	if (argc > 1) {
		fprintf(stderr, "%s: Too many arguments\n", argv[1]);
		goto usage;
	} else if (argc == 1) {
		fname = argv[0];
		fd = -1;
	} else {
		fname = "<stdin>";
		fd = STDIN_FILENO;
	}

	/* Parse. */

	parser = parse_alloc(warn);
	tree = parse_file(parser, fd, fname);
	ptree_reorg(tree, sec);
	rc = tree->flags & TREE_ERROR ? 3 : tree->flags & TREE_WARN ? 2 : 0;

	/* Format. */

	if (outtype != OUTT_LINT && tree->root != NULL) {
		if (rc > 2)
			fputc('\n', stderr);
		if (outtype == OUTT_MDOC) {
			if (fd == -1 && (bname = basename(fname)) != NULL)
				printf(".\\\" automatically generated "
				    "with %s %s\n", progname, bname);
			ptree_print_mdoc(tree);
		} else
			ptree_print_tree(tree);
		if (rc > 2)
			fputs("\nThe output may be incomplete, see the "
			    "parse error reported above.\n\n", stderr);
	}
	parse_free(parser);
	return rc;

usage:
	fprintf(stderr, "usage: %s [-W] [-s section] "
	    "[-T mdoc | tree | lint] [input_filename]\n", progname);
	return 5;
}
