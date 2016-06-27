#include <vector>
#include <map>
#include <utility>
#include <string>
#include <assert.h>
#include "cmdline.h"
#include "MersenneTwister.h"

#define RUN 100
#define ACCOUNT 101


/* typedefs */
typedef std::pair<int, int> pair_t; 
typedef std::vector<pair_t> ptrn_t;

typedef std::pair<std::string, std::string> edge_t; 
typedef int edgeid_t;

typedef std::vector<edge_t> route_t;
typedef std::vector<edgeid_t> uroute_t;

typedef std::vector<edge_t> named_ptrn_t;

class used_edge_t {
    public:
	    edge_t edge;
	    int usage;
        std::vector<edge_t> peers; /* all the terminal-nodes that use this edge */

        bool operator<(used_edge_t b) const {
            if(usage < b.usage) return true;
            else return false;
        }
};

typedef std::vector<used_edge_t> used_edges_t;
typedef std::map<edgeid_t, int> cable_cong_map_t;
typedef std::vector<std::string> namelist_t;

/* prototypes */
void merge_two_patterns_into_one(ptrn_t *ptrn1, ptrn_t *ptrn2, int comm1_size, ptrn_t *ptrn_res);
void exchange_results_sum_max_cong(int mynode, int allnodes);
void exchange_results_hist_max_cong(int mynode, int allnodes);
void exchange_results_by_metric(char *metric_name, int mynode, int allnodes);
void simulation_with_metric(char *metric_name, ptrn_t *ptrn, namelist_t *namelist, int state);
void simulation_hist_max_cong(ptrn_t *ptrn, namelist_t *namelist, int state);
void simulation_hist_effective_bandwidth(ptrn_t *ptrn, namelist_t *namelist, int state);
void simulation_sum_max_cong(ptrn_t *ptrn, namelist_t *namelist, int state);
void simulation_dep_max_delay(gengetopt_args_info *args_info, namelist_t *namelist, int valid_until, int myrank);
void simulation_get_cable_cong(ptrn_t *ptrn, namelist_t *namelist, int state);
void print_commandline_options(FILE *fd, gengetopt_args_info *args_info);
void print_results(gengetopt_args_info *args_info, int mynode, int allnodes);
void print_namelist(namelist_t *namelist);
void generate_namelist_by_name(char *method, namelist_t *namelist, int comm_size);
void generate_random_namelist(namelist_t *namelist, int comm_size);
void generate_linear_namelist_bfs(namelist_t *namelist, int comm_size);
void shuffle_namelist(namelist_t *namelist);
void simulate(used_edges_t *edge_list,  ptrn_t *ptrn, int num_runs);
void find_route(uroute_t *route, std::string n1, std::string n2);
int contains_target(char *comment, char *target);
void get_name_list(namelist_t *namelist);
void generate_random_mapping(named_ptrn_t *mapping, ptrn_t *ptrn);
void insert_route_into_uedgelist(used_edges_t *edge_list, route_t *route);
std::string lookup(int nodenumber, namelist_t *namelist);
void printmapping(named_ptrn_t *mapping);
void my_mpi_init(int *argc, char ***argv, int *rank, int *comm_size);
void read_input_graph(char *filename);
void bcast_namelist(namelist_t *namelist, int comm_size, int rank);
void exchange_results(int mynode, int allnodes, double result);
void exchange_results2(int mynode, int allnodes);
void insert_route_into_cable_cong_map(cable_cong_map_t *cable_cong, uroute_t *route);
void get_max_congestion(uroute_t *route, cable_cong_map_t *cable_cong, int *weight);
void tag_edges(Agraph_t *mygraph);
void allreduce_contig_int_map(std::map<int,int> *map);
void write_graph_with_congestions();

/* globals */
#ifndef MYGLOBALS
#define MYGLOBALS
    extern Agraph_t *mygraph;
#endif
