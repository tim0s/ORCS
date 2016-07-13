#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cgraph.h>
#include <limits.h>
#include "simulator.hpp"
#include "cmdline.h"
#include <regex.h>

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
 *        function. The cleanup can be as simple as just a free(ptrnarg) line
 *        (as in the case of the 'neighbor' pattern for example).
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
 * The function check_possible_values returns the first index in the *values[]
 * array that the exact 'val' has been found. The function will return -1 if
 * there is no match. For both of the 'values' and 'values_to_exclude' arrays
 * the last element must be set to 0.
 */
int check_possible_values(IN const char *val,
                          IN const char *values[],
                          IN const char *values_to_exclude[]) {
	int i, j;
	size_t len;

	if (val == NULL)	/* otherwise strlen() crashes below */
		return -1;		/* -1 means no match */

	for (i = 0, len = strlen(val); values[i]; i++) {
		if (strncmp(val, values[i], len) == 0) {
			/* Found a match. Now ensure that this
			 * is an exact match */
			if (strlen(values[i]) == len) {
				/* It is an exact match. As long as the matched
				 * string is not included in the values_to_exclude
				 * array, return its index. */
				if (values_to_exclude != NULL) {
					for (j = 0; values_to_exclude[j]; j++) {
						if (strncmp(val, values_to_exclude[j], len) == 0) {
							if (strlen(values_to_exclude[j]) == len)
								/* val is included in the values_to_exclude
								 * array, so return a not matched
								 */
								return -1;
						}
					}
				}
				return i;	/* An exact match was found.
							 * no need to check more */
			}
		}
	}

	return -1; /* No matches */
}

/**
 * The function check_if_ptrn_available_for_ptrnvsptrn will check in the
 * list of available patterns if the ptrn exists. Since we use this function
 * to determine if a pattern can be passed as an argument to the ptrnvsptrn
 * function, we want to exclude the ptrnvsptrn from the list of the
 * available commands. Therefore, we use the array ptrns_to_ignore
 * in order to achieve that behaviour.
 */
