#!/bin/bash

#confirm number of arguments
if [ ${#} != 3 ]
  then
    echo "Error: number of arguments (${#}) incorrect. Should be 3."
    exit 
fi

#confirm 1st argument is the correct directory
if [ "${1}" != "./inputs/" ]
  then
  	echo "Error: ${1} is not the correct directory (./input/)"
  	exit
fi

#confirm 2nd argument is the correct directory
if [ "${2}" != "./output/" ]
  then
  	echo "Error: ${2} is not the correct directory (./output/)"
  	exit
fi

#confirm 3rd argument is an integer
if [[ ! "${3}" =~ ^-?[0-9]+$ ]]
  then
  	echo "Error: ${3} is not an integer"
  	exit
fi 
  
#confirm 3rd argument is > 0	
if [ ! ${3} -gt 0 ]
  then
  	echo "Error: ${3} is less than 0"
  	exit
fi

#to keep nr of current test
j=0

#run tests (numInputs)*(maxThreads) times
for input in ${1}*.txt
do
j=$((j+1))
	for i in $(seq ${3})
	do
		echo "InputFile=${input} NumThreads=${i}" 
		./tecnicofs ${input} ${2}test$j-$i.txt $i > ./out.txt
		tail -n 1 ./out.txt
		rm ./out.txt 
	done
done
