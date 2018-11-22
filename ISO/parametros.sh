#!/bin/bash -u
echo "El numero de parametros es $#"
menor=999999
mayor=0
numeros_sin_signo=0
valor_retorno=0
numeros=$(mktemp)
while [ $# -gt 0 ]
do
	if echo $1 | grep  -q "[-+][[:digit:]]\+"
	then
		valor_retorno=1
	elif echo $1 | grep -qvw "[[:digit:]]\+"
	then
		valor_retorno=1
	else
		numeros_sin_signo=$(($numeros_sin_signo+1))
		echo $1 >> "$numeros"
		if [ $1 -le "$menor" ]
		then
			menor=$1
		elif [ $1 -ge "$mayor" ]
		then
			mayor=$1
		fi
	fi
	shift
done
echo "El numero de enteros sin signos recibidos es $numeros_sin_signo"
valorepetido=$(cat $numeros | sort -r -n  | uniq -c | tr -s " " | sort -r -n -k 1 |head -1 | cut -f2 -d' '  )
cantidadrepetido=$(cat $numeros | sort -r -n | uniq -c | tr -s " " | sort -r -n -k 1 | head -1 | cut -f3 -d' ' )
echo "El numero mayor es $mayor y el numero menor es $menor"
echo "El numero mas repetido es $cantidadrepetido que aparece un total de $valorepetido veces"
rm "$numeros"
exit $valor_retorno
