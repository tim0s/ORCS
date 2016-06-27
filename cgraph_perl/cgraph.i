/*
 * Copyright (c) 2009 The Trustees of Indiana University and Indiana
 *                    University Research and Technology
 *                    Corporation.  All rights reserved.
 *
 * Author(s): Timo Schneider <timoschn@cs.indiana.edu>
 *            Torsten Hoefler <htor@cs.indiana.edu>
 *
 */


%module cgraph

%{
#include <string.h>
#include <stdlib.h>
#include <graphviz/cgraph.h>
%}

/* ONLY A FRACTION OF THOSE IS SUPPORTED/TESTED RIGHT NOW */

extern void agpushdisc(Agraph_t * g, Agcbdisc_t * disc, void *state);
extern int agpopdisc(Agraph_t * g, Agcbdisc_t * disc);
extern int agcallbacks(Agraph_t * g, int flag);	/* return prev value */

/* graphs */
extern Agraph_t *agopen(char *name, Agdesc_t desc, Agdisc_t * disc);
extern int agclose(Agraph_t * g);
extern Agraph_t *agread(void *chan, Agdisc_t * disc);
extern void agreadline(int);
extern void agsetfile(char *);
extern Agraph_t *agconcat(Agraph_t * g, void *chan, Agdisc_t * disc);
extern int agwrite(Agraph_t * g, void *chan);
extern int agisdirected(Agraph_t * g);
extern int agisundirected(Agraph_t * g);
extern int agisstrict(Agraph_t * g);
extern int agissimple(Agraph_t * g);

/* nodes */
extern Agnode_t *agnode(Agraph_t * g, char *name, int createflag);
extern Agnode_t *agidnode(Agraph_t * g, unsigned long id, int createflag);
extern Agnode_t *agsubnode(Agraph_t * g, Agnode_t * n, int createflag);
extern Agnode_t *agfstnode(Agraph_t * g);
extern Agnode_t *agnxtnode(Agraph_t * g, Agnode_t * n);
extern Agnode_t *aglstnode(Agraph_t * g);
extern Agnode_t *agprvnode(Agraph_t * g, Agnode_t * n);

extern Agsubnode_t *agsubrep(Agraph_t * g, Agnode_t * n);

/* edges */
extern Agedge_t *agedge(Agraph_t * g, Agnode_t * t, Agnode_t * h,
			char *name, int createflag);
extern Agedge_t *agidedge(Agraph_t * g, Agnode_t * t, Agnode_t * h,
			  unsigned long id, int createflag);
extern Agedge_t *agsubedge(Agraph_t * g, Agedge_t * e, int createflag);
extern Agedge_t *agfstin(Agraph_t * g, Agnode_t * n);
extern Agedge_t *agnxtin(Agraph_t * g, Agedge_t * e);
extern Agedge_t *agfstout(Agraph_t * g, Agnode_t * n);
extern Agedge_t *agnxtout(Agraph_t * g, Agedge_t * e);
extern Agedge_t *agfstedge(Agraph_t * g, Agnode_t * n);
extern Agedge_t *agnxtedge(Agraph_t * g, Agedge_t * e, Agnode_t * n);

/* generic */
extern Agraph_t *agraphof(void* obj);
extern Agraph_t *agroot(void* obj);
extern int agcontains(Agraph_t *, void *);
extern char *agnameof(void *);
extern int agrelabel(void *obj, char *name);	/* scary */
extern int agrelabel_node(Agnode_t * n, char *newname);
extern int agdelete(Agraph_t * g, void *obj);
extern long agdelsubg(Agraph_t * g, Agraph_t * sub);	/* could be agclose */
extern int agdelnode(Agraph_t * g, Agnode_t * arg_n);
extern int agdeledge(Agraph_t * g, Agedge_t * arg_e);
extern int agobjkind(void *);

/* strings */
extern char *agstrdup(Agraph_t *, char *);
extern char *agstrdup_html(Agraph_t *, char *);
extern int aghtmlstr(char *);
extern char *agstrbind(Agraph_t * g, char *);
extern int agstrfree(Agraph_t *, char *);
extern char *agstrcanon(char *, char *);
char *agcanonStr(char *str);  /* manages its own buf */

extern Agsym_t *agattr(Agraph_t * g, int kind, char *name, char *value);
extern Agsym_t *agattrsym(void *obj, char *name);
extern Agsym_t *agnxtattr(Agraph_t * g, int kind, Agsym_t * attr);
extern int      agcopyattr(void *oldobj, void *newobj);

