#!/bin/bash -u
nombrelar=""
tam=0
for f in $( ls -l | tail -n +2 | tr -s " " | cut -d " " -f 9)
do
   a= $( echo $f | wc -m )
   if [ $a -gt $tam ]
   then
       nombrelar=$f
       tam=$(echo $f | wc -m)    
    fi
done
echo $nombrelar

