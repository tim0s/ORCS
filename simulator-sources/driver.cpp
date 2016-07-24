/*
 * Copyright (c) 2009 The Trustees of Indiana University and Indiana
 *                    University Research and Technology
 *                    Corporation.  All rights reserved.
 *
 * Author(s): Timo Schneider <timoschn@cs.indiana.edu>
 *            Torsten Hoefler <htor@cs.indiana.edu>
 *
 */

#define MPICH_IGNORE_CXX_SEEK
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cgraph.h>
#include <mpi.h>
#include "pattern_generator.hpp"
#include "simulator.hpp"
#include "statistics.hpp"
#include "cmdline.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

Agraph_t *mygraph;

extern void perform_sanity_checks_in_args(IN OUT cmdargs_t *cmdargs,
                                          IN int my_mpi_rank);
extern void cleanup_args(IN char *ptrn, void *ptrnarg);

int main(int argc, char *argv[]) {
	
	// MPI variables, comm_rank and comm_size
	int mynode, allnodes;
	namelist_t namelist, part_namelist, complete_namelist, nodeorder_namelist, final_namelist;
	guidlist_t guidlist, part_guidlist, complete_guidlist, nodeorder_guidlist;
	int i, j;

	cmdargs_t cmdargs;

	my_mpi_init(&argc, &argv, &mynode, &allnodes);

	if (cmdline_parser(argc, argv, &cmdargs.args_info) != 0) {
		MPI_Finalize();
		exit(EXIT_FAILURE);
	}
	perform_sanity_checks_in_args(&cmdargs, mynode);

	if (cmdargs.args_info.getnumlevels_given) {
		int level = 0;
		while (1) {
			ptrn_t ptrn;
			
			genptrn_by_name(&ptrn, cmdargs.args_info.ptrn_arg, cmdargs.ptrnarg,
			                cmdargs.args_info.commsize_arg, cmdargs.args_info.part_commsize_arg,
			                level, mynode);
			if (ptrn.size() == 0) { break; }

			level++; //proceed to next level
		}
		if (mynode == 0)
			printf("The given input configuration would result in a %d-level simulation.\n", level);
		return level;
	}

	read_input_graph(cmdargs.args_info.input_file_arg, mynode);
	tag_edges(mygraph);

	/* Read the node ordering if provided */
	if (mynode == 0)
		read_node_ordering(cmdargs.args_info.node_ordering_file_arg,
						   &nodeorder_guidlist);

	/* Read the complete namelist and store it in a temporary vector */
	get_namelist_from_graph(&complete_namelist);

	/* The command line argument sanity checks "should" be placed in the cmdline_extended.cpp
	 * but here we need the get_namelist_from_graph to run first in order to perform the sanity
	 * check for the commsize and part_commsize, that is why we have these two here:
	 *
	 * Check that we have a sane commsize */
	if (complete_namelist.size() < 4) {

		if (mynode == 0)
			fprintf(stderr, "ERROR: The dot file you provided contains the less than four hosts.\n"
				    "       The simulator needs at least four hosts to run\n"
					"");
		MPI_Finalize();
		exit(EXIT_FAILURE);

	} else if (cmdargs.args_info.commsize_arg == 0) {

		cmdargs.args_info.commsize_arg = complete_namelist.size() - complete_namelist.size() % 2;

	} else if (cmdargs.args_info.commsize_arg < 4 ||
	           cmdargs.args_info.commsize_arg > complete_namelist.size()) {

		if (mynode == 0)
			fprintf(stderr, "ERROR: The communicator size (commsize) should be a number between '%d' and '%d'\n"
				    "       You provided '%d'.\n", 4, complete_namelist.size(), cmdargs.args_info.commsize_arg);
		MPI_Finalize();
		exit(EXIT_FAILURE);

	}

	/* Check that the we have a sane part_commsize (min 2, and always smaller than commsize) */
	if (cmdargs.args_info.part_commsize_arg < 2 ||
	        cmdargs.args_info.part_commsize_arg >= cmdargs.args_info.commsize_arg) {
		if (mynode == 0)
			fprintf(stderr, "ERROR: The first-part communicator size (part_commsize) should be a number between '%d' and '%d'\n"
				    "       You provided '%d'.\n", 2, cmdargs.args_info.commsize_arg - 1, cmdargs.args_info.part_commsize_arg);
		MPI_Finalize();
		exit(EXIT_FAILURE);
	}

	if(mynode == 0) { // print graph info

		print_commandline_options(stdout, &cmdargs);

		if (cmdargs.args_info.checkinputfile_given) {
			std::cout << "Number of hosts in the inputfile: " << complete_namelist.size() << "\n";
			std::cout << "Number of nodes in the inputfile: " << agnnodes(mygraph) << "\n";

			for (i = 0; i < complete_namelist.size(); i++) {
				for (j = 0; j < complete_namelist.size(); j++) {
					printf("Testing pair number %d of %d\n", i*complete_namelist.size()+j+1, complete_namelist.size()*complete_namelist.size());
					uroute_t r;
					find_route(&r, complete_namelist.at(i), complete_namelist.at(j));
				}
			}
			printf("Completed\n");
			return 0;
		}

		/* we have to use all hosts for the route quality assessment */
		if (cmdargs.args_info.routequal_given) cmdargs.args_info.commsize_arg = complete_namelist.size();
	}

	/* Get a list of all endpoint-names that we will work with from the dot-file
	 * This list, the namelist, may be a subset of the complete list. */
	if (mynode == 0)
		generate_namelist_by_name(cmdargs.args_info.subset_arg, &namelist,
		                          cmdargs.args_info.commsize_arg);

	/* If the part_subset_art is not "none", we need to process the part_subset */
	if (strcmp(cmdargs.args_info.part_subset_arg, "none") != 0) {
		/* The part_subset_arg can be 'linear_bfs' ONLY if the subset_arg is
		 * 'linear_bfs' as well. */
		if (strcmp(cmdargs.args_info.part_subset_arg, "linear_bfs") == 0 &&
				strcmp(cmdargs.args_info.subset_arg, "linear_bfs") != 0) {
			if (mynode == 0)
				fprintf(stderr, "ERROR: 'part_subset' can be 'linear_bfs' only if 'subset' is 'linear_bfs' as well.\n");
			MPI_Finalize();
			exit(EXIT_FAILURE);
		}

		/* Choose the part_subset from the namelist and store in the part_namelist */
		if (mynode == 0)
			generate_namelist_by_name(cmdargs.args_info.part_subset_arg, &part_namelist,
			                          cmdargs.args_info.part_commsize_arg, &namelist);
	}

	/* Distribute namelist, part_namelist and nodeorder_guidlist from root to all nodes */
	bcast_namelist(&namelist, mynode);
	if (strcmp(cmdargs.args_info.part_subset_arg, "none") != 0)
		bcast_namelist(&part_namelist, mynode);
	bcast_guidlist(&nodeorder_guidlist, mynode);

	if(mynode == 0) {
		std::cout << "   Number of hosts in the subset: " << namelist.size() << "\n";
		std::cout << "Number of nodes in the inputfile: " << agnnodes(mygraph) << "\n";
		std::cout << "Number of edges in the inputfile: " << agnedges(mygraph) << "\n";
	}

	/* Assess the quality of the routing table */
	if (cmdargs.args_info.routequal_given) {
		cable_cong_map_t cable_cong;
		int nconn = namelist.size()*namelist.size();
		
		int myn = namelist.size()/allnodes;
		int mystart = myn*mynode;
		if(mynode == allnodes-1) myn = namelist.size()-mystart;

		/* generate cable-congestion by all routes */
		for (i = mystart; i < mystart+myn; i++) {
			for (j = 0; j < namelist.size(); j++) {
				uroute_t route;
				find_route(&route, namelist.at(i), namelist.at(j));
				insert_route_into_cable_cong_map(&cable_cong, &route);
			}
		}
		/* allreduce cable_cong if parallel */
		if(allnodes > 1) allreduce_contig_int_map(&cable_cong);

		/* begin analysis */
		int iter = 0;
		const unsigned int maxiters = (unsigned int)(~0x0)-1; //100000;
		MTRand mtrand;
		std::map<int, int> bins;
		unsigned int gmax=0, gmin=(unsigned int)(~0x0)-1;
		for (i = mystart; i < mystart+myn; i++) {
			for (j = 0; j < namelist.size(); j++) {
				if(iter++ < maxiters/allnodes) {
					int src, tgt;
					if(nconn < maxiters) {
						//if((mynode == 0) && ((iter) % 1000 == 0)) printf("Evaluating pair number %d of %d (%.2f%%)\n", iter, nconn/allnodes, (float)iter/nconn*allnodes*100);
						src=i; tgt=j;
					} else {
						if((mynode == 0) && ((iter) % 1000 == 0)) printf("Evaluating pair number %d of %d (%.2f%%)\n", iter, maxiters/allnodes, (float)iter/maxiters*allnodes*100);
						src = mtrand.randInt(namelist.size()-1);
						tgt = mtrand.randInt(namelist.size()-1);
					}
					uroute_t route;
					find_route(&route, namelist.at(src), namelist.at(tgt));

					int max = 0;
					for(uroute_t::iterator iter=route.begin(); iter!=route.end(); iter++) {
						//printf("%i\n", cable_cong[*iter]);
						/* do not evaluate the first and last edge! */
						if(iter != route.begin() && (iter+1) != route.end()) {
							if(cable_cong[*iter] > max) max = cable_cong[*iter];
						}
						//printf("%i ", cable_cong[*iter]);
					}
					//printf("| %i -> %i: %i\n", src, tgt, max);
					if(max > gmax) gmax = max;
					if((max < gmin) && (max > 0)) gmin = max;
					if(!bins.count(max)) bins[max] = 0;
					bins[max]++;
				}
			}
		}
		/* allreduce bins if parallel */
		if(allnodes > 1) allreduce_contig_int_map(&bins);

		if(mynode == 0) {
			printf("gmin: %u, gmax: %u\n", gmin, gmax);

			// erase 0
			bins[0] = 0;

			// get number of elements in bins
			float sum=0;
			for(std::map<int, int>::iterator i=bins.begin(); i!=bins.end(); i++) {
				sum+=i->second;
			}

			// get probability for each key
			std::map<int, float> prob;
			for(std::map<int, int>::iterator i=bins.begin(); i!=bins.end(); i++) {
				prob[i->first] = (float)i->second/sum;
			}

			// get E and V
			float E=0,V=0;
			for(std::map<int, int>::iterator i=bins.begin(); i!=bins.end(); i++) {
				E += i->first*prob[i->first];
				V += (i->first*i->first)*prob[i->first];
			}
			V = V - E*E;
			printf("E: %.2f, sigma: %.2f\n", E, sqrt(V));


			printf("Completed\n");
		}

		MPI_Finalize();
		return 0;
	}

	/* If the user has provided a nodeorder guid list we may need to modify the namelist,
	 * or the part_namelist if the part_subset_arg is not "none". */
	if (nodeorder_guidlist.size()) {
		namelist_t *tmp_namelist;
		guidlist_t *tmp_guidlist;

		if (strcmp(cmdargs.args_info.part_subset_arg, "none") != 0) {
			/* If a part subset is provided, the node ordering applies to this part subset
			 * only. So se the tmp_namelist to the corresponding data structures. */
			tmp_namelist = &part_namelist;
			tmp_guidlist = &part_guidlist;
		} else {
			/* If the part subset is not provided... business as usual. Deal with the
			 * default namelist */
			tmp_namelist = &namelist;
			tmp_guidlist = &guidlist;
		}

		/* Get the complete numeric GUID list */
		get_guidlist_from_namelist(&complete_namelist, &complete_guidlist);

		/* Get the shuffled numeric GUID list */
		get_guidlist_from_namelist(tmp_namelist, tmp_guidlist);

		/** We use the numeric GUID lists for the comparisons, because the
		 *  nodeorder_guidlist is already in anumeric format. */

		/* First remove the nodeorder guids that do not exist in the namelist (tmp_guidlist).
		 * For example, if the user asks for the nodeorder of GUIDs 0x100, 0x2, 0x10, 0x1
		 * but GUID 0x100 do not exist at all in the namelist/guidlist (either
		 * mistake from the user, or the user is using a subset and GUID 0x100 is not
		 * part of that subset), then we need to remove 0x100 from this list. */
		std::vector<unsigned long long>::iterator ull_iter;
		for (ull_iter = nodeorder_guidlist.begin(); ull_iter != nodeorder_guidlist.end(); ) {
			if (std::find(tmp_guidlist->begin(), tmp_guidlist->end(), *ull_iter) == tmp_guidlist->end())
				/* If not found, remove from nodeorder_guidlist */
				ull_iter = nodeorder_guidlist.erase(ull_iter);
			else
				ull_iter++;
		}

		/* Convert the nodeordered guidlist back to an ordered namelist. Now we are sure that
		 * all of the nodeorder_namelist entries exist in the namelist that we are going to
		 * shuffle. */
		get_namelist_from_guidlist(&nodeorder_guidlist, &complete_namelist, &nodeorder_namelist);

		/* Then remove from the namelist (namelist is the list we are going to shuffle) the
		 * nodenames that exist in the nodeorder_namelist. We will re-add the removed entries
		 * in the "final_namelist", but after the shuffling of the "namelist" has occured
		 * (that's how we ensure the node-ordering). */
		std::vector<std::string>::iterator str_iter;
		for (i = 0; i < nodeorder_namelist.size(); i++) {
			str_iter = std::find(tmp_namelist->begin(), tmp_namelist->end(), nodeorder_namelist.at(i));
			if (str_iter != tmp_namelist->end())
				str_iter = tmp_namelist->erase(str_iter); // Erases str_iter and returns the next iter
		}
	}

	if (strcmp(cmdargs.args_info.part_subset_arg, "none") != 0) {

		/* If the part_subset is not "none", we need to remove the part_namelist
		 * entries from the namelist in order to ensure we will have unique
		 * entries in the final_namelist..... */
		std::vector<std::string>::iterator str_iter;
		for (i = 0; i < part_namelist.size(); i++) {
			str_iter = std::find(namelist.begin(), namelist.end(), part_namelist.at(i));
			if (str_iter != namelist.end())
				str_iter = namelist.erase(str_iter);
		}

		/* .....as well as any entries from the nodeorder_namelist. */
		for (i = 0; i < nodeorder_namelist.size(); i++) {
			str_iter = std::find(namelist.begin(), namelist.end(), nodeorder_namelist.at(i));
			if (str_iter != namelist.end())
				str_iter = namelist.erase(str_iter);
		}
	}

	if (mynode == 0) {
		if (cmdargs.args_info.verbose_given) {
			if (nodeorder_namelist.size() != 0) {
				print_namelist(&nodeorder_namelist, "NODEORDER_NAMELIST");
			}

			if (part_namelist.size() != 0) {
				print_namelist(&part_namelist, "PART_NAMELIST");
			}

			print_namelist(&namelist, "NAMELIST");
		}
	}

	int run_count = 1;

	while (1) { // perform simulations

		int level = cmdargs.args_info.ptrn_level_arg;
		if(level < 0) level = 0;

		/* Shuffle the list */
		if (!cmdargs.args_info.do_not_shuffle_given)
			shuffle_namelist(&namelist);

		/* Copy the shuffled list to the final_namelist. The final_namelist
		 * will be used to run the simulations. However, */
		final_namelist = namelist;

		/* If we use a subset for the first-part communicator (only when ptrnvsptrn is used)
		 * add the part_namelist on top of the existing final_namelist. */
		if (strcmp(cmdargs.args_info.part_subset_arg, "none") != 0) {
			if (!cmdargs.args_info.do_not_shuffle_given)
				shuffle_namelist(&part_namelist);

			for (i = 0; i < part_namelist.size(); i++)
				final_namelist.insert(final_namelist.begin() + i, part_namelist.at(i));
		}

		/* If the user has provided a nodeorder guid list we need to push these nodes
		 * to the front of the final_namelist */
		if (nodeorder_guidlist.size()) {
			/* Now push the ordered names in the front of the final_namelist that
			 * already contains the contents of the shuffled namelist. */
			for (i = 0; i < nodeorder_namelist.size(); i++)
				final_namelist.insert(final_namelist.begin() + i, nodeorder_namelist.at(i));
		}

		/* The function print_namelist_from_all used MPI_Send and MPI_Recv
		 * to print the namelist from all the MPI nodes to node 0. */
		if (cmdargs.args_info.printnamelist_given)
			print_namelist_from_all(&final_namelist, mynode, allnodes);

		if(strcmp(cmdargs.args_info.metric_arg, "dep_max_delay") == 0) {
			simulation_dep_max_delay(&cmdargs, &final_namelist, cmdargs.args_info.part_commsize_arg, mynode);
			if (cmdargs.args_info.verbose_given && (mynode == 0)) {
				std::cout << "Process " << mynode << ": Simulation run number ";
				std::cout << run_count << " finished.\n" << std::flush;
			}
		} else {
			// TODO: uebelst beschissen but a fast solution to the problem.
			while (1) {
				ptrn_t ptrn;

				genptrn_by_name(&ptrn, cmdargs.args_info.ptrn_arg, cmdargs.ptrnarg,
				                cmdargs.args_info.commsize_arg, cmdargs.args_info.part_commsize_arg,
				                level, mynode);

				if (ptrn.size()==0 || (cmdargs.args_info.ptrn_level_arg > -1 && level > cmdargs.args_info.ptrn_level_arg)) {break;}
				if ((cmdargs.args_info.printptrn_given) && (mynode == 0)) { printptrn(&ptrn, &final_namelist); }

				simulation_with_metric(cmdargs.args_info.metric_arg, &ptrn, &final_namelist, RUN);

				if (cmdargs.args_info.verbose_given && (mynode == 0)) {
					std::cout << "Process " << mynode << ": Simulation run number ";
					std::cout << run_count << ", level " << level << " finished.\n" << std::flush;
				}

				level++; //proceed to next level
			}
			simulation_with_metric(cmdargs.args_info.metric_arg, NULL, &final_namelist, ACCOUNT);
		}
		run_count++;
		//TODO Add support for error treshold(?)
		if (run_count > ceil((double) cmdargs.args_info.num_runs_arg / (double) allnodes)) {break;}
	}

	exchange_results_by_metric(cmdargs.args_info.metric_arg, mynode, allnodes);
	print_results(&cmdargs, mynode, allnodes);

	agclose(mygraph);

	cleanup_args(cmdargs.args_info.ptrn_arg, cmdargs.ptrnarg);

	MPI_Finalize();
	return EXIT_SUCCESS;
}
