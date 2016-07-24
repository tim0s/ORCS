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
#include <sys/stat.h>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cgraph.h>
#include <queue>
#include <map>
#include <mpi.h>
#include "simulator.hpp"
#include "statistics.hpp"
#include <string.h>

#include <boost/config.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/properties.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/depth_first_search.hpp>

#include "pattern_generator.hpp"

void exchange_results_by_metric(char *metric_name, int mynode, int allnodes) {
	if (strcmp(metric_name, "sum_max_cong") == 0) {exchange_results_sum_max_cong(mynode, allnodes);}
	if (strcmp(metric_name, "hist_acc_band") == 0) {exchange_results_sum_max_cong(mynode, allnodes);}
	if (strcmp(metric_name, "hist_max_cong") == 0) {exchange_results_hist_max_cong(mynode, allnodes);}
	if (strcmp(metric_name, "dep_max_delay") == 0) {exchange_results_sum_max_cong(mynode, allnodes);}
}

void exchange_results_sum_max_cong(int mynode, int allnodes) {
	//TODO inline...
	exchange_results2(mynode, allnodes);
}

void exchange_results_hist_max_cong(int mynode, int allnodes) {
	
	int size;
	int *bucket;
	MPI_Status status;
	
	if (mynode != 0) {
		bucket = get_bigbucket(&size);
		MPI_Send(&size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
		MPI_Send(bucket, size, MPI_INT, 0, 0, MPI_COMM_WORLD);
		free(bucket);
	}

	if (mynode == 0) {
		for (int counter = 1; counter < allnodes; counter++) {
			MPI_Recv(&size, 1, MPI_INT, counter, 0, MPI_COMM_WORLD, &status); //size
			bucket = (int *) malloc(size * sizeof(int));
			MPI_Recv(bucket, size, MPI_INT, counter, 0, MPI_COMM_WORLD, &status); //data
			add_to_bigbucket(bucket, size);
			free(bucket);
		}
	}
}

void simulation_with_metric(char *metric_name, ptrn_t *ptrn, namelist_t *namelist, int state) {
	if (strcmp(metric_name, "sum_max_cong") == 0) {simulation_sum_max_cong(ptrn, namelist, state);}
	if (strcmp(metric_name, "hist_max_cong") == 0) {simulation_hist_max_cong(ptrn, namelist, state);}
	if (strcmp(metric_name, "hist_acc_band") == 0) {simulation_hist_effective_bandwidth(ptrn, namelist, state);}
	if (strcmp(metric_name, "get_cable_cong") == 0) {simulation_get_cable_cong(ptrn, namelist, state);}
}

void merge_two_patterns_into_one(ptrn_t *ptrn1, ptrn_t *ptrn2, int comm1_size, ptrn_t *ptrn_res) {

	// typedef std::pair<int, int> pair_t;
	// typedef std::vector<pair_t> ptrn_t;

	ptrn_res->clear();
	ptrn_t::iterator iter_ptrn1;
	for (iter_ptrn1 = ptrn1->begin(); iter_ptrn1 != ptrn1->end(); iter_ptrn1++) {
		ptrn_res->push_back( std::make_pair(iter_ptrn1->first, iter_ptrn1->second ));
	}
	ptrn_t::iterator iter_ptrn2;
	for (iter_ptrn2 = ptrn2->begin(); iter_ptrn2 != ptrn2->end(); iter_ptrn2++) {
		ptrn_res->push_back(std::make_pair(iter_ptrn2->first + comm1_size, iter_ptrn2->second + comm1_size ));
	}
}

using namespace boost;
/* this is a node on the dependency graph */
typedef struct {
	int name; /* the name (rank) */
	int level; /* the level of the collective */
	int dist; /* dist from the last investigated root node */
} vertex_t;

struct vertex_info_t {
	typedef vertex_property_tag kind;
};
struct vertex_dist_t {
	typedef vertex_property_tag kind;
};
struct edge_load_t {
	typedef edge_property_tag kind;
};

typedef adjacency_list < vecS, vecS, directedS > dumgraph_t;

template < typename DistMap, typename LoadMap >
class bfs_edge_visitor:public default_bfs_visitor {
	typedef typename property_traits < DistMap >::value_type T;

public:
	bfs_edge_visitor(DistMap tmap, LoadMap tload, T & t):m_distmap(tmap), m_loadmap(tload), m_dist(t) { }

	//template < typename Vertex, typename Graph >
	//void initialize_vertex(Vertex v, Graph &g) const {
	//	printf("Initializing Vertex '%d': %i %i\n", v, get(m_distmap, out_degree(v, g)), get(m_distmap, v));
	//}

	//template < typename Vertex, typename Graph >
	//void discover_vertex(Vertex v, Graph &g) const {
	//	printf("Discovering Vertex '%d': %i,\n", v, get(m_distmap, v));
	//}

	template < typename Edge, typename Graph >
	void examine_edge(Edge e, const Graph & g) const {
		//int dist = get(m_distmap, g);
		//printf("Examining Edge: %d %d\n", get(m_distmap, source(e, g)), get(m_loadmap, e));
		put(m_distmap, target(e,g), get(m_distmap, source(e, g)) + get(m_loadmap, e));
	}

	DistMap m_distmap;
	LoadMap m_loadmap;
	T  & m_dist;
};


/* TODO: this function signature is ugly and could be built with the old
 * scheme and global variables and side effects */
void simulation_dep_max_delay(cmdargs_t *cmdargs, namelist_t *namelist, int valid_until, int myrank) {
	// Vertex properties - name
	typedef property < vertex_name_t, std::string, property < vertex_info_t, vertex_t, property < vertex_dist_t, int > > > vertex_p;
	// Edge properties - routing table as comment
	typedef property < edge_index_t, int, property < edge_load_t, int > > edge_p;
	// the graph
	typedef adjacency_list < vecS, vecS, directedS, vertex_p, edge_p > graph_t;
	// the graph
	graph_t graph(0);

	used_edges_t edge_list;
	cable_cong_map_t cable_cong;
	ptrn_t::iterator iter_ptrn;
	bucket_t bucket;

	property_map<graph_t, vertex_info_t>::type info = get(vertex_info_t(), graph);
	typedef property_map<graph_t, edge_load_t>::type ed_load_t;
	ed_load_t load = get(edge_load_t(), graph);
	property_map<graph_t, vertex_index_t>::type indexmap = get(vertex_index, graph);

	std::map<int,graph_traits <graph_t>::vertex_descriptor> prevleveldests; // destinations from the previous level

	int level=0;
	while (1) {
		ptrn_t ptrn;

		genptrn_by_name(&ptrn, cmdargs->args_info.ptrn_arg, cmdargs->ptrnarg,
		                cmdargs->args_info.commsize_arg, cmdargs->args_info.part_commsize_arg,
		                level++, myrank);

		if (ptrn.size() == 0) break;
		//printf("level: %i\n", level-1);

		if ((cmdargs->args_info.printptrn_given) && (myrank== 0)) { printptrn(&ptrn, namelist); }

		std::map<int,graph_traits <graph_t>::vertex_descriptor> thisleveldests; // destinations from this level
		std::map<int,graph_traits <graph_t>::vertex_descriptor> thislevelsources; // sources from this level


		// first step - fill cable congestion map
		cable_cong_map_t cable_cong;
		for (ptrn_t::iterator iter_ptrn = ptrn.begin(); iter_ptrn != ptrn.end(); iter_ptrn++) {
			uroute_t route;
			find_route(&route, namelist->at(iter_ptrn->first), namelist->at(iter_ptrn->second));
			insert_route_into_cable_cong_map(&cable_cong, &route);
		}

		// step two: build graph with weighted edges
		//  vertices are tuples of (level, rank)
		//  each pattern pair is an edge between two vertexes
		//  each source-destination pair in level x is connected with an
		//   edge with weight of the congestion
		//  each source (level x, rank) in level x which has a destination
		//   (level x-1, rank) is connected with an edge with weight
		for (ptrn_t::iterator iter_ptrn = ptrn.begin(); iter_ptrn != ptrn.end(); iter_ptrn++) {

			// only consider the first valid_until ranks - no communication
			// will cross this border (has to be guaranteed in pattern!)
			//printf("%i %i\n", iter_ptrn->first, valid_until);
			if((iter_ptrn->first >= valid_until) || (iter_ptrn->second >= valid_until)) continue;

			uroute_t route;
			find_route(&route, namelist->at(iter_ptrn->first), namelist->at(iter_ptrn->second));
			int weight;
			get_max_congestion(&route, &cable_cong, &weight);

			vertex_t source_vertex_prop, dest_vertex_prop;
			source_vertex_prop.name = iter_ptrn->first;
			source_vertex_prop.level = level;
			dest_vertex_prop.name = iter_ptrn->second;
			dest_vertex_prop.level = level;

			graph_traits <graph_t>::vertex_descriptor source_vertex = add_vertex(graph);
			graph_traits <graph_t>::vertex_descriptor dest_vertex = add_vertex(graph);
			put(info, source_vertex, source_vertex_prop);
			put(info, dest_vertex, dest_vertex_prop);
			//printf("Connection %d -> %d\n", iter_ptrn->first, iter_ptrn->second);
			//printf("Adding vertex descriptors: '%d' for src node '%d'\n"
			//       "                           '%d' for dst node '%d'\n",
			//       source_vertex, get(info, source_vertex).name,
			//       dest_vertex, get(info, dest_vertex).name);

			// add this level's destinations destination to prevleveldests
			thisleveldests.insert(std::pair<int,graph_traits <graph_t>::vertex_descriptor>(iter_ptrn->second,dest_vertex));
			thislevelsources.insert(std::pair<int,graph_traits <graph_t>::vertex_descriptor>(iter_ptrn->first,source_vertex));

			graph_traits <graph_t>::edge_descriptor new_edge;
			bool tmp;
			tie(new_edge, tmp) = add_edge(source_vertex, dest_vertex, graph);

			//printf("Put weight %d\n\n", weight);
			put(load, new_edge, weight);
		}

		//for(std::map<int,graph_traits <graph_t>::vertex_descriptor>::iterator iter = prevleveldests.begin(); iter != prevleveldests.end(); ++iter) {
		//	printf("%i\n", *iter);
		//}

		for(std::map<int,graph_traits <graph_t>::vertex_descriptor>::iterator iter = thislevelsources.begin(); iter != thislevelsources.end(); ++iter) {

			std::map<int,graph_traits <graph_t>::vertex_descriptor>::iterator prevleveldest = prevleveldests.find((*iter).first);

			if(prevleveldests.end() != prevleveldest) {
				//printf("%i in previous destinations\n", (*iter).first);
				// create edge between the two vertices
				graph_traits <graph_t>::edge_descriptor new_edge;
				bool tmp;
				tie(new_edge, tmp) = add_edge((*prevleveldest).second, (*iter).second, graph);

				put(load, new_edge, 0); // this edge has zero weight
			} else {
				//printf("%i not in previous destinations\n", (*iter).first);
			}
		}

		prevleveldests = thisleveldests;
	}
	//printf("weighted dependency graph built\n");

	// traverse graph from the roots and report longest path to any edge
	// TODO: there are cycles if there is cyclic communication in a level :-(
	{
		int max=0;
		for (graph_traits <graph_t>::vertex_descriptor v = 0; v < num_vertices(graph); v++) {

			/* do a dijkstra along every first communication edge */
			//std::vector<graph_traits<graph_t>::vertex_descriptor> p(num_vertices(graph));
			//std::vector<int> d(num_vertices(graph));

			/* compare function is greater, thus it searches longest paths! */
			//dijkstra_shortest_paths(graph, (*iter_ptrn).first, &p[0], &d[0], load, indexmap,
			//        std::less<int>(), closed_plus<int>(),
			//        (std::numeric_limits<int>::max)(), 0,
			//        default_dijkstra_visitor());

			// do BFS and attach distance to root to each vertex
			typedef property_map<graph_t, vertex_dist_t>::type vert_dist_t;
			vert_dist_t m_dist = get(vertex_dist_t(), graph);
			int dist = 0;

			bfs_edge_visitor <vert_dist_t, ed_load_t>vis(m_dist, load, dist);
			breadth_first_search(graph, v, visitor(vis));

			// loop over all vertices and find biggest time
			graph_traits<graph_t>::vertex_iterator viter, viter_end;
			for (tie(viter, viter_end) = vertices(graph); viter != viter_end; ++viter) {
				//for(int i = 0; i < num_vertices(graph); i++) {
				//  int dist = d[i];
				dist = get(m_dist, *viter);
				if(std::numeric_limits<int>::max() != dist) {
					if(dist > max) max=dist;
					//printf("dist: %i\n", dist);
				}
			}
		}
		// TODO: put into bin
		//printf("[%i] max: %i\n", myrank, max);
		account_stats_max_congestions(max);
	}
}

void simulation_hist_max_cong(ptrn_t *ptrn, namelist_t *namelist, int state) {
	used_edges_t edge_list;
	cable_cong_map_t cable_cong;
	ptrn_t::iterator iter_ptrn;
	bucket_t bucket;

	if (state == RUN) {
		int i = 0;
		for (iter_ptrn = ptrn->begin(); iter_ptrn != ptrn->end(); iter_ptrn++) {
			uroute_t route;
			i++;
			find_route(&route, namelist->at(iter_ptrn->first), namelist->at(iter_ptrn->second));
			insert_route_into_cable_cong_map(&cable_cong, &route);
#ifdef TDEBUG
			if (i % 100 == 1) {printf("[%i/%i]\n", i, ptrn->size());}
#endif
		}

		bucket.clear();
		insert_into_bucket_maxcon2(&cable_cong, ptrn, namelist, &bucket);
	}
}

void simulation_get_cable_cong(ptrn_t *ptrn, namelist_t *namelist, int state) {
	used_edges_t edge_list;
	cable_cong_map_t cable_cong;
	ptrn_t::iterator iter_ptrn;
	bucket_t bucket;

	if (state == RUN) {
		int i = 0;
		for (iter_ptrn = ptrn->begin(); iter_ptrn != ptrn->end(); iter_ptrn++) {
			uroute_t route;
			i++;
			find_route(&route, namelist->at(iter_ptrn->first), namelist->at(iter_ptrn->second));
			insert_route_into_cable_cong_map(&cable_cong, &route);
			apply_cable_cong_map_to_global_cable_cong_map(&cable_cong);
		}
	}
}


void simulation_hist_effective_bandwidth(ptrn_t *ptrn, namelist_t *namelist, int state) {
	used_edges_t edge_list;
	cable_cong_map_t cable_cong;
	ptrn_t::iterator iter_ptrn;
	static bucket_t bucket;

	if (state == RUN) {
		for (iter_ptrn = ptrn->begin(); iter_ptrn != ptrn->end(); iter_ptrn++) {
			uroute_t route;
			find_route(&route, namelist->at(iter_ptrn->first), namelist->at(iter_ptrn->second));
			insert_route_into_cable_cong_map(&cable_cong, &route);
		}

		std::sort(edge_list.begin(), edge_list.end());
		insert_into_bucket_maxcon2(&cable_cong, ptrn, namelist, &bucket);

		//		account_stats(&bucket);
		//		bucket.clear();

	}
	else if (state == ACCOUNT) {
		account_stats(&bucket);
		bucket.clear();
	}
}

void simulation_sum_max_cong(ptrn_t *ptrn, namelist_t *namelist, int state) {
	used_edges_t edge_list;
	cable_cong_map_t cable_cong;
	ptrn_t::iterator iter_ptrn;
	bucket_t bucket;
	static int sum_max_congestions = 0;

	if (state == RUN) {
		for (iter_ptrn = ptrn->begin(); iter_ptrn != ptrn->end(); iter_ptrn++) {
			uroute_t route;
			find_route(&route, namelist->at(iter_ptrn->first), namelist->at(iter_ptrn->second));
			insert_route_into_cable_cong_map(&cable_cong, &route);
		}

		std::sort(edge_list.begin(), edge_list.end());
		bucket.clear();
		insert_into_bucket_maxcon2(&cable_cong, ptrn, namelist, &bucket);

		int counter;
		int loc_max_congestion=0;
		for (counter=0; counter<bucket.size(); counter++) {
			if (bucket.at(counter) > 0) {loc_max_congestion = counter;}
		}
		sum_max_congestions += loc_max_congestion;
	}
	else if (state == ACCOUNT) {
		account_stats_max_congestions(sum_max_congestions);
		sum_max_congestions = 0;
	}
}

void print_namelist(namelist_t *namelist) {
	std::cout << "\n\nUsed subset of nodes: \n=================";
	if(namelist->size() == 0) printf(" namelist empty! ============\n");
	else {
		for (int i=0; i<namelist->size()-1; i++) {
			std::cout << "\n" << namelist->at(i);
		}
		std::cout << "\n" <<namelist->at(namelist->size()-1) << "\n===============\n\n";
	}
}

void find_route(uroute_t *route, std::string n1, std::string n2) {

	/**
 * This function returns the list of edges used for the communication from the
 * node named n1 to the node named n2 in a vector of edges.
 * */

	Agedge_t *e;
	Agnode_t *start;
	Agnode_t *dest;
	edge_t edge;
	edgeid_t edgeid;
	std::map<std::string, int> theMap;
	std::pair<std::map<std::string, int>::iterator, bool> theMap_returnval;

	start = agnode(mygraph, (char *) n1.c_str(), 0);
	dest = agnode(mygraph, (char *) n2.c_str(), 0);

	if ((start == NULL) || (dest == NULL)) {
		printf("I didn't find one of the hosts %s and %s!\n", n1.c_str(), n2.c_str());
		return;
	}
	while (start != dest) {
		//printf("Start: %s Dest:  %s\n", agnameof(start), agnameof(dest));
		for (e = agfstout(mygraph, start); e; e = agnxtout(mygraph, e)) {
			//printf("considering edge %s with comment %s\n", agnameof(e), agget(e, ((char *) "comment")));
			if (contains_target(agget(e, ((char *) "comment")), (char *) n2.c_str())) {
				//printf("Using edge: %s\n", agnameof(e));
				theMap_returnval = theMap.insert( std::make_pair(agnameof(aghead(e)), 1));
				if (theMap_returnval.second == false) {
					printf("I tried to visit a node I already visited on the same route. This means we have a routing loop!\n");
					FILE *fderr = fopen("routing_loops.txt", "a");
					if (fderr == NULL) { printf("Eeeek!\n"); exit(EXIT_FAILURE); }
					fprintf(fderr, "%s -> %s\n", agnameof(start), agnameof(dest));
					fclose(fderr);
					route->erase( route->begin(), route->end());
					return;
				}
				//edgeid.assign(agget(e, ((char *) "edge_id")));
				edgeid = atoi(agget(e, ((char *) "edge_id")));
				route->push_back(edgeid);
				start = aghead(e);
				break;
			}
		}
		if (e == NULL) {
			printf("There seems to be no route from %s to %s.\n", (char *) n1.c_str(), (char *) n2.c_str());
			break;
		}
	}
}

int contains_target(char *comment, char *target) {

	/**
 * This function checks if our target appears in the commma seperated list of
 * targets, encoded as a comment in the dot file. Returns 1 if yes, 0
 * otherwise. A single star '*' in the comment matches any target.
 * */

	//	printf("Comment: %s\n", commentin);
	//	printf("Target: %s\n", target);

	char *buffer;
	char *result;
	char *buffer2;
	//	char *comment;
	int i,j;

	/*	comment = (char *) malloc((strlen(commentin) + 1) * sizeof(char));
	j = 0;
	for (i=0; i<strlen(commentin); i++) {
		if ((commentin[i] != ' ') and
		   (commentin[i] != '\t') and
		   (commentin[i] != '\n')) {
			comment[j] = commentin[i];
			j++;
		}
	}
	comment[j]=0;
*/
	if (strcmp(comment, "*") == 0) return 1;
	buffer = (char *) malloc(strlen(comment) * sizeof(char) + 1);
	strcpy(buffer, comment);
	result = strtok(buffer, ", \t\n");
	
	while ((result != NULL) and (strcmp(result, target) != 0)) {
		result = strtok(NULL, ", \t\n");
		if (result == NULL) break;
	}
	free(buffer);
	//	free(comment);
	if (result == NULL) return 0;
	else return 1;
}

unsigned long long convert_nodename_to_guid(std::string nodename) {
	unsigned long long guid;

	/* The first character in the node names is always H.
	 * Remove the first character in order to allow the conversion from hex to long */
	nodename.erase(nodename.begin());

	/* Convert the string from hex to long and add in the guids array */
	guid = strtoul(nodename.data(), NULL, 16);

	return guid;
}

void get_guidlist_from_namelist(IN namelist_t *namelist,
                                OUT guidlist_t *guidlist) {

	/** This function gets a GUID list from the provided node namelist **/

	int i;

	guidlist->clear();

	for (i = 0; i < namelist->size(); i++)
		guidlist->push_back(convert_nodename_to_guid(namelist->at(i)));
}

void get_namelist_from_guidlist(IN guidlist_t *guidlist,
                                IN namelist_t *complete_namelist,
                                OUT namelist_t *namelist) {

	/** This function will return a namelist with the same order as
	 *  the one in the guidlist **/

	guidlist_t tmp_complete_guidlist;
	namelist_t tmp_complete_namelist;
	int i, j;

	namelist->clear();

	/* We do not want to alter the input, so copy the input in a temporary
	 * vector since we are going to make changes to it. */
	tmp_complete_namelist = *complete_namelist;

	/* Get a numeric GUID list from the complete list. We will use this
	 * list as a basis for our comparisons */
	get_guidlist_from_namelist(complete_namelist, &tmp_complete_guidlist);

	for (i = 0; i < guidlist->size(); i++) {
		for (j = 0; j < tmp_complete_guidlist.size(); j++) {
			if (guidlist->at(i) == tmp_complete_guidlist.at(j)) {
				namelist->push_back(tmp_complete_namelist.at(j));

				/* Remove the already added elements in order to potentially
				 * reduce the number of iterations in the next round */
				tmp_complete_guidlist.erase(tmp_complete_guidlist.begin() + j);
				tmp_complete_namelist.erase(tmp_complete_namelist.begin() + j);
				break;
			}
		}
	}
}

void get_namelist_from_graph(namelist_t *namelist) {
	get_namelist_from_graph(namelist, NULL);
}

void get_namelist_from_graph(OUT namelist_t *namelist,
                             OUT guidlist_t *guidlist) {
	
	/** This function places an array of strings (the node names) at the given
	 * position and returns the number of elements in that array. This list should
	 * be generated once and can be used for mapping the node names to integers. The
	 * list is NOT random.
	 **/
	
	Agnode_t *node;
	std::string nodename;

	node = agfstnode(mygraph);
	while (node != NULL) {
		nodename = (std::string) agnameof(node);
		if (nodename.find('H', 0) == 0) {
			namelist->push_back(nodename);
		}
		node = agnxtnode(mygraph, node);
	}

	/* If a guidlist has been provided (not NULL), then get a list
	 * of numeric GUIDs as well */
	if (guidlist)
		get_guidlist_from_namelist(namelist, guidlist);
}

void generate_random_namelist(OUT namelist_t *namelist,
                              IN int comm_size,
                              IN namelist_t *namelist_pool) {
	
	MTRand mtrand;
	namelist_t tmp_namelist;
	int counter;
	int pos;
	int myrand;

	/* If a namelist_pool is provided, read the name list from the
	 * namelist_pool. Otherwise, read from the agraph file */
	if (namelist_pool != NULL)
		tmp_namelist = *namelist_pool;
	else
		get_namelist_from_graph(&tmp_namelist);

	std::vector<bool> bucket(tmp_namelist.size(), false);
	
	for (counter=1; counter <= comm_size; counter++) {
		myrand = mtrand.randInt(tmp_namelist.size() - counter);
		pos=0;
		while (true) {
			if (bucket[pos] == false) {
				if (myrand == 0) {
					bucket[pos] = true;
					if (namelist->size() < comm_size) {
						namelist->push_back(tmp_namelist.at(pos));
					}
					break;
				}
				myrand--;
			}
			pos++;
		}
	}
}

void generate_linear_namelist_bfs(OUT namelist_t *namelist,
                                  IN int comm_size) {

	Agnode_t *node;
	std::queue<Agnode_t*> queue;
	// HTOR: didn't you say that one is not supposed to use the pointer
	// to the char returned by libagraph?
	std::map<char*, int> color;
	Agedge_t *e;
	
	namelist->clear();

	node = agfstnode(mygraph);
	queue.push(node);
	color[agnameof(node)] = 1;
	while (!queue.empty()) {
		node = queue.front();
		queue.pop();
		std::string nodename = (std::string) agnameof(node);
		if (nodename.find('H', 0) == 0) {
			if (namelist->size() < comm_size) {
				namelist->push_back(nodename);
			}
		}
		//mark_and_insert(node);
		for (e = agfstout(mygraph, node); e; e = agnxtout(mygraph, e)) {
			if (color[agnameof(aghead(e))] == 0) {
				queue.push(aghead(e));
				color[agnameof(aghead(e))] = 1;
			}
		}
	}
}

void generate_linear_namelist_guid_order(OUT namelist_t *namelist,
                                         IN int comm_size,
                                         IN namelist_t *namelist_pool,
                                         IN bool asc) {

	namelist_t tmp_namelist;
	guidlist_t guids, sorted_guids;
	unsigned long long curr_guid;
	int counter, pos;

	/* If a namelist_pool is provided, read the namelist from the namelist_pool.
	 * Otherwise read the namelist from the agraph file. Also, get a list of the
	 * numeric GUIDs as well in order to be able to sort the guids numerically. */
	if (namelist_pool) {
		tmp_namelist = *namelist_pool;
		get_guidlist_from_namelist(&tmp_namelist, &guids);
	}
	else
		get_namelist_from_graph(&tmp_namelist, &guids);

	/* Copy the current guids list in a new list that we are going to sort.
	 * Note: the equal operator copies a vector by value and not by reference,
	 * which is what we want in this case. */
	sorted_guids = guids;

	/* Sort the numeric GUIDs */
	if (asc)
		/* In ascending order... */
		std::sort(sorted_guids.begin(), sorted_guids.end());
	else
		/* In descending order... */
		std::sort(sorted_guids.begin(), sorted_guids.end(), std::greater<unsigned long long>());

	/* Find the first comm_size nodes and push them into the namelist */
	for (counter=0; counter < comm_size; counter++) {
		/* Use the curr_guid from the 'sorted_guids' vector as the guid
		 * that needs to be matched and pushed in the namelist vector. */
		curr_guid = sorted_guids.at(counter);
		for(pos = 0; pos < tmp_namelist.size(); pos++) {
			/* Iterate through the temporary namelist vector */
			if (curr_guid == guids.at(pos)) {
				/* If the guid matches the node in the current position,
				 * add the node in the namelist vector and remove it from
				 * the 'tmp_namelist' and 'guids' vectors. in order to
				 * reduce the potential number of iterations in the next
				 * round */
				namelist->push_back(tmp_namelist.at(pos));
				tmp_namelist.erase(tmp_namelist.begin() + pos);
				guids.erase(guids.begin() + pos);
				break;
			}
		}
	}
}

void shuffle_namelist(namelist_t *namelist) {
	
	std::vector<bool> bucket(namelist->size(), false);
	namelist_t shuffled_list;
	MTRand mtrand;
	int myrand;

	for (int counter=1; counter <= namelist->size(); counter++) {
		myrand = mtrand.randInt(namelist->size() - counter);
		int pos=0;
		while (true) {
			if (bucket[pos] == false) {
				if (myrand == 0) {
					bucket[pos] = true;
					shuffled_list.push_back(namelist->at(pos));
					break;
				}
				myrand--;
			}
			pos++;
		}
	}
	namelist->swap(shuffled_list);
}

void insert_route_into_cable_cong_map(cable_cong_map_t *cable_cong, uroute_t *route) {

	uroute_t::iterator iter_route;
	for (iter_route = route->begin(); iter_route != route->end(); iter_route++) {
		std::pair<cable_cong_map_t::iterator, bool> ret;

		ret = cable_cong->insert(std::make_pair(*iter_route, 1));
		if (ret.second == false) {
			ret.first->second = ret.first->second + 1;
		}
	}
} 

std::string lookup(int nodenumber, namelist_t *namelist) {
	namelist_t::iterator iter;
	int counter = 0;

	for (iter = namelist->begin(); iter != namelist->end(); iter++) {
		if (counter == nodenumber) return *iter;
		counter++;
	}

	return *iter;
}

void get_max_congestion(uroute_t *route, cable_cong_map_t *cable_cong, int *weight) {

	uroute_t::iterator route_iter;
	*weight = 0;
	int loc_weight = 0;

	/* go over the physical edges of the route */
	for (route_iter = route->begin(); route_iter != route->end(); route_iter++) {

		cable_cong_map_t::iterator it;
		it = cable_cong->find(*route_iter);
		if (it == cable_cong->end()) {
			printf("There has been a serious error: Route contained entry not in cable_cong\n");
			exit(EXIT_FAILURE);
		}
		if (loc_weight < it->second) {
			loc_weight = it->second;
		}
	}
	*weight = loc_weight;
}

void my_mpi_init(int *argc, char **argv[], int *rank, int *comm_size) {
	int *recvbuf_id, i, name_len;
	char processor_name[MPI_MAX_PROCESSOR_NAME], *recvbuf_proc_name;

	MPI_Init(argc, argv);
	MPI_Comm_size(MPI_COMM_WORLD, comm_size);
	MPI_Comm_rank(MPI_COMM_WORLD, rank);

	MPI_Get_processor_name(processor_name, &name_len);

	if (*rank == 0) {
		recvbuf_id = (int *) malloc(*comm_size * sizeof(*rank));
		recvbuf_proc_name = (char *) malloc(*comm_size * MPI_MAX_PROCESSOR_NAME * sizeof(*processor_name));
	}

	MPI_Gather(rank, 1, MPI_INT,
	           recvbuf_id, 1, MPI_INT,
	           0, MPI_COMM_WORLD);
	MPI_Gather(processor_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR,
	           recvbuf_proc_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR,
	           0, MPI_COMM_WORLD);

	if (*rank == 0) {
		printf("Total MPI nodes participating in the simulation: '%d'\n", *comm_size);
		printf("Hello from Master MPI node '%s' with rank '%d' (%d/%d)\n",
		       recvbuf_proc_name, *rank, *rank + 1, * comm_size);
		for (i = 1; i < *comm_size; i++)
			printf("Hello from MPI node '%s' with rank '%d' (%d/%d)\n",
			       recvbuf_proc_name + MPI_MAX_PROCESSOR_NAME * i,
			       recvbuf_id[i], recvbuf_id[i] + 1, * comm_size);

		free(recvbuf_id);
		free(recvbuf_proc_name);
	}
}

void read_input_graph(char *filename, int my_mpi_rank) {
	FILE *fd;
	char *graph_buffer, *tmp_realloc;
	unsigned long fsize = 0;

	if (my_mpi_rank == 0) {

		if (strcmp(filename, "-") == 0) {

			int graph_bufsize = CHARBUF_INCREMENT_SIZE * sizeof(*graph_buffer);
			char fgetsbuf[READCHAR_BUFFER];
			graph_buffer = (char *) calloc(CHARBUF_INCREMENT_SIZE, sizeof(*graph_buffer));
			if (graph_buffer == NULL)
				goto exit;
			fd = stdin;

			while(fgets(fgetsbuf, READCHAR_BUFFER, fd)) { /* Read until EOF */
				if ((fsize + strlen(fgetsbuf) + 1) >= graph_bufsize) {
					/* Increase the graph_buffer size */
					graph_bufsize += CHARBUF_INCREMENT_SIZE * sizeof(*graph_buffer);
					tmp_realloc = (char *) realloc(graph_buffer, graph_bufsize);
					if (tmp_realloc == NULL)
						goto exitrealloc;
					graph_buffer = tmp_realloc;
				}
				memcpy(graph_buffer + fsize, fgetsbuf, strlen(fgetsbuf));
				fsize += strlen(fgetsbuf);
			}
		} else {
			struct stat st;

			stat(filename, &st);
			fsize =	st.st_size;
			graph_buffer = (char *) malloc(fsize * sizeof(*graph_buffer));
			if (graph_buffer == NULL)
				goto exit;

			fd = fopen(filename, "r");
			if (fd == NULL) {
				fprintf(stderr, "ERROR: Could not open input file '%s'\n", filename);
				MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			}

			/* Read the whole file at once in the buffer */
			fread(graph_buffer, sizeof(char), fsize, fd);

			fclose(fd);
		}

	}

	/* bcast buffer size */
	MPI_Bcast(&fsize, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
	if(my_mpi_rank != 0) {
		graph_buffer = (char *)malloc(fsize * sizeof(*graph_buffer));
		if (graph_buffer == NULL)
			goto exit;
	}

	/* bcast buffer data */
	MPI_Bcast(graph_buffer, fsize, MPI_CHAR, 0, MPI_COMM_WORLD);

	//printf("file-size: %lu\n", fsize);
	mygraph = agmemread(graph_buffer);

	free(graph_buffer);

	return;

exitrealloc:
	free(graph_buffer);
exit:
	fprintf(stderr, "ERROR: Could not allocate memory for graph_buffer\n");
	MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
}

void tag_edges(Agraph_t *mygraph) {
	Agedge_t *e;
	Agnode_t *n;
	int id_cnt;
	char edge_id[64];
	
	id_cnt = 0;
	agattr(mygraph, AGEDGE, (char *) "edge_id", (char *) "");
	n = agfstnode(mygraph);
	while (n != NULL) {
		e = agfstout(mygraph, n);
		while (e != NULL) {
			sprintf(edge_id, "%d", id_cnt);
			id_cnt++;
			agset(e, (char *) "edge_id", edge_id);
			//printf("Edge-ID: %s\n", agget(e, (char *) "edge_id"));
			e = agnxtout(mygraph, e);
		}
		n = agnxtnode(mygraph, n);
	}
}

void read_node_ordering(IN char *filename,
                        OUT guidlist_t *guidorder_list) {

	char line[PARSE_GUID_BUFLEN];
	FILE *fd;

	if (strcmp(filename, "-") == 0)
		return;

	if (!(fd = fopen(filename, "r"))) {
		printf("Could not open node ordering file %s\n", filename);
		exit(EXIT_FAILURE);
	}

	while (fgets(line, sizeof(line), fd)) {
		u_int64_t guid;
		char *p, *e;

		p = line;
		while (isspace(*p))
			p++;
		if (*p == '\0' || *p == '\n' || *p == '#')
			continue;

		guid = strtoull(p, &e, 16);
		if (e == p || (!isspace(*e) && *e != '#' && *e != '\0')) {
			fclose(fd);
			printf("Error when reading from file %s\n"
			       "GUID %s looks like it is not a valid GUID (expected a valid hex number per line in the file)\n",
			       filename, p);
			exit(EXIT_FAILURE);
		}

		p = e;
		while (isspace(*p))
			p++;

		e = strpbrk(p, "\n");
		if (e)
			*e = '\0';

		guidorder_list->push_back(guid);
	}

	fclose(fd);
}

void write_graph_with_congestions() {

	Agedge_t *e;
	Agnode_t *n;
	char cong_str[64];
	char color_str[128];

	agattr(mygraph, AGEDGE, (char *) "congestion", (char *) "");
	agattr(mygraph, AGEDGE, (char *) "color", (char *) "");
	n = agfstnode(mygraph);
	while (n != NULL) {
		e = agfstout(mygraph, n);
		while (e != NULL) {
			char *eid = agget(e, (char *) "edge_id");
			float cong = get_congestion_by_edgeid(atoi(eid));
			cong /= get_max_from_global_cong_map();
			sprintf(cong_str, "%f", cong);
			cong = 1 - cong;
			agset(e, (char *) "congestion", cong_str);
			float h = cong * 0.4;
			float s = 0.9;
			float v = 0.9;
			sprintf(color_str, "%f %f %f", h, s, v);
			agset(e, (char *) "color", (char *) &color_str);
			e = agnxtout(mygraph, e);
		}
		n = agnxtnode(mygraph, n);
	}
	agwrite(mygraph, stdout);
}

void bcast_guidlist(guidlist_t *guidlist, int my_mpi_rank) {
	int count = 0, i;
	unsigned long long *buffer;

	/* Pack buffer on rank 0 */
	if(my_mpi_rank == 0) {
		buffer = (unsigned long long *)malloc(guidlist->size() * sizeof(guidlist->at(0)));
		//unsigned long long *pos = buffer;
		for (i = 0; i < guidlist->size(); i++)
			buffer[i] = guidlist->at(i);
	}

	/* bcast buffer size */
	MPI_Bcast(&count, 1, MPI_INT, 0, MPI_COMM_WORLD);
	if(my_mpi_rank != 0)
		buffer = (unsigned long long *)malloc(count);

	/* bcast buffer data */
	MPI_Bcast(buffer, count, MPI_UNSIGNED_LONG_LONG, 0, MPI_COMM_WORLD);

	/* unpack buffer data on clients */
	if(my_mpi_rank != 0) {
		for (i = 0; i < count; i++)
			guidlist->push_back(buffer[i]);
	}

	free(buffer);
}

void bcast_namelist(namelist_t *namelist, int my_mpi_rank) {
	int count = 0, i;
	char *buffer;
	
	/* Pack buffer on rank 0 */
	if(my_mpi_rank == 0) {
		for (i = 0; i < namelist->size(); i++) {
			count += strlen(namelist->at(i).c_str()) + 1;
		}

		buffer = (char *) malloc(count * sizeof(char));
		char *pos = buffer;
		for (i = 0; i < namelist->size(); i++) {
			strcpy(pos, namelist->at(i).c_str());
			pos += strlen(namelist->at(i).c_str()) + 1;
		}
	}

	/* bcast buffer size */
	MPI_Bcast(&count, 1, MPI_INT, 0, MPI_COMM_WORLD);
	if(my_mpi_rank != 0)
		buffer = (char *) malloc(count * sizeof(char));

	/* bcast buffer data */
	MPI_Bcast(buffer, count, MPI_CHAR, 0, MPI_COMM_WORLD);

	/* unpack buffer data on clients */
	if(my_mpi_rank != 0) {
		char *pos = buffer;
		while (pos < buffer + count) {
			namelist->push_back(pos);
			pos += namelist->back().length() + 1;
		}
	}
	
	free(buffer);
}

void print_commandline_options(FILE *fd, cmdargs_t *cmdargs) {
	fprintf(fd, "Input File: %s\n", cmdargs->args_info.input_file_arg);
	fprintf(fd, "Output File: %s\n", cmdargs->args_info.output_file_arg);
	fprintf(fd, "Commsize: %d\n", cmdargs->args_info.commsize_arg);
	if (strcmp(cmdargs->args_info.ptrn_arg, "ptrnvsptrn") == 0) {
		ptrnvsptrn_t ptrnvsptrn = *((ptrnvsptrn_t *)cmdargs->ptrnarg);

		fprintf(fd, "Pattern: %s\n", cmdargs->args_info.ptrn_arg);
		fprintf(fd, "    First Pattern: %s%s%s\n", ptrnvsptrn.ptrn1,
		        ptrnvsptrn.ptrnarg1 ? "," : "",
		        ptrnvsptrn.ptrnarg1 ? ptrnvsptrn.ptrnargstr1 : "");
		fprintf(fd, "   Second Pattern: %s%s%s\n", ptrnvsptrn.ptrn2,
		        ptrnvsptrn.ptrnarg2 ? "," : "",
		        ptrnvsptrn.ptrnarg2 ? ptrnvsptrn.ptrnargstr2 : "");
	} else {
		fprintf(fd, "Pattern: %s%s%s\n", cmdargs->args_info.ptrn_arg,
		        cmdargs->ptrnarg ? "," : "",
		        cmdargs->ptrnarg ? cmdargs->args_info.ptrnarg_arg : "");
	}
	fprintf(fd, "Level: %d\n", cmdargs->args_info.ptrn_level_arg);
	fprintf(fd, "Runs: %d\n", cmdargs->args_info.num_runs_arg);
	fprintf(fd, "Subset: %s\n", cmdargs->args_info.subset_arg);
	fprintf(fd, "Metric: %s\n", cmdargs->args_info.metric_arg);
	fprintf(fd, "Part_commsize: %i\n\n", cmdargs->args_info.part_commsize_arg);
}

void print_results(cmdargs_t *cmdargs, int mynode, int allnodes) {

	if (mynode == 0) {
		char *filename = cmdargs->args_info.output_file_arg;
		if (strcmp(filename, "-") == 0) {
			if (strcmp(cmdargs->args_info.metric_arg, "dep_max_delay") == 0) {print_statistics_max_delay(stdout);}
			if (strcmp(cmdargs->args_info.metric_arg, "sum_max_cong") == 0) {print_statistics_max_congestions(stdout);}
			if (strcmp(cmdargs->args_info.metric_arg, "hist_acc_band") == 0) {print_histogram(stdout);}
			if (strcmp(cmdargs->args_info.metric_arg, "hist_max_cong") == 0) {printbigbucket(stdout);}
			if (strcmp(cmdargs->args_info.metric_arg, "get_cable_cong") == 0) {write_graph_with_congestions();}
		}
		else {
			FILE *fd;

			fd = fopen(filename, "w");
			if (fd == NULL) {
				std::cout << "Could not open output file " << filename << std::endl << std::flush;
				exit(EXIT_FAILURE);
			}
			else {
				print_commandline_options(fd, cmdargs);
				if (strcmp(cmdargs->args_info.metric_arg, "hist_acc_band") == 0) {print_histogram(fd);}
				if (strcmp(cmdargs->args_info.metric_arg, "sum_max_cong") == 0) {print_statistics_max_congestions(fd);}
				if (strcmp(cmdargs->args_info.metric_arg, "dep_max_delay") == 0) {print_statistics_max_delay(fd);}
				if (strcmp(cmdargs->args_info.metric_arg, "hist_max_cong") == 0) {printbigbucket(fd);}
				if (strcmp(cmdargs->args_info.metric_arg, "get_cable_cong") == 0) {print_cable_cong(fd);}
				fclose(fd);
			}
		}
	}
}

void exchange_results2(int mynode, int allnodes) {
	
	int size;

	double *buffer = get_results(&size);
	double *recvbuf = (double *) malloc(size * allnodes * sizeof(double));

	/*
	if (mynode == 0) {
		for (int p=1; p<allnodes; p++) {
			MPI_Status status;
			MPI_Recv(buffer, size, MPI_DOUBLE, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
			insert_results(buffer, size);
		}
	}
	else {
		MPI_Send(buffer, size, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
	}
*/
	MPI_Gather(buffer, size, MPI_DOUBLE, recvbuf, size, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	if (mynode == 0) {
		insert_results(recvbuf, size * allnodes);
	}
}

void generate_namelist_by_name(IN char *method,
                               OUT namelist_t *namelist,
                               IN int comm_size,
                               IN namelist_t *namelist_pool) {
	if (strcmp(method, "rand") == 0)
		generate_random_namelist(namelist, comm_size, namelist_pool);

	else if (strcmp(method, "linear_bfs") == 0)
		generate_linear_namelist_bfs(namelist, comm_size);

	else if (strcmp(method, "guid_order_asc") == 0)
		generate_linear_namelist_guid_order(namelist, comm_size, namelist_pool);

	else if (strcmp(method, "guid_order_desc") == 0)
		generate_linear_namelist_guid_order(namelist, comm_size, namelist_pool, false);
}

/* quick and dirty allreduce for maps<int,int> where the maximum key is
 * not too large and the keyspace is rather dense */
void allreduce_contig_int_map(std::map<int,int> *map) {
	// find maximum key element in map
	unsigned int max = 0;
	for(std::map<int, int>::iterator i=map->begin(); i!=map->end(); i++) {
		if(i->first > max) max = i->first;
	}

	// allreduce maximum key element
	int gmax;
	MPI_Allreduce (&max, &gmax, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
	//printf("max: %i %i\n", max, gmax);

	int *sfield = (int*)calloc(gmax+1,sizeof(int));
	int *rfield = (int*)calloc(gmax+1,sizeof(int));
	// fill the field with the map contents
	for(std::map<int, int>::iterator i=map->begin(); i!=map->end(); i++) {
		assert(i->first <= gmax);
		sfield[i->first] = i->second;
	}

	MPI_Allreduce(sfield, rfield, gmax+1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

	for(int i=0; i<gmax+1;i++) {
		if(rfield[i]) (*map)[i] = rfield[i];
	}

	free(sfield);
	free(rfield);
}
