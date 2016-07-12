/*
 * Copyright (c) 2009 The Trustees of Indiana University and Indiana
 *                    University Research and Technology
 *                    Corporation.  All rights reserved.
 *
 * Author(s): Timo Schneider <timoschn@cs.indiana.edu>
 *            Torsten Hoefler <htor@cs.indiana.edu>
 *
 */

/* the pattern generator generates different communication pattern, it
 * offers support for random patterns and different common communication
 * patterns for collective operations and also application patterns such
 * as nearest neighbor communication
 *
 * The generator has different input arguments depending on the
 * generated pattern and returns a vector of communication pairs.
 *
 */
#include <stdlib.h>
#include <string.h>
#include <cgraph.h>
#include <algorithm>
#include <boost/assign/std/vector.hpp>
#include "pattern_generator.hpp"
#include "simulator.hpp"

template< class T >
struct next
{
	T seed;
	next( T seed ) : seed(seed)
	{ }

	T operator()()
	{
		return seed++;
	}
};

/* generates a random communication pattern for a communicator of size
 * comm_size */
void genptrn_rand(int comm_size, int level, ptrn_t *ptrn) {
	/* this pattern generator draws two nodes, ensures that they are not the same
	 * in order to avoid having nodes sending traffic to themselves (wight 0), and
	 * puts them as communication pairs into the pattern */
	MTRand mtrand;
	std::vector<int> available_dests_bucket;
	int src = 0, dst = 0;
	int myrand_pos;

	if(level != 0) return;

	/* Initialize the available_dests_bucket vector with values from 0 to comm_size - 1
	 * Then we will use this vector to pull random destinations and form random
	 * source -> destination pairs. Everytime we pull a value we remove it from the
	 * bucket, thus, ensuring that we will not add any duplicate destinations */
	boost::assign::push_back(available_dests_bucket).repeat_fun(comm_size, next<int>(0));

	for (src = 0; src < comm_size; src++) {
		while(true) {
			myrand_pos = mtrand.randInt((comm_size - 1) - src);

			dst = available_dests_bucket.at(myrand_pos);
			/* We do not want to end up with pairs communicating with themeselves: e.g. 5 -> 5
			 *
			 * So add the pattern pair only if counter != pos.
			 */
			if (src != dst) {
				ptrn->push_back(std::pair<int, int>(src, dst));
				available_dests_bucket.erase(available_dests_bucket.begin() + myrand_pos);
				break;
			} else {
				/* If we run in this "else" statement, then src == dst.
				 *
				 * If this is not the last loop in the for statetement (src == (comm_size - 1)),
				 * re-run in the while loop in order to re-draw a random number for the given src.
				 */
				if (src == (comm_size - 1)) {
					/* However, there is a small chance that we may end up in a deadlock situation if we
					 * reach the last pair and  all the previous random pairs are different. Then the only
					 * possible last pair left is src == dst. Consider the following example:
					 *
					 * comm_size = 4:
					 * Random pairs:
					 * 0 -> 2
					 * 1 -> 0
					 * 2 -> 1
					 * 3 -> 3 <------ Only choice left. We want to avoid this deadlock situation.
					 *
					 * In this case we want to swap a random number from the already added pairs...
					 */
					/* 1. Choose a new random tmp_pair from ptrn */
					myrand_pos = mtrand.randInt(ptrn->size() - 1);
					pair_t &tmp_pair = ptrn->at(myrand_pos);

					/* 2. Store the destination value from the tmp_pair in variable dst. */
					dst = tmp_pair.second;

					/* 3. Change the destination value of the tmp_pair to the last available
					 * value which is equal to (comm_size - 1). */
					tmp_pair.second = (comm_size - 1);

					/* 4. Use the stored dst value for the last pair. */
					ptrn->push_back(std::pair<int, int>(src, dst));

					/* At this point there is only one element left in the vector, so
					 * just delete the .begin() */
					available_dests_bucket.erase(available_dests_bucket.begin());

					break;
				}
			}
		}
	}
}

