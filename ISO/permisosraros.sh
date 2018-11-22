#!/bin/bash
while [ ! -z $1 ]
do
    usuarios=$(ls -l | tail -n +2 | tr -s " " | cut -d " " -f 3 | uniq	| sort)
	for us in $usuarios
	do
	    echo "$us:"
		find $1 -type f -exec ls -l {} \; | grep $us | (while read linea; do
		perms=$(echo $linea | cut -d " " -f 1)
		nom=$(echo $linea | cut -d " " -f 9 | cut -d "/" -f 2)
		echo -e "\t $nom $perms"
		done;
		)
    done
   shift 
done   