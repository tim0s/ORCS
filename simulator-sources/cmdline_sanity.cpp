#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cgraph.h>
#include <limits.h>
#include "simulator.hpp"
#include "cmdline.h"

/* --------------------------------------------------------------------------------
 * How the ptrnargs argument works.
 * --------------------------------------------------------------------------------
 * When we have a pattern that needs some user input in order to run, the ptrnargs
 * command line argument is used to provide this input. Since different patterns
 * may need different types of arguments (int, double, specially formatted strings
 * etc), we need to ensure that the correct input has been provided by the user
 * before we try to run the pattern.
 *
 * All the patterns that need a pattern argument to be provided should provide a
 * help/usage message to the user. The help message can be viewed when the user
 * calls the simulator with the given pattern, and uses the string 'help' as the
 * pattern argument.
 *
 * For example, the pattern 'neighbor' requires from the user to provide
 * a valid integer greater than 0. The help message for the 'neighbor'
 * pattern can be viewed if orcs is called like this:
 *    ./orcs -i topology.dot -p neighbor -a help
 *
 * And it prints the following message:
 *    Usage: Pattern 'neighbor' requires an integer ptrnarg that is greater than 0.
 *
 * Once the pattern with a correct pattern argument is provided, the simulator
 * runs as normal. In the case of the 'neighbor' pattern example, the user may run
 * the simulator as follows:
 *    ./orcs -i topology.dot -p neighbor -a 2
 *
 *
 * --------------------------------------------------------------------------------
 * Steps to add a new pattern that require pattern arguments to be provided, and
 * program it correctly using the provided functions in this source file
 * --------------------------------------------------------------------------------
 * Once you add a new pattern that requires some pattern arguments to be provided,
 * there is a sieres of steps that needs to be taken:
 *
 *    1. Add a help message for the new pattern in the function print_ptrnarg_help.
 *
 *        Look at an existing implementation to see how the help message can be
 *        printed properly. The print_ptrnarg_help function is not only used to
 *        print a help message, but also to print an ERROR message if the user has
 *        not provided the correct type of argument needed by the given pattern
 *        e.g. a pattern requires a positive integer to be be provided, but the
 *        user provided a negative number or a string that contains letters.
 *
 *        The print_ptrnarg_help function will stop execution of the simulator, and
 *        return with an EXIT_FAILURE or EXIT_SUCCESS code. An EXIT_SUCCESS should
 *        be typically returned when the user has deliberately asked for help
 *        (--ptrnarg help), while an EXIT_FAILURE should be returned when the user
 *        has failed to provide the correct type of argument (other than help).
 *
 *        The success/failure exit behavior is determined by setting the parameter
 *        "error" of the function print_ptrnarg_help to false or true respectively.
 *
 *    2. Ensure that when the users wants to use the new pattern, a pattern
 *       argument IS provided.
 *
 *        The function perform_sanity_checks_in_args is used for performing this
 *        simple sanity check. Add a new "or'ed" (||) line like this:
 *            strcmp(ptrn, "your-pattern") == 0
 *        in the existing 'if' statement, and ensure that you have provided a
 *        help/error message as described in step 1 previously.
 *
 *    3. Parse and process the arguments.
 *
 *        Use the function process_ptrnargs to parse and process the provided
 *        pattern arguments. The function process_ptrnargs is responsible to
 *        provide some additional sanity checks if it cannot parse the pattern
 *        arguments as expected, or if the arguments have been parsed but are
 *        not within the expected limits, and call the print_ptrnarg_help function
 *        with the "error" parameter set to true. When the pattern arguments are
 *        correctly parsed and within the expected limits, memory has to be
 *        malloc'ed and store the args. At last, the void pointer cmdargs->ptrnarg
 *        has to be pointed to the malloc'ed data.
 *
 *    4. Implement a cleanup function.
 *
 *        Since we allocate some heap memory in step 3, we need to deallocate the
 *        memory before the simulator exits. The cleanup_args function is called
 *        as the very last function in the driver.cpp. Ensure that you have added
 *        the necessary cleanup steps for the given pattern in the cleanup_args
 *        function. The cleanup can be as simple as just a free(cmdargs->ptrnarg)
 *        line (as in the case of the 'neighbor' pattern for example).
 */

/**
 * strtoi (string to int) behaves exactly as strtol or strtoul etc behave, however,
 * the function checks if the provided number is within the INT limits.
 */