void genptrn_bisect(int comm_size, int level, ptrn_t *ptrn) {
	/* Generates a bisection bandwith testing pattern */
	if (level != 0) return;

	for (int counter = 0; counter < comm_size-1; counter++) {
		ptrn->push_back(std::pair<int, int>(counter, counter++));
	}
}

void genptrn_null(int comm_size, int level, ptrn_t *ptrn) {
	/* Generates a bisection bandwith testing pattern */
	if (level != 0) return;
	ptrn->clear();
}


void genptrn_bisect_fb_sym(int comm_size, int level, ptrn_t *ptrn) {
	/* Generates a bisection bandwith testing pattern */
	if (level != 0) return;

	for (int counter = 0; counter < comm_size-1; counter++) {
		ptrn->push_back(std::pair<int, int>(counter, counter+1));
		ptrn->push_back(std::pair<int, int>(counter+1, counter));
		counter++;
	}
}

/* generates a binomial tree communication pattern for a communicator of
 * size comm_size, level indicates the level of the algorithm
 * (0..log_2(comm_size)-1) */
void genptrn_tree(int comm_size, int level, ptrn_t *ptrn) {
	int dist = 1<<level;

	for(int i = 0; i < dist; i++) {
		if(i+dist >= comm_size) break;
		pair_t pair(i,i+dist);

		ptrn->push_back(pair);
	}
}

/* generates a n-ary binomial tree communication pattern for a communicator of
 * size comm_size, level indicates the level of the algorithm
 * (0..log_n(comm_size)-1) */
void genptrn_ntree(int comm_size, int level, ptrn_t *ptrn) {
	int dist = 1<<level;

	/*
	for(int i = 0; i < dist; i++) {
		if(i+dist >= comm_size) break;
		pair_t pair(i,i+dist);

		ptrn->push_back(pair);
	}*/
}

/* generates a bruck-style communication pattern for a communicator of
 * size comm_size, level indicates the level of the algorithm
 * (0..log_2(comm_size)-1) */
void genptrn_bruck(int comm_size, int level, ptrn_t *ptrn) {
	int dist = 1<<level;

	if(dist >= comm_size) return;

	for(int i = 0; i < comm_size; i++) {
		pair_t pair1(i,(i+dist) % comm_size);
		//pair_t pair2((i+dist) % comm_size, i);

		ptrn->push_back(pair1);
		//ptrn->push_back(pair2);
	}
}

/* generates a gather communication pattern for a communicator of
 * size comm_size */
void genptrn_gather(int comm_size, int level,  ptrn_t *ptrn) {
	if(level != 0) return;
	for(int i = 1; i < comm_size; i++) {
		pair_t pair(i,0);

		ptrn->push_back(pair);
	}
}

/* generates a scatter communication pattern for a communicator of
 * size comm_size */
void genptrn_scatter(int comm_size, int level,  ptrn_t *ptrn) {
	if(level != 0) return;
	for(int i = 1; i < comm_size; i++) {
		pair_t pair(0,i);

		ptrn->push_back(pair);
	}
}

