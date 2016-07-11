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

extern void perform_sanity_checks_in_args(IN OUT cmdargs_t *cmdargs);
extern void cleanup_args(IN char *ptrn, void *ptrnarg);

int main(int argc, char *argv[]) {
	
	// MPI variables, comm_rank and comm_size
	int mynode, allnodes;
	namelist_t namelist, complete_namelist, nodeorder_namelist, final_namelist;
	guidlist_t guidlist, complete_guidlist, nodeorder_guidlist;
	int i, j;

	cmdargs_t cmdargs;

	if (cmdline_parser(argc, argv, &cmdargs.args_info) != 0) exit(EXIT_FAILURE);
	perform_sanity_checks_in_args(&cmdargs);
	
	if (cmdargs.args_info.getnumlevels_given) {
		int level = 0;
		while (1) {
			ptrn_t ptrn;
			
			//void genptrn_by_name(ptrn_t *ptrn, char *name, char *frsname, char *secname, int comm_size, int partcomm_size, int level) {
			genptrn_by_name(&ptrn, cmdargs.args_info.ptrn_arg, cmdargs.ptrnarg,
			                cmdargs.args_info.commsize_arg, cmdargs.args_info.part_commsize_arg,
			                level);
			if (ptrn.size() == 0) { break; }

			level++; //proceed to next level
		}
		std::cout << "The given input configuration would result in a " << level << " level simulation." << std::endl;
		return level;
	}

	my_mpi_init(&argc, &argv, &mynode, &allnodes);

	/* should only be done on rank 0 */
	read_input_graph(cmdargs.args_info.input_file_arg);
	tag_edges(mygraph);

	/* Read the node ordering if provided */
	read_node_ordering(cmdargs.args_info.node_ordering_file_arg, &nodeorder_guidlist);

	/* Read the complete namelist and store it in a temporary vector */
	get_namelist_from_graph(&complete_namelist);

	if (cmdargs.args_info.commsize_arg == 0) {
		cmdargs.args_info.commsize_arg = complete_namelist.size() - complete_namelist.size() % 2;
	} else if (cmdargs.args_info.commsize_arg > complete_namelist.size()) {
		printf("You chose a very large 'commsize'.\n"
		       "The maximum possible communication size is %d\n", complete_namelist.size());
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

	/* get a list of all endpoint-names that we will work with from the dot-file
	 * This list, the namelist, may be a subset of the complete list. */
	if (mynode == 0)
		generate_namelist_by_name(cmdargs.args_info.subset_arg, &namelist, cmdargs.args_info.commsize_arg);

	/* distribute namelist from root to all nodes */
	bcast_namelist(&namelist, allnodes, mynode);

	/* assess the quality of the routing table */
	if(mynode == 0) {
		std::cout << "Number of hosts in the inputfile: " << namelist.size() << "\n";
		std::cout << "Number of nodes in the inputfile: " << agnnodes(mygraph) << "\n";
		std::cout << "Number of edges in the inputfile: " << agnedges(mygraph) << "\n";
	}

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
			printf("gmin: %i, gmax: %i\n", gmin, gmax);

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

	/* If the user has provided a nodeorder guid list we may need to modify the namelist */
	if (nodeorder_guidlist.size()) {
		/* Get the complete GUID list */
		get_guidlist_from_namelist(&complete_namelist, &complete_guidlist);

		/* Get the shuffled GUID list */
		get_guidlist_from_namelist(&namelist, &guidlist);

		/* First remove the nodeorder guids that do not exist in the shuffle_namelist */
		bool found;
		std::vector<unsigned long long>::iterator ull_iter;
		for (ull_iter = nodeorder_guidlist.begin(); ull_iter != nodeorder_guidlist.end(); ) {
			found = false;

			for (i = 0; i < guidlist.size(); i++) {
				if (*ull_iter == guidlist.at(i)) {
					found = true;
					break;
				}
			}

			if (!found)
				ull_iter = nodeorder_guidlist.erase(ull_iter);
			else
				ull_iter++;
		}

		get_namelist_from_guidlist(&nodeorder_guidlist, &complete_namelist, &nodeorder_namelist);

		/* Then remove from the namelist the nodenames that exist in the nodeorder_guidlist */
		std::vector<std::string>::iterator str_iter;
		for (str_iter = namelist.begin(); str_iter != namelist.end(); ) {
			found = false;

			for (i = 0; i < nodeorder_namelist.size(); i++) {
				if (!strcmp((*str_iter).data(), nodeorder_namelist.at(i).data())) {
					found = true;
					break;
				}
			}

			if (found)
				str_iter = namelist.erase(str_iter);
			else
				str_iter++;
		}
	}

	int run_count = 1;

	while (1) { // perform simulations

		int level = cmdargs.args_info.ptrn_level_arg;
		if(level < 0) level = 0;

		/* Shuffle the list */
		shuffle_namelist(&namelist);

		/* Copy the shuffled list to the final_namelist. The final_namelist
		 * will be used to run the simulations. However, */
		final_namelist = namelist;

		/* If the user has provided a nodeorder guid list we need to push these nodes
		 * to the front of the final_namelist */
		if (nodeorder_guidlist.size()) {
			/* Now push the ordered names in the front of the final_namelist that
			 * already contains the contents of the shuffled namelist. */
			for (i = 0; i < nodeorder_namelist.size(); i++)
				final_namelist.insert(final_namelist.begin() + i, nodeorder_namelist.at(i));
		}

		if ((cmdargs.args_info.printnamelist_given) && (mynode == 0)) { print_namelist(&final_namelist); }

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
				                level);

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

	//	simulation_with_metric(args_info.metric_arg, NULL, &namelist, ACCOUNT);
	exchange_results_by_metric(cmdargs.args_info.metric_arg, mynode, allnodes);
	print_results(&cmdargs, mynode, allnodes);

	MPI_Finalize();
	agclose(mygraph);

	cleanup_args(cmdargs.args_info.ptrn_arg, cmdargs.ptrnarg);

	return EXIT_SUCCESS;
}