extern void *agbindrec(void *obj, char *name, unsigned int size,
		       int move_to_front);
extern Agrec_t *aggetrec(void *obj, char *name, int move_to_front);
extern int agdelrec(void *obj, char *name);
extern void aginit(Agraph_t * g, int kind, char *rec_name, int rec_size,
		   int move_to_front);
extern void agclean(Agraph_t * g, int kind, char *rec_name);

extern char *agget(void *obj, char *name);
extern char *agxget(void *obj, Agsym_t * sym);
extern int agset(void *obj, char *name, char *value);
extern int agxset(void *obj, Agsym_t * sym, char *value);
extern int agsafeset(void* obj, char* name, char* value, char* def);

/* defintions for subgraphs */
extern Agraph_t *agsubg(Agraph_t * g, char *name, int cflag);	/* constructor */
extern Agraph_t *agidsubg(Agraph_t * g, unsigned long id, int cflag);	/* constructor */
extern Agraph_t *agfstsubg(Agraph_t * g), *agnxtsubg(Agraph_t * subg);
extern Agraph_t *agparent(Agraph_t * g);

/* set cardinality */
extern int agnnodes(Agraph_t * g), agnedges(Agraph_t * g);
extern int agdegree(Agraph_t * g, Agnode_t * n, int in, int out);
extern int agcountuniqedges(Agraph_t * g, Agnode_t * n, int in, int out);

/* memory */
extern void *agalloc(Agraph_t * g, size_t size);
extern void *agrealloc(Agraph_t * g, void *ptr, size_t oldsize,
		       size_t size);
extern void agfree(Agraph_t * g, void *ptr);
extern struct _vmalloc_s *agheap(Agraph_t * g);



%inline %{

Agnode_t *maghead(Agedge_t *e) {
    return aghead(e);
}

Agnode_t *magtail(Agedge_t *e) {
    return agtail(e);        
}

Agraph_t *new_from_file(char *filename) {

    Agraph_t *mygraph;

	if (strcmp(filename, "-") == 0) {
		mygraph = agread(stdin, 0);
		if (mygraph == NULL) {
			fprintf(stderr, "Input graph could not be parsed.\n");
		    exit(EXIT_FAILURE);
        }
	}
	else {
		FILE *fd;

		fd = fopen(filename, "r");
		if (fd == NULL) {
			fprintf(stderr, "Could not open input file\n");
			exit(EXIT_FAILURE);
		}
		else {
			mygraph = agread(fd, 0);
			
			if (mygraph == NULL) {
				fprintf(stderr, "Input graph could not be parsed.\n");
			    exit(EXIT_FAILURE);
            }
		    fclose(fd);
		}
	}
    return mygraph;
}

void write_to_file(Agraph_t *graph, char *filename) {
    
    FILE *fd;

    if (strcmp(filename, "-") == 0) {
        agwrite(graph, stdout);
    }
    else {
        fd = fopen(filename, "w");
        if (fd == NULL) {
            fprintf(stderr, "Couln't open output file\n");
            exit(EXIT_FAILURE);
        }
        else {
            agwrite(graph, fd);
        }
    }
}

Agraph_t *new_directed(char *name) {
    return agopen(name, Agdirected, NULL);
}

Agraph_t *new_undirected(char *name) {
    return agopen(name, Agundirected, NULL);
}

Agnode_t *add_node(Agraph_t *graph, char *name) {
    return agnode(graph, name, 1);
 }

Agedge_t *add_edge(Agraph_t *graph, Agnode_t *from, Agnode_t *to, char *name) {
    return agedge(graph, from, to, name, 1);
}

Agnode_t *get_node(Agraph_t *graph, char *name) {
    return agnode(graph, name, 0);
 }

Agedge_t *get_edge(Agraph_t *graph, Agnode_t *from, Agnode_t *to, char *name) {
    return agedge(graph, from, to, name, 0);
}

Agedge_t *get_edge_without_name(Agraph_t *graph, Agnode_t *from, Agnode_t *to) {
    return agedge(graph, from, to, (char *) NULL, 0);
}

void create_edge_attr(Agraph_t *graph, char *name, char *value) {
    agattr(graph, AGEDGE, name, value);
}

void create_node_attr(Agraph_t *graph, char *name, char *value) {
    agattr(graph, AGNODE, name, value);
}

void create_graph_attr(Agraph_t *graph, char *name, char *value) {
    agattr(graph, AGRAPH, name, value);
}


%}
