IF "%1%"=="web_sstt" (
	IF "%2%"=="linux" (
		pscp C:\Users\pc\Documents\Estudios\SSTT\web_sstt.c samuel@192.168.63.4:/home/samuel/sockets
	) ELSE IF "%2%"=="windows" (
		pscp samuel@192.168.63.4:/home/samuel/sockets/web_sstt.c C:\Users\pc\Documents\Estudios\SSTT\web_sstt.c
	)
) ELSE IF "%1%"=="wireshark" (
	pscp samuel@192.168.63.4:/home/samuel/fichero C:\Users\pc\Documents\Estudios\SSTT
)
