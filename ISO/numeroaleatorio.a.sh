#!/bin/bash -u
if [  $# -ne 2 ]
then
	echo "Error: numero de argumentos incorrectos\nUso: numeroaleatorio.sh cifras suma" >&2
	exit 1
fi
if echo $@ | tr " " "\n" | grep -vqw "[[:digit:]]\+"
then
	echo "Error: alguno de los argumenos (o ambos) no es un numero natural\nUso: numeroaleatorio.sh cifras suma " >&2
	exit 2
fi
if [ $1 -gt 10 ] || [ $1 -lt 2 ]
then
	echo "Error: el primer argumento debe ser un numero natural entre 2 y 10\nUso:numeroaleatorio.sh cifras suma">&2
	exit 3
fi
