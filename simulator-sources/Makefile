CXXFLAGS = -I/usr/include/graphviz/ -Wno-deprecated
CCFLAGS = -I/usr/include/graphviz/
LIBS = -lm -lgsl -lgslcblas -lcgraph -L/usr/lib/graphviz
MPICXX = mpicxx -g
CC = mpicc -g
OBJECTS = pattern_generator.o simulator.o statistics.o cmdline.o cmdline_sanity.o

all: orcs

orcs: driver.o $(OBJECTS) cmdline.h
	$(MPICXX) $(CXXFLAGS) driver.o $(OBJECTS) -o $@ $(LIBS)

cmdline.h: options_def.ggo
	gengetopt < options_def.ggo

cmdline.o: cmdline.h
	$(CC) $(CCFLAGS) cmdline.c -c -o cmdline.o

cmdline_sanity.o: cmdline_sanity.cpp
	$(CC) $(CCFLAGS) cmdline_sanity.cpp -c -o cmdline_sanity.o

%.o: %.cpp cmdline.h
	$(MPICXX) $(CXXFLAGS) $< -c -o $@ 

clean: 
	rm -f $(OBJECTS) driver.o ibroutesim cmdline.h cmdline.c
