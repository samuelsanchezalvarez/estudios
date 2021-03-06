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
numero_cifras=$(($1-1))
i=0
numero_nueves=0
while [ $i -lt $numero_cifras ]
do
	numero_nueves=$(($numero_nueves*10+9))
	i=$(($i+1))
done
numero_rango=$((9*10**$numero_cifras))
numero_aleatorio=0
suma=0
while [ $suma -ne $2 ]
do
	suma=0
	numero_aleatorio=$((($RANDOM % $numero_rango)+$numero_nueves))
	j=1
	while [ $j -le $1 ]
	do
		numero=$(echo $numero_aleatorio | cut -c $j)
		echo "Cifra $j: $numero"
		suma=$(($suma+$numero))
		j=$(($j+1))
	done
done
echo "$numero_aleatorio"
