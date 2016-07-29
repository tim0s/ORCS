#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cgraph.h>
#include <limits.h>
#include "simulator.hpp"
#include "cmdline.h"
#include <regex.h>
#include <mpi.h>

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
 *        Look at the existing simple implementation of the "neighbor" pattern,
 *        that parses only an integer, or for more advanced implementations with
 *        regular expressions and multiple arguments (some of them optional), you
 *        can look at the implementation of the receivers or ptrnvsptrn patterns.
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
                               IN int my_mpi_rank,
                               IN bool error = false,
                               IN bool terminate_prog = true) {

	if (my_mpi_rank == 0) {

		fprintf(stderr, "\n%s: ", error ? "ERROR" : "Usage");

		if (strcmp(ptrn, "neighbor") == 0) {
			/* Prints an INT Required usage/error message */
			fprintf(stderr, "Pattern '%s' requires an integer ptrnarg that is greater than 0.\n", ptrn);
		} else if (strcmp(ptrn, "recvs_one_src") == 0 ||
		           strcmp(ptrn, "recvs_all_src") == 0) {
			fprintf(stderr, "Pattern '%s' requires a ptrnarg in the following format:\n"
			        "         <num_receivers>[,<chance_factor_1>[,<chance_factor_2>]][,choose_sender_mode]\n"
			        "\n"
			        "%s\n"
			        "       The receivers that the source nodes choose to send traffic to, are always chosen based on the\n"
			        "         source node's 'linear-picking-index' mod 'num_receivers'.\n"
			        "\n"
			        "       The 'num_receivers' arg is a mandatory integer number greater than zero, and defines the number\n"
			        "         of receivers that will be used in the experiment.\n"
			        "       The 'chance_factor_1' arg is an optional percentage (accepts values between 0.0 and 1.0) and defines\n"
			        "         a chance that a chosen source node will have to communicate with a receiver in the pattern. If no\n"
			        "         chance_factor_1 is provided, the chance_factor_1 is set to 1.0, and the chosen source nodes will\n"
			        "         always communicate with a receiver.\n"
			        "       The 'chance_factor_2' arg is another optional percentage (accepts values between 0.0 and 1.0) and\n"
			        "         defines the chance that if a chosen source node is decided that will not communicate with a\n"
			        "         receiver (based on chance_factor_1), there is a chance that it will stay idle (i.e. not communicate\n"
			        "         at all with any other node). If the chance_factor_1 is set to 1.0, the chance_factor_2 will have no\n"
			        "         effect in the experiment. The chance_factor_2 is set to 0.0 by default, i.e. there are no idle nodes.\n"
			        "       The 'choose_sender_mode' arg can accept either the value 'rand' or 'linear', and will define if the\n"
			        "         sender nodes that send traffic to the receivers will be chosen randomly or linearly. The default\n"
			        "         value is 'rand'.",
			        ptrn, (strcmp(ptrn, "recvs_one_src") == 0 ) ?
			            "       The pattern recvs_one_src will choose only one sender per receiver node."
			          :
			            "       The pattern recvs_all_src will use all non-receivers nodes as senders"
			            " towards the receiver nodes.");
		} else if (strcmp(ptrn, "ptrnvsptrn") == 0) {
			fprintf(stderr, "Pattern '%s' requires a string ptrnarg in the following format:\n"
			        "         <pattern1>[:<arg2>]::<pattern2>[:<arg2>]\n"
			        "         \n"
			        "       The args ('arg1' and/or 'arg2') are optional, and should only be provided if the used patterns\n"
			        "        need an argument. All of the available patterns except 'ptrnvsptrn' can be used for either\n"
			        "        'pattern1' or 'pattern2'.\n", ptrn);
		}

		fprintf(stderr, "%s%s%s%s\n",
		        ptrnarg ? "\nPattern argument '" : "",
		        ptrnarg ? ptrnarg : "",
		        ptrnarg ? "' " : "",
		        ptrnarg ? "provided." : "");
	}

	if (terminate_prog) {
		MPI_Finalize();
		if (error)
			exit(EXIT_FAILURE);
		else
			exit(EXIT_SUCCESS);
	}
}