int check_if_ptrn_available_for_ptrnvsptrn(const char *ptrn) {
	/* The last element in the ptrns_to_ignore array MUST be 0.
	 * We use this zero to dermine the size of the array dynamically. */
	const char *ptrns_to_ignore[] = {"ptrnvsptrn", 0};

	/* The cmdline_parser_ptrn_values array is generated by gengetops,
	 * and is defined in the cmdline.h header file. */
	return check_possible_values(ptrn, cmdline_parser_ptrn_values, ptrns_to_ignore);
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
		fprintf(stderr, "\n%s: Pattern '%s' requires an integer ptrnarg that is greater than 0. %s%s%s%s\n",
		        error ? "ERROR" : "Usage", ptrn,
		        ptrnarg ? "'" : "",
		        ptrnarg ? ptrnarg : "",
		        ptrnarg ? "' " : "",
		        ptrnarg ? "provided." : "");
	else if (strcmp(ptrn, "ptrnvsptrn") == 0)
		fprintf(stderr, "\n%s: Pattern '%s' requires a string ptrnarg in the following format:\n"
		        "         pattern1:arg2,pattern2:arg2\n"
		        "         \n"
		        "       The args (arg1 and/or arg2) are optional, and should only be provided\n"
		        "       if the used patterns need an argument.\n"
		        "       All of the available patterns except 'ptrnvsptrn' can be used for either\n"
		        "       pattern1 or pattern2.\n"
		        "%s%s%s%s\n",
		        error ? "ERROR" : "Usage", ptrn,
		        ptrnarg ? "\nPattern argument '" : "",
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

	/* If the pattern argument is "help", just print the help
	 * message corresponding to that ptrn and exit.
	 * TODO: Implement a help message for each one of the patterns, because
	 *       at the moment if a user calls, for example, the rand pattern with
	 *       'help' ptrnarg, the simulator will just exit without printing
	 *       anything.
	 */
	if (strcmp(ptrnarg, "help") == 0)
		print_ptrnarg_help(ptrn, NULL);

	/* If the pattern argument != "help", check which pattern
	 * we are currently processing, and parse the pattern args
	 * as necessary */
	if ((strcmp(ptrn, "neighbor") == 0 ||
	     strcmp(ptrn, "receivers") == 0)) {

		/**
		 * For neighbor or receivers pattern, the
		 * pattern argument must be an integer
		 */

		char *next_num;

		int *ptrnarg_i = (int *)malloc(sizeof(*ptrnarg_i));
		if (ptrnarg_i == NULL)
			goto exit;

		*ptrnarg_i = strtoi(ptrnarg, &next_num, 10);
		if (strlen(next_num) != 0 || *ptrnarg_i < 1) {
			free(ptrnarg_i);
			print_ptrnarg_help(ptrn, ptrnarg, true);
		}

		cmdargs->ptrnarg = (void *)ptrnarg_i;

	} else if (strcmp(ptrn, "ptrnvsptrn") == 0) {

		/**
		 * For ptrnvsptrn communication the pattern argument
		 * must be a string of this format:
		 *	   ptrn1(:arg1)?,ptrn2(:arg2)?
		 */

		/* For some help with C regex's:
		 *    https://gist.github.com/ianmackinnon/3294587
		 *    http://www.lemoda.net/c/unix-regex/
		 *    https://www.freebsd.org/cgi/man.cgi?query=re_format&section=7
		 */
		regex_t regex; /* We will compile our regular expression in regex */
		const int numGroups = 7;  /* We expect a max of 4 groups in a single match: ptrn1, arg1, ptrn2, arg2
		                           * However, the regex matches once the complete string, plus some uneccessary
		                           * groups (I couldn't find how to exclude a match from group in C... ?: is
		                           * not working) that's why we choose numGroups = 7; */
		regmatch_t matchedGroups[numGroups]; /* Contains the matches found */
		int ret, g, start_pos, end_pos;
		char *cursor = ptrnarg;

		const int max_match_size = MAX_ARG_SIZE * 4 + 1;
		char complete_match[max_match_size], *cur_match;
		ptrnvsptrn_t *ptrnvsptrn = (ptrnvsptrn_t *)calloc(1, sizeof(*ptrnvsptrn));	/* Contains the extracted data. */
		if (ptrnvsptrn == NULL)
			goto exit;

		memset(complete_match, 0, sizeof(complete_match));

		/* Ensure that the user is not providing a string larger than what we can handle */
		if (strlen(ptrnarg) > max_match_size) {
			fprintf(stderr, "ERROR: The max accepted arg size is %d\n", max_match_size);
			exit(EXIT_FAILURE);
		}

		/* Compile the regex first */
		/* 1: ^                     <- Matches beginning of the line."
		 * 2: ([^:,[:blank:]]+)     <- Matches one or more non-space, non-colon, non-comma characters"
		 * 3: (:([^:,[:blank:]]+))? <- An optional string follows that starts with a colon, and follows the rules of 2 (right above)
		 * 4:,                      <- A comma must follow
		 * 5:                       <- Steps 2,3 are repeated
		 * 6:$                      <- until the end of line is reached.
		 *
		 * This regex will match strings like the following and nothing else:
		 *     ptrn1:arg1,ptrn2:arg2
		 *     ptrn1:arg1,ptrn2
		 *     ptrn1,ptrn2:arg2
		 *     ptrn1,ptrn2
		 */
		ret = regcomp(&regex, "^([^:,[:blank:]]+)(:([^:,[:blank:]]+))?,([^:,[:blank:]]+)(:([^:,[:blank:]]+))?$", REG_EXTENDED);
		if (ret) {
			fprintf(stderr, "Could not compile regex\n");
			exit(EXIT_FAILURE);
		}

		/* Then search for a match. The regexec starts searching for a match from
		 * the position where the cursor points at */
		ret = regexec(&regex, cursor, numGroups, matchedGroups, 0);
		if (ret)
			/* No match, so print the error message and exit */
			print_ptrnarg_help(ptrn, ptrnarg, true);

		/* We have matches, and information about those matches is stored in the
		 * matchedGroups array */
		for (g = 0; g < numGroups; g++) {

			/* If no match in the current group (since we accept optional arguments),
			 * continue with the next group */
			if (matchedGroups[g].rm_so == -1)
				continue;

			/* matchedGroups[g].rm_so returns a relative-to-the-begininning-of-match
			 * start offset for the matched group.
			 * matchedGroups[g].rm_eo returns a relative-to-the-begininning-of-match
			 * end offset for the matched group. */
			start_pos = matchedGroups[g].rm_so;
			end_pos = matchedGroups[g].rm_eo;

			//printf("start: %d, finish: %d, %d, %d\n", start_pos, end_pos,
			//	   matchedGroups[g].rm_so, matchedGroups[g].rm_eo);

			if (g == 0) {
				/* Group 0 stores information about the complete match.
				 * Copy the complete matched string in the complete_match array.
				 * Perform some bound checks. */
				strncpy(complete_match, cursor,
				        (end_pos - start_pos) < max_match_size ?
				            end_pos - start_pos : max_match_size);
				complete_match[max_match_size - 1] = 0;

				/* Check if the complete matched string is the same as the ptrnarg. */
				if (strncmp(ptrnarg, complete_match, strlen(complete_match)) != 0) {
					printf("here: %s, %s, %d\n", ptrnarg, complete_match, strlen(complete_match));
					/* The complete string provided by the user should match exactly.
					 * Otherwise print an error message and exit.
					 */
					print_ptrnarg_help(ptrn, ptrnarg, true);
				}

			} else {
				/* In this 'else' statement we run only for g > 1, so we are sure that
				 * the user has provided an string that is accepted by the regex, and
				 * we are ready to extract the needed information from the regex groups.
				 *
				 * From group 1 we extract ptrn1
				 * From group 3 we extract ptrnarg1 (optional)
				 * From group 4 we extract ptrn2
				 * From group 6 we extract ptrnarg2 (optional)*/
				switch(g) {
					case 1:
						cur_match = ptrnvsptrn->ptrn1; break;
					case 3:
						cur_match = ptrnvsptrn->ptrnargstr1; break;
					case 4:
						cur_match = ptrnvsptrn->ptrn2; break;
					case 6:
						cur_match = ptrnvsptrn->ptrnargstr2; break;
					default:
						cur_match = NULL;
				}

				/* Copy the matched group strings in the individual_match array.
				 * Perform some bound checks. */
				if (cur_match != NULL) {
					strncpy(cur_match, cursor + start_pos,
					        (end_pos - start_pos) < MAX_ARG_SIZE ?
					            end_pos - start_pos : MAX_ARG_SIZE);
					cur_match[MAX_ARG_SIZE - 1] = 0;
				}
			}

			//printf ("Matched in group %d: '%.*s' (bytes %d:%d)\n", g,
			//		(end_pos - start_pos), ptrnarg + start_pos,
			//		start_pos, end_pos);
		}

		regfree(&regex);

		/* Validate that the user has provided an known pattern */
		if ((check_if_ptrn_available_for_ptrnvsptrn(ptrnvsptrn->ptrn1) == -1 ||
		     check_if_ptrn_available_for_ptrnvsptrn(ptrnvsptrn->ptrn2) == -1))
			print_ptrnarg_help(ptrn, ptrnarg, true);

		/* Use some temporary cmdargs variables, and call the function process_ptrnargs
		 * recursively for each of the provided patterns. The function process_ptrnargs
		 * will perform the necessary type-sanity checks for each of the provided
		 * patterns */
		cmdargs_t cmdargs_ptrn1, cmdargs_ptrn2;
		cmdargs_ptrn1.args_info = cmdargs->args_info;
		cmdargs_ptrn2.args_info = cmdargs->args_info;

		process_ptrnargs(ptrnvsptrn->ptrn1, ptrnvsptrn->ptrnargstr1, &cmdargs_ptrn1);
		process_ptrnargs(ptrnvsptrn->ptrn2, ptrnvsptrn->ptrnargstr2, &cmdargs_ptrn2);

		/* If both process_ptrnargs lines returned successfully and we run at this line,
		 * both ptrn1 and ptrn2 have been provided with a correct ptrnarg (if any). Add
		 * the ptrnargs in the malloc'ed ptrnvsptrn struct and point the cmdargs->ptrnarg
		 * pointer to it. */
		ptrnvsptrn->ptrnarg1 = cmdargs_ptrn1.ptrnarg;
		ptrnvsptrn->ptrnarg2 = cmdargs_ptrn2.ptrnarg;

		cmdargs->ptrnarg = (void *)ptrnvsptrn;
	} else {

		/* If the pattern cannot accept a ptrnarg, just set the cmdargs->ptrnarg to NULL */
		cmdargs->ptrnarg = NULL;
	}

	return;

exit:
	fprintf(stderr, "ERROR: Could not allocate memory\n");
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

	/* First check the pattern name, and if it needs a mandatory
	 * pattern argument that hasn't been provided, warn and exit. */
	if ((strcmp(ptrn, "neighbor") == 0 ||
	     strcmp(ptrn, "receivers") == 0 ||
	     strcmp(ptrn, "ptrnvsptrn") == 0) &&
	        ptrnarg == NULL)
		print_ptrnarg_help(ptrn, NULL, true);

	/* Ensure that if a pattern argument is provided, it has been
	 * provided in the correct format/type needed by the chosen pattern */
	if (ptrnarg != NULL)
		process_ptrnargs(ptrn, ptrnarg, cmdargs);
}

/**
 * The cleanup_args function takes care of cleaning the memory that has
 * been potentially allocated in heap by the process_ptrnargs function.
 * It is called, at least, at the last line in driver.cpp file.
 */
void cleanup_args(IN char *ptrn, IN void *ptrnarg) {

	if ((strcmp(ptrn, "neighbor") == 0 ||
	     strcmp(ptrn, "receivers") == 0))
		free(ptrnarg);
	else if (strcmp(ptrn, "ptrnvsptrn") == 0) {
		ptrnvsptrn_t *ptrnvsptrn = (ptrnvsptrn_t *)ptrnarg;
		cleanup_args(ptrnvsptrn->ptrn1, ptrnvsptrn->ptrnarg1);
		cleanup_args(ptrnvsptrn->ptrn2, ptrnvsptrn->ptrnarg2);
		free(ptrnvsptrn);
	}
}
