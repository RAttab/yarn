#! /bin/bash

#
# Executes the tests until it fails. Useful to quickly reproduce sporadic issues.
#

c=0
while [ $? -eq 0 ]; do
    c=$(($c+1))
    echo -ne "Testing attempt: $c\r"
    ./test/check_libyarn 1 > out.log
done