/**
 * The function process_ptrnargs processes the provided pattern arguments and
 * allocates memory to store the data in the necessary data structs
 */
static void process_ptrnargs(IN char *ptrn,
                             IN char *ptrnarg,
                             IN OUT cmdargs_t *cmdargs,
                             IN int my_mpi_rank) {

	int max_ptrn_size;

	if (strcmp(ptrn, "ptrnvsptrn") == 0)
		max_ptrn_size = MAX_PTRNVSPTRN_ARG_SIZE;
	else
		max_ptrn_size = MAX_ARG_SIZE;

	/* Ensure that the user is not providing a string larger than what we can handle */
	if (strlen(ptrnarg) > max_ptrn_size) {
		if (my_mpi_rank == 0)
			fprintf(stderr, "ERROR: The max accepted arg size for ptrn '%s' is %d\n", ptrn, max_ptrn_size);
		MPI_Finalize();
		exit(EXIT_FAILURE);
	}

	/* If the pattern argument is "help", just print the help
	 * message corresponding to that ptrn and exit.
	 * TODO: Implement a help message for each one of the patterns, because
	 *       at the moment if a user calls, for example, the rand pattern with
	 *       'help' ptrnarg, the simulator will just exit without printing
	 *       anything.
	 */
	if (strcmp(ptrnarg, "help") == 0)
		print_ptrnarg_help(ptrn, NULL, my_mpi_rank);

	/* If the pattern argument != "help", check which pattern
	 * we are currently processing, and parse the pattern args
	 * as necessary */
	if (strcmp(ptrn, "neighbor") == 0) {

		/** ****************************************************************
		 * For the neighbor pattern, the pattern argument must be
		 * an integer greater than zero.
		 *******************************************************************/

		char *next_num;

		int *ptrnarg_i = (int *) malloc(sizeof(*ptrnarg_i));
		if (ptrnarg_i == NULL)
			goto exit;

		*ptrnarg_i = strtoi(ptrnarg, &next_num, 10);
		if (strlen(next_num) != 0 || *ptrnarg_i < 1) {
			free(ptrnarg_i);
			print_ptrnarg_help(ptrn, ptrnarg, my_mpi_rank, true);
		}

		cmdargs->ptrnarg = (void *)ptrnarg_i;

	} else if (strcmp(ptrn, "recvs_one_src") == 0 ||
	           strcmp(ptrn, "recvs_all_src") == 0) {

		/** ****************************************************************
		 * For the receivers pattern, the pattern argument must be a
		 * string in this format:
		 *     integer[,double]
		 *
		 * That is, a mandatory integer number greater than zero must
		 * be provided, and an optional percentage that is following
		 * after a comma, between 0.0 and 1.0 can be provided if the
		 * user wants to have source nodes sending traffic to the
		 * receiver based on a chance factor.
		 *******************************************************************/

		regex_t regex;
		const int numGroups = 8;
		regmatch_t matchedGroups[numGroups];
		int ret, g, start_pos, end_pos;
		char *cursor = ptrnarg;
		char match[MAX_ARG_SIZE];

		int num_receivers;
		double process_a_chance;
		receivers_t *receivers_args = (receivers_t *) malloc(sizeof(*receivers_args));
		if (receivers_args == NULL)
			goto exit;

		/* Default -1 indicates that the chance_to_send_to_a_receiver hasn't been provided by the user */
		receivers_args->chance_to_communicate_with_a_receiver = 1.0;
		receivers_args->chance_to_not_communicate_at_all = 0.0;
		strcpy(receivers_args->choose_src_method, "rand");

		/* Why I use double backslash to escape the 'dot': http://stackoverflow.com/a/18477178/1275161 */
		//"^([[:digit:]]+)(,([-+]?[0-9]*\\.?[0-9]+))?(,([-+]?[0-9]*\\.?[0-9]+))?$"
		ret = regcomp(&regex, "^([[:digit:]]+)(,([-+]?[0-9]*\\.?[0-9]+))?(,([-+]?[0-9]*\\.?[0-9]+))?(,(rand|linear))?$", REG_EXTENDED);
		if (ret) {
			if (my_mpi_rank == 0)
				fprintf(stderr, "Could not compile regex\n");
			free(receivers_args);
			MPI_Finalize();
			exit(EXIT_FAILURE);
		}

		ret = regexec(&regex, cursor, numGroups, matchedGroups, 0);
		if (ret)
			/* No match, so print the error message and exit */
			print_ptrnarg_help(ptrn, ptrnarg, my_mpi_rank, true);

		for (g = 0; g < numGroups; g++) {

			if (matchedGroups[g].rm_so == -1)
				continue;

			start_pos = matchedGroups[g].rm_so;
			end_pos = matchedGroups[g].rm_eo;

			//printf("start: %d, finish: %d, rm_so: %d, rm_eo: %d\n", start_pos, end_pos,
			//	   matchedGroups[g].rm_so, matchedGroups[g].rm_eo);

			memset(match, 0, sizeof(match));
			strncpy(match, cursor + start_pos,
			        (end_pos - start_pos) < MAX_ARG_SIZE ?
			            end_pos - start_pos : MAX_ARG_SIZE);
			match[MAX_ARG_SIZE - 1] = 0;

			//printf("Matching now: '%s', cursor: '%s'\n", match, cursor);

			if (g == 0) {
				/* Check if the complete matched string is the same as the ptrnarg. */
				if (strncmp(ptrnarg, match, strlen(match)) != 0) {
					//printf("here: %s, %s, %d\n", ptrnarg, match, strlen(match));
					/* The complete string provided by the user should match exactly.
					 * Otherwise print an error message and exit. */
					print_ptrnarg_help(ptrn, ptrnarg, my_mpi_rank, true);
				}
			} else {
				char *next_num;

				switch(g) {
					case 1:
						/* In the first group we must capture an integer */
						num_receivers = strtoi(match, &next_num, 10);
						if (strlen(next_num) != 0 || num_receivers < 1) {
							free(receivers_args);
							print_ptrnarg_help(ptrn, ptrnarg, my_mpi_rank, true);
						}
						receivers_args->num_receivers = num_receivers;
						break;
					case 3:
					case 5:
						/* In the third and fifth group we must capture a floating
						 * point number between 0 and 1 */
						process_a_chance = strtod(match, &next_num);
						if (strlen(next_num) != 0 ||
						        (process_a_chance < 0 ||
						         process_a_chance > 1)) {
							free(receivers_args);
							print_ptrnarg_help(ptrn, ptrnarg, my_mpi_rank, true);
						}
						if (g == 3)
							receivers_args->chance_to_communicate_with_a_receiver = process_a_chance;
						else if (g == 5)
							receivers_args->chance_to_not_communicate_at_all = process_a_chance;
						break;
					case 7:
						strncpy(receivers_args->choose_src_method, match,
						        sizeof(receivers_args->choose_src_method));
						receivers_args->choose_src_method[sizeof(receivers_args->choose_src_method) - 1] = '\0';
						break;
				}
			}
		}

		regfree(&regex);

		cmdargs->ptrnarg = (void *)receivers_args;

	} else if (strcmp(ptrn, "ptrnvsptrn") == 0) {

		/** ****************************************************************
		 * For ptrnvsptrn communication the pattern argument
		 * must be a string of this format:
		 *	   ptrn1(:arg1)?,ptrn2(:arg2)?
		 *******************************************************************/

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
		int ret, i, g, start_pos, end_pos;
		char *cursor = ptrnarg;

		char complete_match[MAX_PTRNVSPTRN_ARG_SIZE], *cur_match;
		ptrnvsptrn_t *ptrnvsptrn = (ptrnvsptrn_t *)calloc(1, sizeof(*ptrnvsptrn));	/* Contains the extracted data. */
		if (ptrnvsptrn == NULL)
			goto exit;

		memset(complete_match, 0, sizeof(complete_match));

		/* Compile the regex first */
		/* 1) ^                     <- Matches beginning of the line."
		 * 2) ([^:[:blank:]]+)      <- Matches one or more non-space, non-colon characters"
		 * 3) (:([^:[:blank:]]+))?  <- An optional string follows that starts with a colon, and follows the rules of 2 (right above)
		 * 4) ::                    <- A double colon must follow (this is the separator of the two patterns
		 * 5)                       <- Steps 2,3 are repeated
		 * 6) $                     <- until the end of line is reached.
		 *
		 * This regex will match strings like the following and nothing else:
		 *     ptrn1:arg1::ptrn2:arg2
		 *     ptrn1:arg1::ptrn2
		 *     ptrn1::ptrn2:arg2
		 *     ptrn1::ptrn2
		 */
		ret = regcomp(&regex, "^([^:[:blank:]]+)(:([^:[:blank:]]+))?::([^:[:blank:]]+)(:([^:[:blank:]]+))?$", REG_EXTENDED);
		if (ret) {
			if (my_mpi_rank == 0)
				fprintf(stderr, "Could not compile regex\n");
			free(ptrnvsptrn);
			MPI_Finalize();
			exit(EXIT_FAILURE);
		}

		/* Then search for a match. The regexec starts searching for a match from
		 * the position where the cursor points at */
		ret = regexec(&regex, cursor, numGroups, matchedGroups, 0);
		if (ret)
			/* No match, so print the error message and exit */
			print_ptrnarg_help(ptrn, ptrnarg, my_mpi_rank, true);

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

			//printf("start: %d, finish: %d, rm_so: %d, rm_eo: %d\n", start_pos, end_pos,
			//	   matchedGroups[g].rm_so, matchedGroups[g].rm_eo);

			if (g == 0) {
				/* Group 0 stores information about the complete match.
				 * Copy the complete matched string in the complete_match array.
				 * Perform some bound checks. */
				strncpy(complete_match, cursor,
				        (end_pos - start_pos) < MAX_PTRNVSPTRN_ARG_SIZE ?
				            end_pos - start_pos : MAX_PTRNVSPTRN_ARG_SIZE);
				complete_match[MAX_PTRNVSPTRN_ARG_SIZE - 1] = 0;

				/* Check if the complete matched string is the same as the ptrnarg. */
				if (strncmp(ptrnarg, complete_match, strlen(complete_match)) != 0) {
					//printf("here: %s, %s, %d\n", ptrnarg, complete_match, strlen(complete_match));
					/* The complete string provided by the user should match exactly.
					 * Otherwise print an error message and exit. */
					print_ptrnarg_help(ptrn, ptrnarg, my_mpi_rank, true);
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
		bool unknown_ptrn1 = false, unknown_ptrn2 = false;
		if (check_if_ptrn_available_for_ptrnvsptrn(ptrnvsptrn->ptrn1) == -1)
			unknown_ptrn1 = true;
		if (check_if_ptrn_available_for_ptrnvsptrn(ptrnvsptrn->ptrn2) == -1)
			unknown_ptrn2 = true;

		if (unknown_ptrn1 || unknown_ptrn2) {
			print_ptrnarg_help(ptrn, ptrnarg, my_mpi_rank, true, false);

			if (my_mpi_rank == 0) {
				printf("\n"
				       "-------------------------------\n"
				       "Unknown pattern%s: %s%s%s\n"
				       "-------------------------------\n",
				       unknown_ptrn1 && unknown_ptrn2 ? "s" : "",
				       unknown_ptrn1 ? ptrnvsptrn->ptrn1 : "",
				       unknown_ptrn1 && unknown_ptrn2 ? ", " : "",
				       unknown_ptrn2 ? ptrnvsptrn->ptrn2 : "");

				/* Print available patterns */
				printf("\nAvailable patterns are:\n");
				for (i = 0; cmdline_parser_ptrn_values[i]; i++) {
					if (strcmp(cmdline_parser_ptrn_values[i], "ptrnvsptrn") != 0)
						printf("     %s\n", cmdline_parser_ptrn_values[i]);
				}
			}
			free(ptrnvsptrn);
			MPI_Finalize();
			exit(EXIT_FAILURE);
		}

		/* Use some temporary cmdargs variables, and call the function process_ptrnargs
		 * recursively for each of the provided patterns. The function process_ptrnargs
		 * will perform the necessary type-sanity checks for each of the provided
		 * patterns */
		cmdargs_t cmdargs_ptrn1, cmdargs_ptrn2;
		cmdargs_ptrn1.args_info = cmdargs->args_info;
		cmdargs_ptrn2.args_info = cmdargs->args_info;

		process_ptrnargs(ptrnvsptrn->ptrn1, ptrnvsptrn->ptrnargstr1, &cmdargs_ptrn1, my_mpi_rank);
		process_ptrnargs(ptrnvsptrn->ptrn2, ptrnvsptrn->ptrnargstr2, &cmdargs_ptrn2, my_mpi_rank);

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
	if (my_mpi_rank == 0)
		fprintf(stderr, "ERROR: Could not allocate memory\n");
	MPI_Finalize();
	exit(EXIT_FAILURE);
}

/**
 * The function perform_sanity_checks_in_args does some basic sanity
 * checks and is called once in the beginning of the driver.cpp file,
 * right after the cmdline_parser has been called.
 */
void perform_sanity_checks_in_args(IN OUT cmdargs_t *cmdargs,
                                   IN int my_mpi_rank) {

	char *ptrn = cmdargs->args_info.ptrn_arg;
	char *ptrnarg = cmdargs->args_info.ptrnarg_arg;

	if (strcmp(cmdargs->args_info.part_subset_arg, "none") != 0) {
		if (strcmp(ptrn, "ptrnvsptrn") != 0) {
			if (my_mpi_rank == 0)
				fprintf(stderr, "ERROR: The 'part_subset' option can only be used with 'ptrnvsptrn' pattern.\n");
			MPI_Finalize();
			exit(EXIT_FAILURE);
		}
	}

	/* Check the pattern name, and if the chosen pattern needs a
	 * mandatory pattern argument that hasn't been provided, warn
	 * and exit. */
	if ((strcmp(ptrn, "neighbor") == 0 ||
	     strcmp(ptrn, "recvs_one_src") == 0 ||
	     strcmp(ptrn, "recvs_all_src") == 0 ||
	     strcmp(ptrn, "ptrnvsptrn") == 0) &&
	        ptrnarg == NULL)
		print_ptrnarg_help(ptrn, NULL, my_mpi_rank, true);

	/* Ensure that if a pattern argument is provided, it has been
	 * provided in the correct format/type needed by the chosen pattern */
	if (ptrnarg != NULL)
		process_ptrnargs(ptrn, ptrnarg, cmdargs, my_mpi_rank);
}

/**
 * The cleanup_args function takes care of cleaning the memory that has
 * been potentially allocated in heap by the process_ptrnargs function.
 * It is called, at least, at the last line in driver.cpp file.
 */
void cleanup_args(IN char *ptrn, IN void *ptrnarg) {

	if ((strcmp(ptrn, "neighbor") == 0 ||
	     strcmp(ptrn, "recvs_one_src") == 0 ||
	     strcmp(ptrn, "recvs_all_src") == 0))
		free(ptrnarg);
	else if (strcmp(ptrn, "ptrnvsptrn") == 0) {
		ptrnvsptrn_t *ptrnvsptrn = (ptrnvsptrn_t *)ptrnarg;
		cleanup_args(ptrnvsptrn->ptrn1, ptrnvsptrn->ptrnarg1);
		cleanup_args(ptrnvsptrn->ptrn2, ptrnvsptrn->ptrnarg2);
		free(ptrnvsptrn);
	}
}