static int strtoi(IN const char *s,
				  IN OUT char **endptr,
				  IN int base) {
	long retnum;

	retnum = strtoul(s, endptr, base);
	if ((retnum > INT_MIN) && (retnum < INT_MAX))
		return (int)retnum;

	*endptr = (char *)s;
	return -1;
}

/**
 * print_ptrnarg_help function prints pattern-specific help when ptrnarg parameter
 * is used with the string "help". Read the comments in the top of this source
 * file to understand how to use this function.
 */
static void print_ptrnarg_help(IN char *ptrn,
							   IN char *ptrnarg,
							   IN bool error = false) {

	if ((strcmp(ptrn, "neighbor") == 0 ||
		 strcmp(ptrn, "receivers") == 0))
		/* Prints an INT Required usage/error message */
		printf("\n%s: Pattern '%s' requires an integer ptrnarg that is greater than 0. %s%s%s%s\n",
			   error ? "ERROR" : "Usage", ptrn,
			   ptrnarg ? "'" : "",
			   ptrnarg ? ptrnarg : "",
			   ptrnarg ? "' " : "",
			   ptrnarg ? "provided." : "");

	if (error)
		exit(EXIT_FAILURE);
	else
		exit(EXIT_SUCCESS);
}

/**
 * The function process_ptrnargs processes the provided pattern arguments and
 * allocates memory to store the data in the necessary data structs
 */
static void process_ptrnargs(IN char *ptrn,
							 IN char *ptrnarg,
							 IN OUT cmdargs_t *cmdargs) {
	char *next_num;

	/* If the pattern argument is "help", just print the help
	 * message corresponding to that ptrn and exit. */
	if (strcmp(ptrnarg, "help") == 0)
		print_ptrnarg_help(ptrn, NULL);

	/* If the pattern argument != "help", check which pattern
	 * we are currently processing, and parse the pattern args
	 * as necessary */
	if ((strcmp(ptrn, "neighbor") == 0 ||
		 strcmp(ptrn, "receivers") == 0)) {
		/* For neighbor or receivers pattern, the
		 * pattern argument must be an integer */
		int *ptrnarg_i = (int *)malloc(sizeof(*ptrnarg_i));
		if (ptrnarg_i == NULL)
			goto exit;

		*ptrnarg_i = strtoi(ptrnarg, &next_num, 10);
		if (strlen(next_num) != 0 || *ptrnarg_i < 1) {
			free(ptrnarg_i);
			print_ptrnarg_help(ptrn, ptrnarg, true);
		}

		cmdargs->ptrnarg = (void*)ptrnarg_i;
	} else if (strcmp(ptrn, "ptrnvsptrn") == 0) {
		/* For ptrnvsptrn communication the pattern argument
		 * must be a string of this format:
		 *	   ptrn1(:arg1)?,ptrn2(:arg2)?
		 */
	}

	return;

exit:
	printf("ERROR: Could not allocate memory\n");
	exit(EXIT_FAILURE);
}

/**
 * The function perform_sanity_checks_in_args does some basic sanity
 * checks and is called once in the beginning of the driver.cpp file,
 * right after the cmdline_parser has been called.
 */
void perform_sanity_checks_in_args(IN OUT cmdargs_t *cmdargs) {

	char *ptrn = cmdargs->args_info.ptrn_arg;
	char *ptrnarg = cmdargs->args_info.ptrnarg_arg;

	cmdargs->ptrnarg = NULL;

	/* First check the pattern name, and if it needs a mandatory
	 * pattern argument that hasn't been provided, warn and exit. */
	if ((strcmp(ptrn, "neighbor") == 0 ||
		 strcmp(ptrn, "receivers") == 0) &&
			ptrnarg == NULL)
		print_ptrnarg_help(ptrn, NULL, true);

	/* Ensure that if a pattern argument is provided, it has been
	 * provided in the correct format/type needed by the chosen pattern */
	process_ptrnargs(ptrn, ptrnarg, cmdargs);
}

/**
 * The cleanup_args function takes care of cleaning the memory that has
 * been potentially allocated in heap by the process_ptrnargs function.
 */
void cleanup_args(IN cmdargs_t *cmdargs) {

	char *ptrn = cmdargs->args_info.ptrn_arg;

	if ((strcmp(ptrn, "neighbor") == 0 ||
		 strcmp(ptrn, "receivers") == 0))
		free(cmdargs->ptrnarg);
}
