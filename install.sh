#!/bin/bash

cd simulator-sources
make clean
make
if test $? -ne 0
then
	cd ..
	echo "Installation failed."
	exit 1
fi
cd ..
cp simulator-sources/orcs .

cd cgraph_perl
make clean
make
if test $? -ne 0
then
	cd ..
	echo "Installation failed."
	exit 1
fi

cd ..

cd dot2osm
make clean
make
if test $? -ne 0
then
	cd ..
	echo "Installation failed."
	exit 1
fi
cd ..

satisfied=0
UMAD2SIMDIR=""

while test $satisfied -eq 0
do

dialog --inputbox "
This distribution contains tools that work together with the ibsim tool, available from openfabrics.org. (Or packaged by your linux distribution, i.e. install ibsim-utils package on debian based systems.)

For these tools to work we need the directory where the umad2sim library, libumad2sim.so, is installed, typically /usr/lib/umad2sim.

If you don't have ibsim installed or don't need the tools based on ibsim select cancel.
" 18 72 "/usr/lib/umad2sim" 2> answer

res=$?

UMAD2SIMDIR=`cat answer`
rm answer

if test $res -ne 0
then
	echo "Enjoy ORCS (without ibsim tools)"
	exit 0
fi


if test -e $UMAD2SIMDIR/libumad2sim.so
then
	satisfied=1
else
	dialog --msgbox "$UMAD2SIMDIR/libumad2sim.so does not exist. Please try again." 8 50
fi
done

dialog --inputbox "
To make use of the ibsim tools you will also need OpenSM, available from openfabrics.org. (Or packaged by your linux distribution, i.e. install opensm package on debian based systems.)
 

Do you want me to prepend something to your PATH before I try to execute opensm and related tools? Typically they end up in /usr/sbin, which most people don't have in their path.
" 18 72 "/usr/sbin" 2> answer
PATHPREPEND=`cat answer`
rm answer

dialog --inputbox "
If you installed the opensm tools in a custom directory you probably also want your LD_LIBRARY_PATH adjusted before we use those tools.
 

Do you want me to prepend something to your LD_LIBRARY_PATH before I try to execute opensm and related tools?
" 18 72 2> answer
LIBPATHPREPEND=`cat answer`
rm answer


write_start_ibsim() {
	echo "#!/bin/sh" > start_ibsim.sh
	echo "# This script starts the InfiniBand simulator and uses" >> start_ibsim.sh
	echo "# it to simulate the network you provided as first cmdline" >> start_ibsim.sh
	echo "# argument " >> start_ibsim.sh
	echo "export PATH=$PATHPREPEND:\$PATH" >> start_ibsim.sh
	echo "export LD_LIBRARY_PATH=$LIBPATHPREPEND:\$LD_LIBRARY_PATH" >> start_ibsim.sh
	echo "which ibsim > /dev/null" >> start_ibsim.sh
	echo "if test \$? -ne 0; then echo \"ibsim\" is not in your \\\$PATH; exit 1; fi" >> start_ibsim.sh
	echo "" >> start_ibsim.sh
	echo "if test \$# -lt 1; then" >> start_ibsim.sh
	echo "	echo \"Please provide the an OpenSM topology file as argument\" 1>&2" >> start_ibsim.sh
	echo "	exit -1"  >> start_ibsim.sh
	echo "fi" >> start_ibsim.sh
	echo "export LD_PRELOAD=$UMAD2SIMDIR/libumad2sim.so" >> start_ibsim.sh
	echo "ibsim -s \$1" >> start_ibsim.sh
	chmod u+x start_ibsim.sh
}

write_start_opensm() {
	echo "#!/bin/sh" > start_osm.sh
	echo "# This script starts opensm after preloading the simulator library" >> start_osm.sh
	echo "# before you run this script you have to run ./start_ibsim and provide" >> start_osm.sh
	echo "# a topology file. You have to provide the name of the routing engine" >> start_osm.sh
	echo "# which opensm should use as a command line argument." >> start_osm.sh
	echo "# as soon as opensm prints the SUBNET UP message you may run dump_routes.sh" >> start_osm.sh
	echo "# to archive the routing tables for later analysis." >> start_osm.sh
	echo "export PATH=$PATHPREPEND:\$PATH" >> start_osm.sh
	echo "export LD_LIBRARY_PATH=$LIBPATHPREPEND:\$LD_LIBRARY_PATH" >> start_osm.sh
	echo "which opensm > /dev/null" >> start_osm.sh
	echo "if test \$? -ne 0; then echo \"ibsim\" is not in your \\\$PATH; exit 1; fi" >> start_osm.sh
	echo "" >> start_osm.sh
	echo "if test \$# -lt 1; then" >> start_osm.sh
	echo "  echo \"Please provide the name of a routing engine as argument\" 1>&2" >> start_osm.sh
	echo "  echo \"supported arguments: minhop, updn, file, ftree, lash, dor\" 1>&2" >> start_osm.sh
	echo "	exit 1"  >> start_osm.sh
	echo "fi" >> start_osm.sh
	echo "export LD_PRELOAD=$UMAD2SIMDIR/libumad2sim.so" >> start_osm.sh
	echo "" >> start_osm.sh
	echo "# Opensm tries to use /var for logs and cache files" >> start_osm.sh
	echo "# normaly users can't write to /var directly" >> start_osm.sh
	echo "export OSM_CACHE_DIR=/tmp" >> start_osm.sh
	echo "export OSM_TMP_DIR=/tmp" >> start_osm.sh
	echo "" >> start_osm.sh
	echo "# start opensm, make it log to stdout" >> start_osm.sh
	echo "opensm -R \$1 -f -" >> start_osm.sh
	chmod u+x start_osm.sh
}

write_dump_routes() {
	echo "#!/bin/sh" > dump_routes.sh
	echo "export PATH=$PATHPREPEND:\$PATH" >> dump_routes.sh
	echo "export LD_LIBRARY_PATH=$LIBPATHPREPEND:\$LD_LIBRARY_PATH" >> dump_routes.sh
	echo "export LD_PRELOAD=$UMAD2SIMDIR/libumad2sim.so" >> dump_routes.sh
	echo "" >> dump_routes.sh
	echo "dump_lfts" >> dump_routes.sh
	chmod u+x dump_routes.sh
}


write_start_ibsim
write_start_opensm
write_dump_routes

exit 0

# TODO check if everything is fine
#else
#	echo "Something went wrong during the install, check the output." 1>&2
#	exit -1
#fi


