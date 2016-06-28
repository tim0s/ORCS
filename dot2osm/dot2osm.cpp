/*
 * Copyright (c) 2009 The Trustees of Indiana University and Indiana
 *                    University Research and Technology
 *                    Corporation.  All rights reserved.
 *
 * Author(s): Timo Schneider <timoschn@cs.indiana.edu>
 *            Torsten Hoefler <htor@cs.indiana.edu>
 *
 */

#include <map>
#include <string>
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>
#include <string.h>
#include <graphviz/cgraph.h>

typedef std::map<int, int> edgeinfo_table_t;

Agraph_t *mygraph;

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
			id_cnt++;
			sprintf(edge_id, "%d", id_cnt);
			agset(e, (char *) "edge_id", edge_id);		
			e = agnxtout(mygraph, e);
        }
        n = agnxtnode(mygraph, n);
    }
}

void read_input_graph(char *filename) {

	if (strcmp(filename, "-") == 0) {
		mygraph = agread(stdin, 0);
	}
	else {
		FILE *fd;

		fd = fopen(filename, "r");
		if (fd == NULL) {
			fprintf(stderr, "Could not open input file %s \n", filename);
			exit(EXIT_FAILURE);
		}
		else {
			mygraph = agread(fd, 0);
			fclose(fd);
		}
	}
	/* now we should have a nice graph */
	assert(mygraph != NULL);
}

bool is_id_in_list(edgeinfo_table_t *edgeinfo, int id) {
	
	edgeinfo_table_t::iterator iter;

	iter = edgeinfo->find(id);
	if (iter == edgeinfo->end()) {
		return false;
	}
	return true;		
}

int get_partner_edge_id(edgeinfo_table_t *edgeinfo, int id) {
	
	edgeinfo_table_t::iterator iter;

	iter = edgeinfo->find(id);
	assert(iter != edgeinfo->end());

	return iter->second;
}

int get_edge_id(Agedge_t *edge) {
	
	int edge_id;
	char *edge_id_str;

	edge_id_str = agget(edge, (char *) "edge_id");
	edge_id = strtol(edge_id_str, NULL, 10);
	assert(edge_id > 0);
	assert(edge_id <= agnedges(mygraph));
	
	/* strtol returns 0 if something went wrong */
	assert(edge_id != 0);
	return edge_id;
}

int find_partner_edge(Agraph_t *mygraph, Agedge_t *edge, edgeinfo_table_t *edgeinfo) {
	
	Agnode_t *node_head, *node_tail;
	Agedge_t *partner_edge;
	int id = -1;

	node_head = aghead(edge);
	node_tail = agtail(edge);

	/* finding a partner won't work if we allready assigned one */
	if (is_id_in_list(edgeinfo, get_edge_id(edge))) {
		return get_partner_edge_id(edgeinfo, get_edge_id(edge));
	}

	partner_edge = agfstout(mygraph, node_head);
	while (partner_edge != NULL) {
		if (strcmp(agnameof(node_tail), agnameof(aghead(partner_edge))) == 0) {
			/* we found a potential partner edge: it is going in the right direction,
		   	but we need to check if it is already a partner edge for another edge */
			id = get_edge_id(partner_edge);			
			if (is_id_in_list(edgeinfo, id) == false) {
				/* it is not, so we can just return it as the partner edge of "edge" */
				return id;
			}
			/* this edge was already taken, let's try the next one */
			partner_edge = agnxtout(mygraph, partner_edge);
		}
		else {
			partner_edge = agnxtout(mygraph, partner_edge);
		}
	}
	/* we should never be here, that means there are edges without partners,
	   which is impossible for IB networks, as the cables are bi-directional */
	fprintf(stderr, "Problem with finding a partner edge\n");
	int dir1 = 0;
	int dir2 = 0;
	Agedge_t *tmpedge = agfstout(mygraph, node_tail);
	while (tmpedge != NULL) {
		if (strcmp(agnameof(aghead(tmpedge)), agnameof(node_head)) == 0) {
			dir1++;
		}
		tmpedge = agnxtout(mygraph, tmpedge);
	}
	tmpedge = agfstout(mygraph, node_head);
	while (tmpedge != NULL) {
		if (strcmp(agnameof(aghead(tmpedge)), agnameof(node_tail)) == 0) {
			dir2++;
		}
		tmpedge = agnxtout(mygraph, tmpedge);
	}
	fprintf(stderr, "I couldn't find a partner edge for an edge going from %s to %s.\n", agnameof(node_tail), agnameof(node_head));
	fprintf(stderr, "There are %i edges going from %s to %s\n", dir1, agnameof(node_tail), agnameof(node_head));
	fprintf(stderr, "There are %i edges going from %s to %s\n", dir2, agnameof(node_head), agnameof(node_tail));
	fprintf(stderr, "Please fix the input file and rerun the converter\n");
	return id;
}

void add_to_list(int id, int partner_id, edgeinfo_table_t *edgeinfo) {
	
	std::pair<edgeinfo_table_t::iterator, bool> ret;
	
	ret = edgeinfo->insert(std::make_pair(id, partner_id));
	/* it should never happen that we try to insert
	   something which is already there */
	assert(ret.second == true);
}

void fill_edgeinfo_table(Agraph_t *mygraph, edgeinfo_table_t *edgeinfo) {
	
	Agnode_t *node;
	Agedge_t *edge;
	int ncnt = 1;

	/* iterate over all nodes */
	node = agfstnode(mygraph);
	while (node != NULL) {
		printf("\r");
		printf("processing node %i out of %i\n", ncnt++, agnnodes(mygraph));
		/* iterate over all out-edges of that node */
		edge = agfstout(mygraph, node);
		while (edge != NULL) {
			int id = get_edge_id(edge);
			if (is_id_in_list(edgeinfo, id) == false) {
				/* the edge is not in the list yet, that means we didn't
				   assign a partner edge yet. We will do so now and put both
				   in our list */
				int partner_id = find_partner_edge(mygraph, edge, edgeinfo);
				add_to_list(id, partner_id, edgeinfo);
				add_to_list(partner_id, id, edgeinfo);
			}
			edge = agnxtout(mygraph, edge);		
		}
		node = agnxtnode(mygraph, node);		
	}

}

