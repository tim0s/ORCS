#ifndef PATTERN_GENERATOR_HPP
#define PATTERN_GENERATOR_HPP

#include <vector>
#include <utility>
#include <algorithm>
#include <string.h>
#include <string>
#include "MersenneTwister.h"
#include "simulator.hpp"

typedef std::pair<int, int> pair_t; 
typedef std::vector<pair_t> ptrn_t;

void genptrn_bisect(int comm_size, int level, ptrn_t *ptrn,
                    bool respect_print_once = true);

void genptrn_bisect_fb_sym(int comm_size, int level, ptrn_t *ptrn,
                           bool respect_print_once = true);

void genptrn_null(int comm_size, int level, ptrn_t *ptrn,
                  bool respect_print_once = true);

void genptrn_rand(int comm_size, int level, ptrn_t *ptrn,
                  bool respect_print_once = true);

void genptrn_scatter(int comm_size, int level, ptrn_t *ptrn,
                     bool respect_print_once = true);

void genptrn_gather(int comm_size, int level, ptrn_t *ptrn,
                    bool respect_print_once = true);

void genptrn_tree(int comm_size, int level, ptrn_t *ptrn,
                  bool respect_print_once = true);

void genptrn_bruck(int comm_size, int level, ptrn_t *ptrn,
                   bool respect_print_once = true);

void genptrn_ring(int comm_size, int level, ptrn_t *ptrn,
                  bool respect_print_once = true);

void genptrn_neighbor2d(int comm_size, int level, ptrn_t *ptrn,
                        bool respect_print_once = true);

void genptrn_recdbl(int comm_size, int level, ptrn_t *ptrn,
                    bool respect_print_once = true);

void genptrn_nneighbor(int nprocs, int level, int neighbors, ptrn_t *ptrn,
                       bool respect_print_once = true);

void genptrn_nreceivers_with_chance(int comm_size, int level, int num_receivers,
                                    double chance_to_communicate_with_a_receiver,
                                    ptrn_t *ptrn, bool respect_print_once = true);

void printptrn(ptrn_t *ptrn, namelist_t *namelist);

void genptrn_by_name(ptrn_t *ptrn, char *ptrnname, void *ptrnarg, int comm_size,
                     int partcomm_size, int level, bool respect_print_once = true);

#endif
