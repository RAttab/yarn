#! /bin/bash

#
# Executes the tests until it fails. Useful to quickly reproduce sporadic issues.
#

while [ $? -eq 0 ]; do
    echo Trying again...
    ./check_libyarn > err.log
done