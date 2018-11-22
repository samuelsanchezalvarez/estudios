#!/bin/bash -u
if [ $# -le 0 ]
then
	echo "Uso: monton.sh argumento" >&2
	exit 1
fi
if echo $1 | grep -x -v -q "[0-9]\+"
then
	echo "El argumento no es un numero" >&2
	exit 1
fi
if [ $1 -le 0 ]
then
	echo "El argumento tiene que ser un numero positivo" >&2
	exit 1
fi
num_filas=$1
let i=1
let num_espacio=$num_filas-1
while [ $i -le $num_filas ]
do
	let num_ast=$i*2-1
	let j=1
	if [ $num_espacio -ne 0 ]
	then
		let k=0
		while [ $k -lt $num_espacio ]
		do
			echo -n " "
			let k+=1
	  	done
	fi
	while [ $j -le $num_ast ]
	do
	 	echo -n "*"
		let j+=1
	done
	echo " "
	let i+=1
	let num_espacio-=1
done
