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

int main(int argc, char **argv) {
	
    // MPI variables, comm_rank and comm_size
	int mynode, allnodes;

	gengetopt_args_info args_info;

    if (cmdline_parser(argc, argv, &args_info) != 0) exit(EXIT_FAILURE);
	
	if (args_info.getnumlevels_given) {
		int level = 0;
		while (1) {
			ptrn_t ptrn;
			
//void genptrn_by_name(ptrn_t *ptrn, char *name, char *frsname, char *secname, int comm_size, int partcomm_size, int level) {
			genptrn_by_name(&ptrn, args_info.ptrn_arg, args_info.ptrnfst_arg, args_info.ptrnsec_arg, args_info.commsize_arg, args_info.part_commsize_arg, level);
			if (ptrn.size()==0) {break;}

			level++; //proceed to next level
		}
		std::cout << "The given input configuration would result in a " << level << " level simulation." << std::endl;
		return level;
	}

    my_mpi_init(&argc, &argv, &mynode, &allnodes);

    /* should only be done on rank 0 */
    read_input_graph(args_info.input_file_arg);
    tag_edges(mygraph);

	namelist_t tnamelist;
      get_name_list(&tnamelist);

	if (args_info.commsize_arg == 0) {
		args_info.commsize_arg = tnamelist.size() - tnamelist.size()%2;
	}

    if(mynode == 0) { // print graph info
	

      print_commandline_options(stdout, &args_info);
     	
		if (args_info.checkinputfile_given) {
			std::cout << "Number of hosts in the inputfile: " << tnamelist.size() << "\n";
			std::cout << "Number of nodes in the inputfile: " << agnnodes(mygraph) << "\n";

			for (int i=0; i<tnamelist.size(); i++) {
				for (int j=0; j<tnamelist.size(); j++) {
                	printf("Testing pair number %d of %d\n", i*tnamelist.size()+j+1, tnamelist.size()*tnamelist.size());
                	uroute_t r;
                	find_route(&r, tnamelist.at(i), tnamelist.at(j));
            	}
        	}
        	printf("Completed\n");
        	return 0;
    	}

      // we have to use all hosts for the route quality assessment 
      if (args_info.routequal_given) args_info.commsize_arg = tnamelist.size();
    }

  // a list of all endpoint-names from the dot-file
	namelist_t namelist;
	if (mynode == 0) { 
		generate_namelist_by_name(args_info.subset_arg, &namelist, args_info.commsize_arg);
	}
  // distribute namelist from root to all nodes
  bcast_namelist(&namelist, allnodes, mynode);

  /* assess the quality of the routing table */
    if(mynode == 0) {
      std::cout << "Number of hosts in the inputfile: " << namelist.size() << "\n";
      std::cout << "Number of nodes in the inputfile: " << agnnodes(mygraph) << "\n";
	  std::cout << "Number of edges in the inputfile: " << agnedges(mygraph) << "\n";	
    }
	
	if (args_info.routequal_given) {
	  used_edges_t edge_list;
	  cable_cong_map_t cable_cong;
    int nconn = namelist.size()*namelist.size();
    int conn=0;
		
    int myn = namelist.size()/allnodes;
    int mystart = myn*mynode;
    if(mynode == allnodes-1) myn = namelist.size()-mystart;
    //printf("[%i] myn: %i, mystart: %i\n", mynode, myn, mystart);

    /* generate cable-congestion by all routes */
    for (int i=mystart; i<mystart+myn; i++) {
			for (int j=0; j<namelist.size(); j++) {
				//if((mynode == 0) && ((conn++) % 1000 == 0)) printf("Initializing pair number %d of %d (%.2f%%)\n", conn, nconn/allnodes, (float)conn/nconn*allnodes*100);
        //printf("%i -> %i\n", i, j);
			  uroute_t route;
				find_route(&route, namelist.at(i), namelist.at(j));
			  insert_route_into_cable_cong_map(&cable_cong, &route);
			}
		}
    /* allreduce cable_cong if parallel */
    if(allnodes > 1) allreduce_contig_int_map(&cable_cong);
  
    /* begin analysis */
    int iter = 0;
    const unsigned int maxiters = (unsigned int)(~0x0)-1;//100000;
	  MTRand mtrand;
    std::map<int, int> bins;
    unsigned int gmax=0, gmin=(unsigned int)(~0x0)-1;
    for (int i=mystart; i<mystart+myn; i++) {
			for (int j=0; j<namelist.size(); j++) {
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

	int run_count = 1;

	while (1) { // perform simulations
	
		int level = args_info.ptrn_level_arg;
        if(level < 0) level = 0;
		
		shuffle_namelist(&namelist);
		if ((args_info.printnamelist_given) && (mynode == 0)) { print_namelist(&namelist); }
	
    if(strcmp(args_info.metric_arg, "dep_max_delay") == 0) {
      simulation_dep_max_delay(&args_info, &namelist, args_info.part_commsize_arg, mynode);
      if (args_info.verbose_given && (mynode == 0)) {
          std::cout << "Process " << mynode << ": Simulation run number ";
          std::cout << run_count << " finished.\n" << std::flush;
      }
    } else {
      // TODO: uebelst beschissen but a fast solution to the problem.
      while (1) {
        ptrn_t ptrn;
//void genptrn_by_name(ptrn_t *ptrn, char *name, char *frsname, char *secname, int comm_size, int partcomm_size, int level) {
        genptrn_by_name(&ptrn, args_info.ptrn_arg, args_info.ptrnfst_arg, args_info.ptrnsec_arg, args_info.commsize_arg, args_info.part_commsize_arg, level);
        
        if ((args_info.printptrn_given) && (mynode == 0)) {printptrn(&ptrn);}
        if (ptrn.size()==0 || (args_info.ptrn_level_arg > -1 && level > args_info.ptrn_level_arg)) {break;}

        simulation_with_metric(args_info.metric_arg, &ptrn, &namelist, RUN);

        if (args_info.verbose_given && (mynode == 0)) {
          std::cout << "Process " << mynode << ": Simulation run number ";
          std::cout << run_count << ", level " << level << " finished.\n" << std::flush;
        }
      
        level++; //proceed to next level
      }
      simulation_with_metric(args_info.metric_arg, NULL, &namelist, ACCOUNT);
    }
		run_count++;
		//TODO Add support for error treshold(?)
		if (run_count > ceil((double) args_info.num_runs_arg / (double) allnodes)) {break;}
	}

//	simulation_with_metric(args_info.metric_arg, NULL, &namelist, ACCOUNT);
	exchange_results_by_metric(args_info.metric_arg, mynode, allnodes);
	print_results(&args_info, mynode, allnodes);

	MPI_Finalize();
	agclose(mygraph);
    return 0;
}