int get_remote_portnumber(Agraph_t *mygraph, Agedge_t *edge, edgeinfo_table_t *edgeinfo) {
	
	/* we need to get the remote port number of "edge"'s partner edge */

	int edgeid = get_edge_id(edge);
	int partnerid = get_partner_edge_id(edgeinfo, edgeid);

	/* if "edge" goes from u to v, it's partner edge has to go from v to u,
	   so we take v and iterate over all out-edges until we found the one with 
	   the id partnerid and count the edges over which we iterated starting from
	   one. That will be our port number */
	
	int portnum = 0;
	Agnode_t *head = aghead(edge);
	Agedge_t *outedge = agfstout(mygraph, head);
	while (outedge != NULL) {
		portnum++;
		if (partnerid == get_edge_id(outedge)) {
			return portnum;
		}
		outedge = agnxtout(mygraph, outedge);
	}

	/* we should never reach this line, because it means
	   we didn't find an edge meeting our conditions descriobed above
	   which means we have an error in the partner edge detection */	
	assert(0 == 1);
	return 0;
}

void write_node_info(FILE *fd, Agraph_t *mygraph, Agnode_t *node, edgeinfo_table_t *edgeinfo) {

	/* The output file format: 

	May <S> be the starting symbol, then the following
	grammar describes the input file:

	<S> ::= [ <node header line> [<connected port line>]* [<newline>]+ ]+ ;
	<node header line> ::= <type> " " <ports> " " <id> "\n" ;
	<type> ::= "Switch" | "Hca" ;
	<ports> ::= number of ports the switch has
	<id> ::= unique string enclosed in quotation marks
	<newline> ::= an empty line
	<connected port line> ::= "[" <localport> "] " <remoteid> " [" <remoteport>"]\n" ;
	<localport> ::= local port number
	<remoteid> ::= id of remote note in quotation marks
	<remoteport> ::= remote port number */
	
	/* Get the nodes type */
	char *nodename = agnameof(node);
	if (strncmp(nodename, "H", 1) == 0) {
		fprintf(fd, "Hca ");
	}
	else {
		fprintf(fd, "Switch ");
	}
	/* get the number of connected ports, this is equal
	   to the number of out-edges in the graph */
	fprintf(fd, "%i ", agdegree(mygraph, node, 0, 1));
	/* the nodes nname */
	fprintf(fd, "\"%s\"\n", nodename);
	
	int cnt = 0;
	Agedge_t *edge = agfstout(mygraph, node);
	while (edge != NULL) {
		cnt++;
		fprintf(fd, "[%i] \"%s\"", cnt, agnameof(aghead(edge)));
		/* NOTE: If there is whitespace between the nodename and the remoteport
		 * ibsim will not work... */
		fprintf(fd, "[%i]\n", get_remote_portnumber(mygraph, edge, edgeinfo));
		edge = agnxtout(mygraph, edge);
	}
	/* an empty line to seperate nodes */
	fprintf(fd, "\n");
}

FILE *open_output_file(char *filename) {

	FILE *fd;

	if (strcmp(filename, "-") == 0) {
		fd = stdout;
	}
	else {
		fd = fopen(filename, "w");
		if (fd == NULL) {
			fprintf(stderr, "Could not open output file %s \n", filename);
			exit(EXIT_FAILURE);
		}
	}
	return fd;
}

void show_usage(char *progname) {
	printf("Usage: \n\n\t%s inputfile outputfile\n\n", progname);
	printf("inputfile: Name of the inputfile. The input file must be in dot format,\n");
	printf("\tif you specify \"-\" as filename the input is red from STDIN.\n\n");
	printf("outputfile: Name of the outputfile, will be in an OpenSM specific format\n");
	printf("\tif you specify \"-\" as filename the output is written to STDOUT.\n\n");
}

int main(int argc, char **argv) {

	if (argc != 3) {
		show_usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	
	read_input_graph(argv[1]);
	
	/* We need to be able to uniquely identify every edge, since we could deal
	with multigraphs and the head and tail of an edge are not sufficient in this
	case. Therefore we "tag" all edges with an "edge_id" property */
	
	tag_edges(mygraph);
	
	/* Our input graph is a directed graph but for IB's symetric "Port X is
	connected to Port Y" modell it is necessary to know the reverse edge for
	every edge in the graph (we assume the always exist since IB links are
	bi-directional). Since our input graph could be a multigraph (two nodes
	could be directly connected with more than one edge) this task is
	non-trivial. We solve it by filling a table which associates every edge-id
	with it's "partner" edge-id. */

	edgeinfo_table_t edgeinfo_table;
	printf("filling edgeinfo table\n");
	fill_edgeinfo_table(mygraph, &edgeinfo_table);
	
	/* now we have gathered all information we need to write the output file */

	int ncnt = 1;
	FILE *outfile = open_output_file(argv[2]);

	printf("\nWriting osm topology file:\n");
	Agnode_t *node = agfstnode(mygraph);
	while (node != NULL) {
		printf("\r");
		printf("processing node %i out of %i\n", ncnt++, agnnodes(mygraph));
		write_node_info(outfile, mygraph, node, &edgeinfo_table);
		node = agnxtnode(mygraph, node);
	}
	fclose(outfile);
}
