#ifndef SIMULATOR_HPP
#define SIMULATOR_HPP

#include <vector>
#include <map>
#include <utility>
#include <string>
#include <assert.h>
#include "cmdline.h"
#include "MersenneTwister.h"

#define RUN 100
#define ACCOUNT 101
#define PARSE_GUID_BUFLEN  256
#define MAX_ARG_SIZE 80
/* IN and OUT are used to indicate if a function's
 * parameter is used as input or output */
#define IN
#define OUT


/* typedefs */
typedef struct {
	gengetopt_args_info args_info;
	void *ptrnarg;
} cmdargs_t;

typedef struct {
	char ptrn1[MAX_ARG_SIZE];        // ptrn1 name
	char ptrnargstr1[MAX_ARG_SIZE];  // ptrnarg1 string
	void *ptrnarg1;	                 // ptrnarg1 converted in the expected data type
	char ptrn2[MAX_ARG_SIZE];        // ptrn2 name
	char ptrnargstr2[MAX_ARG_SIZE];  // ptrnarg2 string
	void *ptrnarg2;                  // ptrnarg2 converted in the expected data type
} ptrnvsptrn_t;

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
typedef std::vector<unsigned long long> guidlist_t;

/* prototypes */
void merge_two_patterns_into_one(ptrn_t *ptrn1, ptrn_t *ptrn2, int comm1_size, ptrn_t *ptrn_res);
void exchange_results_sum_max_cong(int mynode, int allnodes);
void exchange_results_hist_max_cong(int mynode, int allnodes);
void exchange_results_by_metric(char *metric_name, int mynode, int allnodes);
void simulation_with_metric(char *metric_name, ptrn_t *ptrn, namelist_t *namelist, int state);
void simulation_hist_max_cong(ptrn_t *ptrn, namelist_t *namelist, int state);
void simulation_hist_effective_bandwidth(ptrn_t *ptrn, namelist_t *namelist, int state);
void simulation_sum_max_cong(ptrn_t *ptrn, namelist_t *namelist, int state);
void simulation_dep_max_delay(cmdargs_t *cmdargs, namelist_t *namelist, int valid_until, int myrank);
void simulation_get_cable_cong(ptrn_t *ptrn, namelist_t *namelist, int state);
void print_commandline_options(FILE *fd, cmdargs_t *cmdargs);
void print_results(cmdargs_t *cmdargs, int mynode, int allnodes);
void print_namelist(namelist_t *namelist);
void generate_namelist_by_name(char *method, namelist_t *namelist, int comm_size);
void generate_random_namelist(namelist_t *namelist, int comm_size);
void generate_linear_namelist_bfs(namelist_t *namelist, int comm_size);
void generate_linear_namelist_guid_order(namelist_t *namelist, int comm_size, bool asc);
void shuffle_namelist(namelist_t *namelist);
void simulate(used_edges_t *edge_list,  ptrn_t *ptrn, int num_runs);
void find_route(uroute_t *route, std::string n1, std::string n2);
int contains_target(char *comment, char *target);
unsigned long long convert_nodename_to_guid(std::string nodename);
void get_guidlist_from_namelist(IN namelist_t *namelist,
                                OUT guidlist_t *guidlist);
void get_namelist_from_guidlist(IN guidlist_t *guidlist,
                                IN namelist_t *complete_namelist,
                                OUT namelist_t *namelist);
void get_namelist_from_graph(OUT namelist_t *namelist);
void get_namelist_from_graph(OUT namelist_t *namelist,
                             OUT guidlist_t *guidlist);
void generate_random_mapping(named_ptrn_t *mapping, ptrn_t *ptrn);
void insert_route_into_uedgelist(used_edges_t *edge_list, route_t *route);
std::string lookup(int nodenumber, namelist_t *namelist);
void printmapping(named_ptrn_t *mapping);
void my_mpi_init(int *argc, char ***argv, int *rank, int *comm_size);
void read_input_graph(char *filename);
void read_node_ordering(IN char *filename,
                        OUT guidlist_t *guidorder_list);
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

#endif