/* generates a nearest neighbor communication pattern on a rectangular grid, all communications are done at the same time - so there is only one level */
static inline void node_to_coords(int node, int xmax, int *x, int *y) { *x = node%xmax; *y = node/xmax; }
static inline void coords_to_node(int xmax, int ymax, int x, int y, int *node) { 
	if(x < 0) x = xmax+x; if(x>=xmax) x = x%xmax;
	if(y < 0) y = ymax+y; if(y>=ymax) y = y%ymax;
	*node = y*xmax + x;
}
void genptrn_neighbor2d(int comm_size, int level, ptrn_t *ptrn) {

	if(level > 0) return;

	int xmax = (int)ceil(sqrt((double)comm_size));
	int ymax = (int)ceil((double)comm_size/(double)xmax);

	for(int node = 0; node < comm_size; node++) {
		int x,y,i;
		node_to_coords(node, xmax, &x, &y);
		//printf("node %i coords %i, %i (xmax %i, ymax %i)\n", node, x,y,xmax,ymax);
		int west;
		/* jump over empty 2d-grid elements */
		i=0; do { coords_to_node(xmax, ymax, x-(++i), y, &west); } while (west >= comm_size);
		pair_t pwest(node, west);
		ptrn->push_back(pwest);
		int east;
		i=0; do { coords_to_node(xmax, ymax, x+(++i), y, &east); } while (east >= comm_size);
		pair_t peast(node, east);
		ptrn->push_back(peast);
		int north;
		i=0; do { coords_to_node(xmax, ymax, x, y-(++i), &north); } while (north >= comm_size);
		pair_t pnorth(node, north);
		ptrn->push_back(pnorth);
		int south;
		i=0; do { coords_to_node(xmax, ymax, x, y+(++i), &south); } while (south >= comm_size);
		pair_t psouth(node, south);
		ptrn->push_back(psouth);
	}

	/* erase double elements */
	std::sort(ptrn->begin(), ptrn->end());
	ptrn->erase(unique(ptrn->begin(), ptrn->end()), ptrn->end());

	/* erase send to oneself */
	ptrn_t::iterator iter;
	for(iter = ptrn->begin(); iter != ptrn->end(); ) {
		if(iter->first == iter->second) iter = ptrn->erase(iter);
		else ++iter;
	}
}

void genptrn_nneighbor(int nprocs, int level, int neighbors, ptrn_t *ptrn) {

	if(level > 0) return;

	if(neighbors > nprocs-1) {
		neighbors = nprocs-1;
		printf("#*** correcting neighbor number to %i (commsize: %i)\n", neighbors, nprocs);
	}
	int *indexes = (int*)malloc(sizeof(int)*nprocs);
	int *tmpedges= (int*)malloc(sizeof(int*)*nprocs*neighbors);
	for(int i=0;i<nprocs*neighbors; i++) tmpedges[i] = -1;
	/* go over all procs and neighbors for this proc and find a proc
   * right of it with a free neighbor slot. Fill this slot and go to
   * next free neighbor slot. Peer up from left to right. This might
   * leave to empty slots (MPI_PROC_NULL). */
	for(int i=0;i<nprocs;i++) {
		int nei;
		for(nei=0;nei<neighbors;nei++) {
			int ind=i*neighbors + nei;
			/* if this has not a neighbor yet */
			if(tmpedges[ind] == -1) {
				int found = 0,k,l;
				/* find peer process */
				for(int k=i+1; k<nprocs;k++) {
					/* check if there is a connection to this process already */
					int foundme=0;
					for(int l=0;l<neighbors;l++) {
						int remind = k*neighbors+l;
						if(tmpedges[remind] == i) {
							foundme = 1;
						}
					}
					/* if there is a connection already - go to next possible peer */
					if(foundme) continue;

					/* see if there any any empty slots in peer process */
					for(int l=0;l<neighbors;l++) {
						int remind = k*neighbors+l;
						if(tmpedges[remind] == -1 && !found) {
							tmpedges[ind] = k;
							tmpedges[remind] = i;
							found = 1;
						} } } } } }

	//int *edges= (int*)malloc(sizeof(int*)*nprocs*neighbors);
	//for(int i=0;i<nprocs*neighbors; i++) edges[i] = -1;
	/* now, the adjacency list still has MPI_PROC_NULLs (-1) :-/ ....
   * filter them and arrange new array ... */
	int ind=0;
	for(int i=0;i<nprocs;i++) {
		for(int nei=0;nei<neighbors;nei++) {
			int eind=i*neighbors + nei;
			if(tmpedges[eind] != -1) {
				pair_t newpair(i, tmpedges[eind]);
				ptrn->push_back(newpair);
				ind++;
			}
		}
	}
	free(tmpedges);

	/*
  int j=0;
  for(int i=0;i<ind;i++) {
	if(i == indexes[j]) { printf(" | "); j++; }
	printf(" %i ", edges[i]);
  }
  free(edges);*/

#if 0
	/* erase double elements */
	std::sort(ptrn->begin(), ptrn->end());
	ptrn->erase(unique(ptrn->begin(), ptrn->end()), ptrn->end());

	/* erase send to oneself */
	ptrn_t::iterator iter;
	for(iter = ptrn->begin(); iter != ptrn->end(); ) {
		if(iter->first == iter->second) iter = ptrn->erase(iter);
		else ++iter;
	}
#endif
}

