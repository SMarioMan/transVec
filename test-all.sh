# !/bin/bash
#
# Soliman Alnaizy - May 2019
# Insired from Dr. Szumlanski's program testing scripts.
# Test Script for AREA 67's transactional vector data structure.
#
# This script is made with the intention of providing a convinient way to run
# multiple testcases and generate a cute report at the end. If you want to run
# any individual testcase, just change the MAIN variable in the makefile and
# simply run "make" from a commandline.
#
# ADDING YOUR OWN TESTCASES: Just create a file named testcaseX.cpp and place
# it in the test_cases/ directory. Update the variable below to the new number
# of testcases
NUM_TEST_CASES=19

# Insert the data structures that you want to test in this array.
DATA_STRUCTURES=(SEGMENTVEC COMPACTVEC BOOSTEDVEC STMVEC STOVEC)

# Redirect all local error messages to /dev/null (ie "process aborted" errors).
exec 2> /dev/null

###############################################################################
# Gather some system data before starting anything
###############################################################################
DATE=$(date)
REPORT="reports/final_report$(date +%m%d%H%M%S).txt"
NUM_CORES=$(nproc)
MODEL="Processor $(lscpu | grep GHz)"
MODEL=$(echo "$MODEL" | sed -r 's/\s\s\s\s\s\s\s\s\s//g')
RAM="$(free -h --si | grep -o -m 1 "\w*G\b" | head -1)B"

# Create the file
# TIP: The final report will look a lot cleaner if your tab width is set to 8 :D
touch $REPORT

# Make sure there aren't any pre-existing object files
make clean

# Output a bunch of housekeeping information
# echo "====================================================="             >> $REPORT
# echo "  T R A N S A C T I O N A L   V E C T O R   T E S T  "             >> $REPORT
# echo                                                                     >> $REPORT
# echo "Test preformed on $DATE."                                          >> $REPORT
# echo "Tested on a computer with $NUM_CORES cores."                       >> $REPORT
# echo "$MODEL"                                                            >> $REPORT
# echo "Total RAM on system: $RAM"                                         >> $REPORT
# echo "====================================================="             >> $REPORT
# echo                                                                     >> $REPORT
echo -n -e "DS\tTC\tSGMT\tNUM_TXN\t"     >> $REPORT
echo -n -e "TXN_SIZ\tTHRD_CT\tTIME\tABRTS"                   >> $REPORT
echo -e "\tPREP\tSHARED\tTOTAL"                   >> $REPORT

# More grepping to get data that will be useful to print out
NUM_TXN=$(grep "NUM_TRANSACTIONS" define.hpp | grep "[0-9]*" -o)
SGMT_SIZE=$(grep "SGMT_SIZE" define.hpp | grep "(.*)" -o)

# Make sure that TRANSACTION_SIZE, THREAD_COUNT, and all the vector defines in define.hpp
# are commented out. If the output is not 0, display message and then exit the test scipt.
# Bad things will happen if they're not commented out.
g++ test_cases/sanity_check.cpp
./a.out
execution_val=$?
if [[ $execution_val == 1 ]]; then
    echo -e -n "\e[91mError!\e[0m Please make sure that TRANSACTION_SIZE and THREAD_COUNT"
    echo " in define.hpp are commented out."
    rm $REPORT
    exit
elif [[ $execution_val == 2 ]]; then
    echo -e -n "\e[91mError!\e[0m Please make sure that all vector implementations"
    echo " in define.hpp are commented out."
    rm $REPORT
    exit
fi
rm a.out

# Run every test 5 times to generate an average result.
for l in `seq 1 5`
do
    for i in `seq 1 $NUM_CORES`
    do
        # Test for TRANSACTION_SIZE from 1 - 5
        for j in 5
        do
            # Each of the data structures will go through all these testcases
            for ds in "${DATA_STRUCTURES[@]}"
            do
                # echo "SGMT_SIZE        = $SGMT_SIZE"                         >> $REPORT
                # echo "NUM_TRANSACTIONS = $NUM_TXN  "                         >> $REPORT
                # echo "THREAD_COUNT     = $i        "                         >> $REPORT
                # echo "TRANSACTION_SIZE = $j        "                         >> $REPORT
                # echo                                                         >> $REPORT

                # Parsing the values that will be changed during compile time
                TO_BE_PASSED="|THREAD_COUNT=$i|TRANSACTION_SIZE=$j"

                # Loop through the testcases
                for k in "04" "05" "07"
                do
                    # Tab separated information (Display only the first 5 chars of DS)
                    echo -n -e "${ds:0:5}\t$k\t"                                   >> $REPORT

                    echo -e -n "\e[93mStarting testcase$k: "
                    echo -e -n "\e[96mDS = \e[35m$ds\e[96m, THRD_CNT = \e[35m$i\e[96m,"
                    echo -e -n " TXN_SIZE = \e[35m$j\e[96m ... \e[0m"
                    # echo -n "Testcase $k ... "                               >> $REPORT

                    # Make the executable file with a different main file everytime
                    make -j$NUM_CORES DATA_STRUCTURE=$ds MAIN=test_cases/testcase$k.cpp DEFINES=$TO_BE_PASSED

                    # Check if it compiled correctly or not. If not, output a bunch of -1's
                    compile_val=$?
                    if [[ $compile_val != 0 ]]; then
                        echo -e "\e[91m fail (failed to compile)\e[0m"                
                        # echo "fail (failed to compile)"                      >> $REPORT
                        echo -e "-1\t-1\t-1\t-1\t-1"                         >> $REPORT
                        continue
                    fi

                    # Run the executable and suppress all ouptut
                    ./transVec.out                                           >> $REPORT

                    # Check if the program crashed or not. If it did, output a bunch of -2's               
                    execution_val=$?
                    if [[ $execution_val != 0 ]]; then
                        echo -e "\e[91m fail (program crashed)\e[0m"                
                        # echo "fail (program crashed)"                        >> $REPORT
                        echo -e "-2\t-2\t-2\t-2\t-2"                         >> $REPORT
                        continue
                    fi

                    echo -e "\e[92m Success!\e[0m"
                done

                # Clean all object files to prepare for the next round of testing
                make clean
                echo "====================================================="
            done
        done
    done
done

# Clean up after yo self
echo 
echo "====================================================="
echo "   End of test script. Cleaning up object files...   "
echo "====================================================="
# echo "====================================================="         >> $REPORT
# echo "         E N D   O F   T E S T   S C R I P T         "         >> $REPORT
# echo "====================================================="         >> $REPORT
make clean
echo
