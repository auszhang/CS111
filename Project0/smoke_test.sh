#!/bin/bash

# NAME: Austin Zhang
# EMAIL: aus.zhang@gmail.com   
# ID: 604736503

# run lab0 with test0 as the input and output0 as the output
`./lab0 --input="test0" --output="output0"`

# check if the program runs to completion
if [ $? -eq 0 ]
then 
    echo "Success: The program ran to completion."
else
    echo "Failed smoke-test: The program did not run to completion."
fi

# check if argument is valid
`./lab0 --invalid 2> /dev/null`
if [ $? -eq 1 ]
then
    echo "Success: Invalid option, as expected."
else
    echo "Failed smoke-test: All arguments are valid when they should not be."
fi

# check if input is valid
`./lab0 --input=i_dont_exist.txt 2> /dev/null`
if [ $? -eq 2 ]
then 
    echo "Success: Invalid input, as expected."
else
    echo "Failed smoke-test: The input was valid when it should not be."
fi

# compare input and output files
`cmp test0 output0`
if [ $? -eq 0 ]
then 
    echo "Success: Standard input was copied to standard output."
else
    echo "Failed smoke-test: Input and output files do not match."
fi

# check segfault creation
`./lab0 --segfault --catch 2> /dev/null`
if [ $? -eq 4 ]
then 
    echo "Success: Segmentation fault created and successfully caught."
else
    echo "Failed smoke-test: Failed to create or catch segmentation fault."
fi

# removes the extra files after smoke-testing has finished
`rm -f output0`
