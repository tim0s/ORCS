#!/bin/bash

which dialog 2> /dev/null
if test $? -ne 0
then
	echo ""
	echo "The 'dialog' command is needed for this installation script, but it was not found in your system."
	echo ""
	echo "Please install the 'dialog' command and try again."
	echo ""
	exit 1
fi

which gengetopt 2> /dev/null
if test $? -ne 0
then
	echo ""
	echo "The 'gengetopt' command is needed for this installation script, but it was not found in your system."
	echo ""
	echo "Please install the 'gengetopt' command and try again."
	echo ""
	exit 1
fi

if test ! -d /usr/include/graphviz/
then
	echo ""
	echo "Graphviz header files were not found in /usr/include/graphviz/."
	echo ""
	echo "Please install graphviz development files."
	echo ""
	exit 1
fi

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

# Use a "correct" default UMAD2SIMDIR path if possible.
UMAD2SIMDIR="$(whereis umad2sim | head -1 | awk '{print $2}')"
UMAD2SIMDIR=${UMAD2SIMDIR:-"/usr/lib/umad2sim"}

dialog --inputbox "
This distribution contains tools that work together with the ibsim tool, available from openfabrics.org. (Or packaged by your linux distribution, i.e. install ibsim-utils package on debian based systems.)

For these tools to work we need the directory where the umad2sim library, libumad2sim.so, is installed, typically /usr/lib/umad2sim.

If you don't have ibsim installed or don't need the tools based on ibsim select cancel.
" 18 72 "$UMAD2SIMDIR" 2> answer

res=$?

UMAD2SIMDIR=$(cat answer)
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

OPENSM_BIN="$(which opensm)"

if test ! -f "$OPENSM_BIN"
then
	dialog --inputbox "
opensm binary was not found in your PATH:"$PATH"

To make use of the ibsim tools you will also need OpenSM, available from openfabrics.org or packaged by your linux distribution (e.g. you can install opensm package on debian-based or redhat-based systems.)

Please install opensm, and ensure that you can execute the opensm binary system-wide, i.e. the binary can be found in your PATH.

If you need to add a directory in your PATH, please add the directory in the input box now:
" 22 78 2> answer
	PATHPREPEND="$(cat answer)"
	rm answer

	dialog --inputbox "
If you installed the opensm tools in a custom directory you probably also want your LD_LIBRARY_PATH adjusted before we use those tools.

Do you want me to prepend something to your LD_LIBRARY_PATH before I try to execute opensm and related tools?
" 18 72 2> answer
	LIBPATHPREPEND=$(cat answer)
	rm answer
else
	dialog --yesno "
OpenSM binary was found in:
'"$OPENSM_BIN"'

Do you want to use this opensm binary?

If you do not have a custom opensm installation that you would like to use in a different folder, select 'yes'. Otherwise select 'no' and you will be asked to provide the PATH of you opensm installation in the next dialog box.
" 20 76

	res=$?

	# If no was selected....
	if test $res -eq 1
	then
		dialog --inputbox "
Choose the custom directory where opensm binary can be found:
" 22 78 2> answer
		PATHPREPEND="$(cat answer)"
		rm answer

		dialog --inputbox "
If you installed the opensm tools in a custom directory you probably also want your LD_LIBRARY_PATH adjusted before we use those tools.

Do you want me to prepend something to your LD_LIBRARY_PATH before I try to execute opensm and related tools?
" 18 72 2> answer
		LIBPATHPREPEND=$(cat answer)
		rm answer
    fi
fi

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
        echo "ibsim -s -N 49152 -S 49152 -P 200000 \"\$1\"" >> start_ibsim.sh
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
	echo "if test \$? -ne 0; then echo \"opensm\" is not in your \\\$PATH; exit 1; fi" >> start_osm.sh
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
	echo "opensm -R \"\$1\" -f -" >> start_osm.sh
	chmod u+x start_osm.sh
}

write_dump_routes() {
	echo "#!/bin/sh" > dump_routes.sh
	echo "export PATH=$PATHPREPEND:\$PATH" >> dump_routes.sh
	echo "export LD_LIBRARY_PATH=$LIBPATHPREPEND:\$LD_LIBRARY_PATH" >> dump_routes.sh
	echo "export LD_PRELOAD=$UMAD2SIMDIR/libumad2sim.so" >> dump_routes.sh
	echo "" >> dump_routes.sh
	echo "dump_fts" >> dump_routes.sh
	chmod u+x dump_routes.sh
}


write_start_ibsim
write_start_opensm
write_dump_routes

dialog --msgbox "
You can now use the following three generated scripts to run ORCS simulator:

start_ibsim.sh
start_osm.sh
dump_routes.sh
" 18 72

exit 0

# TODO check if everything is fine
#else
#	echo "Something went wrong during the install, check the output." 1>&2
#	exit -1
#fi


