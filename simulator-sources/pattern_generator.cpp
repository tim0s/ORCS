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
#include <cgraph.h>
#include <algorithm>
#include "pattern_generator.hpp"
#include "simulator.hpp"
 
/* generates a random communication pattern for a communicator of size
 * comm_size */
void genptrn_rand(int comm_size, int level, ptrn_t *ptrn) {
    /* this pattern generator draws two nodes out of the bucket and puts
     * them as communication pair into the pattern */
    MTRand mtrand;
    std::vector<bool> bucket(comm_size, false);
	int counter;
	int myrand;
	int pos;

    if(level != 0) return;

	for (counter = 0; counter < comm_size; counter++) {
    		myrand = mtrand.randInt(comm_size-counter-1);
		pos=0;
		while (true) {
			if (bucket[pos] == false) {
				if (myrand == 0) {
					bucket[pos] = true;
					ptrn->push_back(std::pair<int, int>(counter, pos));
					break;
				}
				myrand--;
			}
			pos++;
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
static inline void node_to_coords(int node, int xmax, int *x, int *y) { *x = node%xmax; *y = node/xmax; };
static inline void coords_to_node(int xmax, int ymax, int x, int y, int *node) { 
  if(x < 0) x = xmax+x; if(x>=xmax) x = x%xmax;
  if(y < 0) y = ymax+y; if(y>=ymax) y = y%ymax;
  *node = y*xmax + x;
};
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

void printptrn(ptrn_t *ptrn) {

    ptrn_t::iterator iter;
   
	if (ptrn->size() == 0) {
		printf("Pattern empty!\n");
		return;
	}

    std::cout << "\nUsed Pattern:\n===========\n";
	for(iter = ptrn->begin(); iter != ptrn->end(); iter++) {
		printf("%i -> %i\n", iter->first, iter->second);
    }
	std::cout << "==========\n";
}

void genptrn_by_name(ptrn_t *ptrn, char *name, char *frsname, char *secname, int comm_size, int partcomm_size, int level) {
	
	//printf("Name: %s\n Size: %i\n Level: %i\n", name, comm_size, level);
	
	ptrn->clear();
	if (strcmp(name, "rand") == 0) {genptrn_rand(comm_size, level, ptrn);}
	else if (strcmp(name, "bisect") == 0) {genptrn_bisect(comm_size, level, ptrn);}
	else if (strcmp(name, "null") == 0) {genptrn_null(comm_size, level, ptrn);}
	else if (strcmp(name, "bisect_fb_sym") == 0) {genptrn_bisect_fb_sym(comm_size, level, ptrn);}
	else if (strcmp(name, "tree") == 0) {genptrn_tree(comm_size, level, ptrn);}
	else if (strcmp(name, "bruck") == 0) {genptrn_bruck(comm_size, level, ptrn);}
	else if (strcmp(name, "gather") == 0) {genptrn_gather(comm_size, level, ptrn);}
	else if (strcmp(name, "scatter") == 0) {genptrn_scatter(comm_size, level, ptrn);}
	else if (strcmp(name, "neighbor2d") ==0) {genptrn_neighbor2d(comm_size, level, ptrn);}
	else if (strcmp(name, "ring") == 0) {genptrn_ring(comm_size, level, ptrn);}
	else if (strcmp(name, "recdbl") == 0) {genptrn_recdbl(comm_size, level, ptrn);}
	else if (strcmp(name, "2neighbor") == 0) {genptrn_nneighbor(comm_size, level, 2, ptrn);}
	else if (strcmp(name, "4neighbor") == 0) {genptrn_nneighbor(comm_size, level, 4, ptrn);}
	else if (strcmp(name, "6neighbor") == 0) {genptrn_nneighbor(comm_size, level, 6, ptrn);}
	else if (strcmp(name, "ptrnvsptrn") == 0) {
			static int level_ptrn2 = 0;
			ptrn_t ptrn1, ptrn2;
			genptrn_by_name(&ptrn1, frsname, (char *)"", (char *)"", partcomm_size, 0, level);
			genptrn_by_name(&ptrn2, secname, (char *)"", (char *)"", comm_size - partcomm_size, 0, level_ptrn2);
			if ((ptrn2.size()==0) && (ptrn1.size()!=0)) {
				level_ptrn2=0;
				genptrn_by_name(&ptrn2, secname, (char *)"", (char *)"", comm_size - partcomm_size, 0, level_ptrn2);
			}
			level_ptrn2++;
			merge_two_patterns_into_one(&ptrn1, &ptrn2, partcomm_size, ptrn);
	}
  else printf("%s pattern not implemented\n", name);
}
