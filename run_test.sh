#! /bin/bash 

DYNAMORIO_DIR="/home/micky/Workspace/dynamorio"
DYNAMORIO_BUILD_DIR="${DYNAMORIO_DIR}/build"

cd ${DYNAMORIO_DIR}/dynamorio_prog_test 

make 

cd ${DYNAMORIO_BUILD_DIR}

for prog_test in ${DYNAMORIO_DIR}/dynamorio_prog_test/*.out; do
	echo -e "TEST ${prog_test}\n"
	${DYNAMORIO_BUILD_DIR}/bin64/drrun -c ${DYNAMORIO_BUILD_DIR}/api/bin/libinterflop.so -- ${prog_test}	
	echo -e "\n############################################################################################################\n"
done

cd -
