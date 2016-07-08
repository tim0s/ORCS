#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cgraph.h>
#include <limits.h>
#include "simulator.hpp"
#include "cmdline.h"

static int strtoi(const char *s, char **endptr, int base) {
	long retnum;

	retnum = strtoul(s, endptr, base);
	if ((retnum > INT_MIN) && (retnum < INT_MAX))
		return (int)retnum;

	*endptr = (char *)s;
	return -1;
}

void perform_sanity_checks_in_args(IN OUT cmdargs_t *cmdargs) {

	char *ptrn = cmdargs->args_info.ptrn_arg;

	cmdargs->ptrnarg = NULL;

	/* First check the pattern name, and if it needs a mandatory
	 * pattern argument that hasn't been provided, warn and exit. */
	if ((strcmp(ptrn, "neighbor") == 0 ||
		 strcmp(ptrn, "receivers") == 0) &&
			!cmdargs->args_info.ptrnarg_arg) {
		printf("ERROR: Pattern '%s' requires a positive integer pattern argument.", ptrn);
		exit(EXIT_FAILURE);
	}

	/* Ensure that if a pattern argument is provided, it has been
	 * provided in the correct format needed by the chosen pattern */
	if (cmdargs->args_info.ptrnarg_arg) {
		char *ptrnarg, *next_num;

		ptrnarg = cmdargs->args_info.ptrnarg_arg;

		if (strcmp(ptrn, "neighbor") == 0 || strcmp(ptrn, "receivers") == 0) {
			/* For neighbor or receivers pattern, the
			 * pattern argument must be an integer */
			int *ptrnarg_i = (int *)malloc(sizeof(int));
			if (!ptrnarg_i)
				goto exit;

			*ptrnarg_i = strtoi(ptrnarg, &next_num, 10);
			if (strlen(next_num) != 0 || *ptrnarg_i < 1) {
				printf("Pattern argument '%s' must be a number greater than 0 for pattern '%s'\n",
					   ptrnarg, ptrn);
				exit(EXIT_FAILURE);
			}

			cmdargs->ptrnarg = (void*)ptrnarg_i;
		} else if (strcmp(ptrn, "ptrnvsptrn") == 0) {
			/* For ptrnvsptrn communication the pattern argument
			 * must be a string of this format:
			 *	   ptrn1(:arg1)?,ptrn2(:arg2)?
			 */
		}
	}

	return;
exit:
	printf("ERROR: Could not allocate memory\n");
	exit(EXIT_FAILURE);
}
