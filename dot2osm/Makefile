BINARIES = dot2osm
CXX = g++
CXXFLAGS = -I/usr/include/graphviz
LIBS = -lcgraph -L/usr/lib/graphviz


SRCS = dot2osm.cpp
OBJS = dot2osm.o

default:	all
all:	$(BINARIES)

dot2osm: $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@ $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $< -c -o $@ 

.PHONY: clean all
clean:
	rm -rf $(OBJS) $(BINARIES)

