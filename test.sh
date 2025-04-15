#!/bin/bash

max=4

get()
{
for i in `seq 1 $max`
do
echo 'get 1mb'$i'.zip'
done
}

list()
{
for i in `seq 1 $max`
do
echo 'list'
done
}

put()
{
for i in `seq 1 $max`
do
echo 'put scaletta'$i'.txt'
done
}

get|./client.o

exit