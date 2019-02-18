#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define VERSION		24
#define BUFSIZE		8096
#define ERROR		42
#define LOG			44
#define PROHIBIDO	403
#define NOENCONTRADO	404
#define EXTENSIONES	10

struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },
	{"jpg", "image/jpg" },
	{"jpeg","image/jpeg"},
	{"png", "image/png" },
	{"ico", "image/ico" },
	{"zip", "image/zip" },
	{"gz",  "image/gz"  },
	{"tar", "image/tar" },
	{"htm", "text/html" },
	{"html","text/html" },
	{0,0} };

void debug(int log_message_type, char *message, char *additional_info, int socket_fd)
{
	int fd ;
	char logbuffer[BUFSIZE*2];
	
	switch (log_message_type) {
		case ERROR: (void)sprintf(logbuffer,"ERROR: %s:%s Errno=%d exiting pid=%d",message, additional_info, errno,getpid());
			break;
		case PROHIBIDO:
			// Enviar como respuesta 403 Forbidden
			(void)sprintf(logbuffer,"FORBIDDEN: %s:%s",message, additional_info);
			break;
		case NOENCONTRADO:
			// Enviar como respuesta 404 Not Found
			(void)sprintf(logbuffer,"NOT FOUND: %s:%s",message, additional_info);
			break;
		case LOG: (void)sprintf(logbuffer," INFO: %s:%s:%d",message, additional_info, socket_fd); break;
	}

	if((fd = open("webserver.log", O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) {
		(void)write(fd,logbuffer,strlen(logbuffer));
		(void)write(fd,"\n",1);
		(void)close(fd);
	}
	if(log_message_type == ERROR || log_message_type == NOENCONTRADO || log_message_type == PROHIBIDO) exit(3);
}




void process_web_request(int descriptorFichero)
{
	debug(LOG,"request","Ha llegado una peticion",descriptorFichero);
	//
	// Definir buffer y variables necesarias para leer las peticiones
	//
	char* buffer=malloc(sizeof(char*)*BUFSIZE);
	char* metodo=malloc(sizeof(char*)*BUFSIZE);
	char* url=malloc(sizeof(char*)*BUFSIZE);
	char* version=malloc(sizeof(char*)*BUFSIZE);
	char* ok_response_http=malloc(sizeof(char*)*BUFSIZE);	;
	char* bad_request =   "HTTP/1.0 501 Method Not Implemented\n"
		"Content-type: text/html\n"
		"\n"
		"<html>\n"
		" <body>\n"
		"  <h1>Method Not Implemented</h1>\n"
		"  <p>The method %s is not implemented by this server.</p>\n"
		" </body>\n"
		"</html>\n";
	//
	// Leer la petición HTTP
	
	
	
	//
	// Comprobación de errores de lectura
	
	//
	if((read(descriptorFichero,buffer,BUFSIZE-1))!=-1){
	//
	// Si la lectura tiene datos válidos terminar el buffer con un \0
			*(buffer+BUFSIZE)='\0';
		
	//
	
	
	//
	// Se eliminan los caracteres de retorno de carro y nueva linea
	//
			sscanf(buffer,"%s %s %s",metodo,url,version);

	
	
	//
	//	TRATAR LOS CASOS DE LOS DIFERENTES METODOS QUE SE USAN
	//	(Se soporta solo GET)
	//
			if((strcmp("GET",metodo))!=0){		
				write(descriptorFichero,bad_request,strlen(bad_request));
				debug(ERROR,"Solo se permite metodo GET",metodo,descriptorFichero);
			}		
	//
	//	Como se trata el caso de acceso ilegal a directorios superiores de la
	//	jerarquia de directorios
	//	del sistema
	//
			
			/*char* directorio_actual=malloc(sizeof(char*)*PATH_MAX);
			if((chdir(url))==-1){
				debug(ERROR, "No se puede acceder a la direccion",url,descriptorFichero);
			}
			getcwd(directorio_actual,PATH_MAX);
			if((strcmp(directorio_actual,"/home/samuel/sockets"))!=0){
				debug(ERROR, "No debe acceder a la direccion",url,descriptorFichero);
			}
			free(directorio_actual);*/
	
	//
	//	Como se trata el caso excepcional de la URL que no apunta a ningún fichero
	//	html
	//
			debug(LOG,url,"Comprobamos la url",descriptorFichero);	
			if((strcmp("/",url))==0){
				strcat(url,"index.html");
				debug(LOG,url,"Nueva URL",descriptorFichero);
			}
	
	//
	//	Evaluar el tipo de fichero que se está solicitando, y actuar en
	//	consecuencia devolviendolo si se soporta u devolviendo el error correspondiente en otro caso
	//
		//	strcat(ok_response_http,"HTTP/1.1 200 OK\r\n ");
			debug(LOG,ok_response_http,"Preparando la respuesta",descriptorFichero);
			
			char* token=strtok(url,".");
			token=strtok(NULL,".");
			for(int i=0;i<EXTENSIONES;i++){
				if((strcmp(extensions[i].ext,token))==0){
					strcat(ok_response_http,"HTTP/1.1 200 OK\r\nContent-type: ");
					strcat(ok_response_http,extensions[i].filetype);
					strcat(ok_response_http,"\r\n\r\n");
				}
				
			}
			debug(LOG,ok_response_http,"Respuesta preparada",descriptorFichero);
	
	//
	//	En caso de que el fichero sea soportado, exista, etc. se envia el fichero con la cabecera
	//	correspondiente, y el envio del fichero se hace en blockes de un máximo de  8kB
	//
		char* fichero_token=strtok(url,"/");
		//fichero_token=strtok(NULL,"/");
		debug(LOG,"Preparando el fichero",fichero_token,descriptorFichero);

		write(descriptorFichero,ok_response_http,BUFSIZE);
	}	
	free(metodo);
	free(buffer);
	free(url);
	free(version);
	free(ok_response_http);
	close(descriptorFichero);
	exit(1);
}

int main(int argc, char **argv)
{
	int i, port, pid, listenfd, socketfd;
	socklen_t length;
	static struct sockaddr_in cli_addr;		// static = Inicializado con ceros
	static struct sockaddr_in serv_addr;	// static = Inicializado con ceros
	
	//  Argumentos que se esperan:
	//
	//	argv[1]
	//	En el primer argumento del programa se espera el puerto en el que el servidor escuchara
	//
	//  argv[2]
	//  En el segundo argumento del programa se espera el directorio en el que se encuentran los ficheros del servidor
	//
	//  Verficiar que los argumentos que se pasan al iniciar el programa son los esperados
	//

	//
	//  Verficiar que el directorio escogido es apto. Que no es un directorio del sistema y que se tienen
	//  permisos para ser usado
	//

	if(chdir(argv[2]) == -1){ 
		(void)printf("ERROR: No se puede cambiar de directorio %s\n",argv[2]);
		exit(4);
	}
	// Hacemos que el proceso sea un demonio sin hijos zombies
	if(fork() != 0)
		return 0; // El proceso padre devuelve un OK al shell

	(void)signal(SIGCHLD, SIG_IGN); // Ignoramos a los hijos
	(void)signal(SIGHUP, SIG_IGN); // Ignoramos cuelgues
	
	debug(LOG,"web server starting...", argv[1] ,getpid());
	
	/* setup the network socket */
	if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0)
		debug(ERROR, "system call","socket",0);
	
	port = atoi(argv[1]);
	
	if(port < 0 || port >60000)
		debug(ERROR,"Puerto invalido, prueba un puerto de 1 a 60000",argv[1],0);
	
	/*Se crea una estructura para la información IP y puerto donde escucha el servidor*/
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); /*Escucha en cualquier IP disponible*/
	serv_addr.sin_port = htons(port); /*... en el puerto port especificado como parámetro*/
	
	if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0)
		debug(ERROR,"system call","bind",0);
	
	if( listen(listenfd,64) <0)
		debug(ERROR,"system call","listen",0);
	
	while(1){
		length = sizeof(cli_addr);
		if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
			debug(ERROR,"system call","accept",0);
		if((pid = fork()) < 0) {
			debug(ERROR,"system call","fork",0);
		}
		else {
			if(pid == 0) { 	// Proceso hijo
				(void)close(listenfd);
				process_web_request(socketfd); // El hijo termina tras llamar a esta función
			} else { 	// Proceso padre
				(void)close(socketfd);
			}
		}
	}
}