/* generates a ring communication pattern for a communicator of
 * size comm_size */
void genptrn_ring(int comm_size, int level, ptrn_t *ptrn) {
	if(level >= comm_size) return;

	pair_t pair(level,(level+1)%comm_size);

	ptrn->push_back(pair);
}

/* generates a recursive doubling communication pattern for a
 * communicator of size comm_size */
void genptrn_recdbl(int comm_size, int level, ptrn_t *ptrn) {
	int dist = 1<<level;

	int l = (int)floor(log((double)comm_size)/log(2.0));
	int power_comm_size = 1<<l;

	//printf("power comm size: %i (%i), dist %i\n", power_comm_size, l, dist);

	if(dist < power_comm_size) {
		for(int i = 0; i < power_comm_size; i=i+(dist<<1)) {
			for(int j = 0; j<dist; j++) {
				int k=i+j;
				if(dist+k < comm_size) {
					pair_t pair1(k,k+dist);
					ptrn->push_back(pair1);
					pair_t pair2(k+dist,k);
					ptrn->push_back(pair2);
				} } }
	} else if(1<<(level-1) < power_comm_size) {
		for(int i = 0; i<comm_size-power_comm_size; i++) {
			pair_t pair(i,i+power_comm_size);
			ptrn->push_back(pair);
		}
	} else return;
}

void genptrn_nreceivers(int comm_size, int level, int num_receivers, ptrn_t *ptrn) {
	if (level != 0) return;

	/* We cannot have more than comm_size / 2 receivers, because then we will
	 * not have enough senders to send traffic to all of the receivers. One
	 * sender sends traffic to only one receiver at a time. */
	if (num_receivers > floor((double)comm_size / 2)) {
		num_receivers = floor((double)comm_size / 2);
		printf("#*** WARN: cannot have more than commsize/2 receivers.\n"
		       "     Correcting number of receivers to %d (commsize: %d)\n",
		       num_receivers, comm_size);
	}

	MTRand mtrand;
	std::vector<int> receivers_bucket;
	std::vector<int> available_nodes_bucket;
	int receiver, i, src;
	int myrand_pos;

	if(level != 0) return;

	/* Initialize the available_nodes_bucket vector with values from 0 to comm_size - 1
	 * Then we will use this vector to pull first i receivers and random source nodes that
	 * send traffic to the receivers. Each source node sends traffic to one receiver, but
	 * each receivers is receiving traffic from more than one sources. Everytime we pull a
	 * value from the bucket we remove it, thus, ensuring that we will not have sources
	 * sending traffic to more than one receivers */
	boost::assign::push_back(available_nodes_bucket).repeat_fun(comm_size, next<int>(0));

	/* First pull out of the available_nodes_bucket the first num_receivers receiver nodes
	 * and put them in the bucket receivers_bucket */
	for (receiver = 0; receiver < num_receivers; receiver++) {
		receivers_bucket.push_back(available_nodes_bucket.at(0));

		available_nodes_bucket.erase(available_nodes_bucket.begin());
	}

	/* Now choose random sources and make them to communicate with one receiver at a time */
	for (i = 0; available_nodes_bucket.size() > 0; i++) {
		receiver = receivers_bucket.at(i % num_receivers);

		myrand_pos = mtrand.randInt(available_nodes_bucket.size() - 1);

		/* Choose a random source and remove it from the bucket */
		src = available_nodes_bucket.at(myrand_pos);
		available_nodes_bucket.erase(available_nodes_bucket.begin() + myrand_pos);

		ptrn->push_back(std::pair<int, int>(src, receiver));
	}
}

