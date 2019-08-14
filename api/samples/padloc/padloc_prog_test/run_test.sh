#! /bin/bash 

DYNAMORIO_DIR="$(pwd)/../../../.."
DYNAMORIO_BUILD_DIR="${DYNAMORIO_DIR}/build"

cd ${DYNAMORIO_DIR}/api/samples/padloc/padloc_prog_test

make 

cd ${DYNAMORIO_BUILD_DIR}

for prog_test in ${DYNAMORIO_DIR}/api/samples/padloc/padloc_prog_test/*.out; do
	echo -e "TEST ${prog_test}\n"
	${DYNAMORIO_BUILD_DIR}/bin64/drrun -c ${DYNAMORIO_BUILD_DIR}/api/bin/libpadloc.so -- ${prog_test}	
	echo -e "\n############################################################################################################\n"
done

cd -
