#!/usr/bin/make

module:
	swig -perl5 cgraph.i
	gcc -fno-stack-protector -fpic -c cgraph_wrap.c -Dbool=char `perl -MExtUtils::Embed -e ccopts` -I /usr/include/graphviz
	ld -G cgraph_wrap.o -o cgraph.so -lcgraph

clean:
	rm -rf *.o *.so *.pm *.c