void printptrn(ptrn_t *ptrn, namelist_t *namelist) {

	ptrn_t::iterator iter;

	if (ptrn->size() == 0) {
		printf("Pattern empty!\n");
		return;
	}

	printf("\nUsed Pattern:\n=================\n");
	for(iter = ptrn->begin(); iter != ptrn->end(); iter++) {
		printf("% 5i -> %-5i   |   %s -> %s\n",
		       iter->first, iter->second,
		       namelist->at(iter->first).data(),
		       namelist->at(iter->second).data());
	}
	printf("=================\n");
}

void genptrn_by_name(ptrn_t *ptrn, char *ptrnname, void *ptrnarg, int comm_size, int partcomm_size, int level) {
	
	//printf("Name: %s\n Size: %i\n Level: %i\n", ptrnname, comm_size, level);
	
	ptrn->clear();
	if (strcmp(ptrnname, "rand") == 0) { genptrn_rand(comm_size, level, ptrn); }
	else if (strcmp(ptrnname, "bisect") == 0) { genptrn_bisect(comm_size, level, ptrn); }
	else if (strcmp(ptrnname, "null") == 0) { genptrn_null(comm_size, level, ptrn); }
	else if (strcmp(ptrnname, "bisect_fb_sym") == 0) { genptrn_bisect_fb_sym(comm_size, level, ptrn); }
	else if (strcmp(ptrnname, "tree") == 0) { genptrn_tree(comm_size, level, ptrn); }
	else if (strcmp(ptrnname, "bruck") == 0) { genptrn_bruck(comm_size, level, ptrn); }
	else if (strcmp(ptrnname, "gather") == 0) { genptrn_gather(comm_size, level, ptrn); }
	else if (strcmp(ptrnname, "scatter") == 0) { genptrn_scatter(comm_size, level, ptrn); }
	else if (strcmp(ptrnname, "neighbor2d") ==0) { genptrn_neighbor2d(comm_size, level, ptrn); }
	else if (strcmp(ptrnname, "ring") == 0) { genptrn_ring(comm_size, level, ptrn); }
	else if (strcmp(ptrnname, "recdbl") == 0) { genptrn_recdbl(comm_size, level, ptrn); }
	else if (strcmp(ptrnname, "neighbor") == 0) { genptrn_nneighbor(comm_size, level, *((int*)ptrnarg), ptrn); }
	else if (strcmp(ptrnname, "receivers") == 0) { genptrn_nreceivers(comm_size, level, *((int*)ptrnarg), ptrn); }
	else if (strcmp(ptrnname, "ptrnvsptrn") == 0) {

		static int level_ptrn2 = 0;
		ptrnvsptrn_t ptrnvsptrn = *((ptrnvsptrn_t *)ptrnarg);
		ptrn_t ptrn1, ptrn2;

		genptrn_by_name(&ptrn1, ptrnvsptrn.ptrn1, ptrnvsptrn.ptrnarg1, partcomm_size, 0, level);
		genptrn_by_name(&ptrn2, ptrnvsptrn.ptrn2, ptrnvsptrn.ptrnarg2, comm_size - partcomm_size, 0, level_ptrn2);

		if ((ptrn2.size() == 0) && (ptrn1.size()!= 0)) {
			level_ptrn2 = 0;
			genptrn_by_name(&ptrn2, ptrnvsptrn.ptrn2, ptrnvsptrn.ptrnarg2, comm_size - partcomm_size, 0, level_ptrn2);
		}

		merge_two_patterns_into_one(&ptrn1, &ptrn2, partcomm_size, ptrn);
		level_ptrn2++;
	}
	else {
		printf("ERROR: %s pattern not implemented\n", ptrnname);
		exit(EXIT_FAILURE);
	}
}

