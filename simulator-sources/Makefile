CXXFLAGS = -I/usr/include/graphviz/ -Wno-deprecated
CCFLAGS = -I/usr/include/graphviz/
LIBS = -lm -lgsl -lgslcblas -lcgraph -L/usr/lib/graphviz
MPICXX = mpicxx -g
CC = mpicc -g
OBJECTS = pattern_generator.o simulator.o statistics.o cmdline.o

all: orcs

orcs: driver.o $(OBJECTS) cmdline.h
	$(MPICXX) $(CXXFLAGS) driver.o $(OBJECTS) -o $@ $(LIBS)

cmdline.h: options_def.ggo
	gengetopt < options_def.ggo

cmdline.o: cmdline.h
	$(CC) $(CCFLAGS) cmdline.c -c -o cmdline.o

%.o: %.cpp cmdline.h
	$(MPICXX) $(CXXFLAGS) $< -c -o $@ 

clean: 
	rm -f $(OBJECTS) driver.o ibroutesim cmdline.h cmdline.c
