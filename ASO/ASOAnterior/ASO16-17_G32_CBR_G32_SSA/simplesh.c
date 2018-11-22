// Shell `simplesh`
// Grupo 3.2
// Miembros del grupo:
// Candida Barba Ruiz
// Samuel Sanchez Alvarez
#define _XOPEN_SOURCE 500
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <pwd.h>
#include <libgen.h>
#include <ctype.h>
#include <getopt.h>
#include <signal.h>
#include <ftw.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

// Libreadline
#include <readline/readline.h>
#include <readline/history.h>

// Tipos presentes en la estructura `cmd`, campo `type`.
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 15
#define TAM_BUFF 256
#define TIMEOUT 5
// Estructuras
// -----

// La estructura `cmd` se utiliza para almacenar la información que
// servirá al shell para guardar la información necesaria para
// ejecutar las diferentes tipos de órdenes (tuberías, redirecciones,
// etc.)
//
// El formato es el siguiente:
//
//     |----------+--------------+--------------|
//     | (1 byte) | ...          | ...          |
//     |----------+--------------+--------------|
//     | type     | otros campos | otros campos |
//     |----------+--------------+--------------|
//
// Nótese cómo las estructuras `cmd` comparten el primer campo `type`
// para identificar el tipo de la estructura, y luego se obtendrá un
// tipo derivado a través de *casting* forzado de tipo. Se obtiene así
// un polimorfismo básico en C.
struct cmd {
    int type;
};

// Ejecución de un comando con sus parámetros
struct execcmd {
    int type;
    char * argv[MAXARGS];
    char * eargv[MAXARGS];
};

// Ejecución de un comando de redirección
struct redircmd {
    int type;
    struct cmd *cmd;
    char *file;
    char *efile;
    int mode;
    int fd;
};

// Ejecución de un comando de tubería
struct pipecmd {
    int type;
    struct cmd *left;
    struct cmd *right;
};

// Lista de órdenes
struct listcmd {
    int type;
    struct cmd *left;
    struct cmd *right;
};

// Tarea en segundo plano (background) con `&`.
struct backcmd {
    int type;
    struct cmd *cmd;
};
//Comando interno pwd
int run_pwd(){
    char buf[TAM_BUFF];
    getcwd(buf,TAM_BUFF);
	//Control de error
    if(buf==NULL){
		perror("pwd");
		return EXIT_FAILURE;
    }
	//Imprimimos por la salida estándar y de errores
    fprintf(stderr, "simplesh: pwd:");
    printf("%s\n",buf);	
    return 0;	
}
//Comando interno exit
void run_exit(){
   exit(EXIT_SUCCESS);
    
}
//Comando interno cd
int run_cd(char * ruta){
   if(ruta==NULL) ruta = getenv("HOME");
   return chdir(ruta);

}
//Comando interno tee
int run_tee(char *arg[],int argc){
    int aflag = 0;
    int hflag = 0;
    int c;
    extern int optind; 
    char buffer[TAM_BUFF];
   
    //Comprobamos los argumentos y activamos el flag correspondiente si procede

    while ((c = getopt(argc,arg,"ah"))!=-1)
	switch(c){
	case 'a' :
		aflag = 1;
		break;
	case 'h':
		hflag =1;
		break;	
	default:
	   break;
	}
 
   //Comprobamos el flag h. Si está activado se muestra la ayuda y se termina la ejecución del comando
   if(hflag){
	printf("Uso: tee [-h] [-a] [FICHERO] : \n\tCopia stdin a cada FICHERO y a stdout.\n\tOpciones:\n\t-a Añade al final de cada FICHERO\n\t-h help\n");
	return 0;

   }
   //Leemos de la entrada estándar y escribimos en la salida estándar
   int nr;  
   int leidos = 0;
   int numficheros=0;
   while ((nr = read(STDIN_FILENO, buffer,TAM_BUFF)) > 0){
	write(STDOUT_FILENO,buffer,nr);
	//Escribimos en los ficheros
	leidos +=nr;
	while(optind < argc){
		int fd;
		//Comprobamos si el flag a está activado para sobreescribir o añadir
		if(aflag)
			fd = open(arg[optind], O_RDWR|O_CREAT | O_APPEND,S_IRWXU);

		else
			fd = open(arg[optind], O_RDWR|O_CREAT,S_IRWXU);
		if(fd!=-1){
			write(fd,buffer,nr);
			fsync(fd);
			close(fd); 
		}
		optind++;
		numficheros++;

	}
   }
   //Parte opcional: log
   //Creamos e inicializamos las variables del proceso tee que completarán cada línea del log
   int pid;
   char buf[256];
   char numpid[16];
   char numeuid[16];
   char numfile[16];
   char numbytes[16];
   struct tm* ti;
   int euid;
   euid = geteuid();
   size_t s;
   pid = getpid();
   time_t hora = time(NULL);
   ti = localtime(&hora);
   if(ti==NULL){
	perror("localtime");
	return EXIT_FAILURE;
	}
	
   s = strftime(buf,256,"%F %T",ti);
   snprintf(numpid,16,"%d",pid);
   snprintf(numeuid, 16, "%d", euid);
   snprintf(numfile,16,"%d",numficheros);
   snprintf(numbytes,16,"%d",leidos);
   strcat(buf," PID: ");
   strcat(buf,numpid);
   strcat(buf," EUID: ");
   strcat(buf,numeuid);
   strcat(buf," ");
   strcat(buf,numbytes);
   strcat(buf, " leidos ");
   strcat(buf, numfile);
   strcat(buf, " ficheros\n");
   
   //Indicamos la ruta del fichero
   char * ruta =getenv("HOME");
   strcat(ruta,"/.tee.log");
   
   //Con las distintas máscaras de bits controlamos que se cree el fichero si no existe, que se añada la línea y no se sobreescriba,
   // y que el fichero tenga los permisos deseados.
   int log;
   log = open(ruta,O_RDWR|O_CREAT | O_APPEND,S_IRWXU);
   if(log==-1){
	perror("open");
	return EXIT_FAILURE;
	}
   write(log, buf,64);
   close(log);
   
   return 0;

}

static int suma = 0;
static long tvalor = 0;
static int tbandera = 0;
static int vflag = 0;
static int bflag = 0;
//Función que aplica nftw para la ejecución del comando du
int recorrer_directorio(const char* fpath, const struct stat *sb,int tflag, 
	struct FTW *ftwbuf){
	//Comprobamos que nos han pasado un directorio y que el flag v está activado
	if(tflag == FTW_D && vflag)
		printf("%s\n",fpath);
		
	//Comprobamos que nos han pasado un fichero
	if(tflag == FTW_F){
		//Si el flag t está activado
		if(tbandera){
			//Si el valor pasado junto al parámetro -t es negativo
			if(tvalor<0){
				tvalor -= tvalor * 2;
				//Seleccionamos aquellos ficheros cuyo tamaño es menor que tvalor
				if(((long long) sb->st_size) < tvalor){
					//Si el flag b está activado
					if(bflag){
						//Si el flag v está activado
						if(vflag)
							printf("\t%s: %lld\n ",fpath,(long long) sb->st_blocks * 8);
						suma+=(long long) sb->st_blocks * 8;
					}
					//Si el flag b NO está activado					
					else{
						//Si el flag v está activado
						if(vflag)
							printf("\t%s: %lld\n ",fpath,(long long) sb->st_size);
						suma+=(long long) sb->st_size;
					}
				}
			}
			//Si el valor pasado junto al parámetro -t es positivo
				else
				//Seleccionamos aquellos ficheros cuyo tamaño es mayor que tvalor
			   	if(((long long) sb->st_size) > tvalor){
					//Si el flag b está activado
					if(bflag){
						//Si el flag v está activado
						if(vflag)
							printf("\t%s: %lld\n  ",fpath,(long long) sb->st_blocks * 8);
						suma+=(long long) sb->st_blocks * 8;
					}
					//Si el flag b NO está activado	
					else{
						//Si el flag v está activado
						if(vflag)
							printf("\t%s: %lld\n ",fpath,(long long) sb->st_size);
						suma+=(long long) sb->st_size;
					}
				}		
		}
		//Si el flag t NO está activado		
		else{
			//Si el flag b está activado
		     if(bflag){
			 //Si el flag v está activado
			if(vflag)
			  printf("\t%s: %lld\n ",fpath,(long long) sb->st_blocks * 8);
			suma+=(long long) sb->st_blocks * 8;
			}
			//Si el flag b NO está activado	
		     else{
			 //Si el flag v está activado
			if(vflag)
				printf("\t%s: %lld\n ",fpath,(long long) sb->st_blocks * 8);
			suma+=(long long) sb->st_size;
			}
			}
	}
	return 0;
	

}
//Comando interno du
int run_du(char *argv[], int argc){
    int hflag,c;
    hflag = 0;
    extern int optind; 
    char buffer[TAM_BUFF];
	
    //Comprobamos los argumentos y activamos el flag correspondiente si procede
    while ((c = getopt(argc,argv,"hvbt:"))!=-1)
	switch(c){
	case 'h':
		hflag = 1;
		break;
	case 'b' :
		bflag = 1;
		break;
	case 't':
		tbandera =1;
		tvalor = atol(optarg);
		break;	
	case 'v':
		vflag = 1;
		break;
	default:
	   break;
	}
	//Si el flag h está activado mostramos el mensaje de ayuda
    if(hflag){
	printf("Uso: du [-h] [-b] [-t SIZE] [FICHERO | DIRECTORIO]...\n"
		"Para cada fichero, imprime su tamano.\n"
		"Para cada directorio, imprime la suma de los tamaños de todos los ficheros de todos sus subdirectorios.\n"
		"\tOpciones:\n"
		"\t-b Imprime el tamaño ocupado en disco por todos los bloques del fichero.\n"
		"\t-t SIZE Excluye todos los ficheros mas pequeños que SIZE bytes, si es positivo, "
		"o más grandes que SIZE bytes, si es negativo, cuando se procesa un directorio.\n"
		"\t-h help.\n"
		"Nota: Todos los tamaños están expresados en bytes.\n");
	return 0;
    }
    	do{
		char * path;
    	//Si no nos han pasado directorio, el path es el directorio actual
		if(argc==1 || (argc==2 && (bflag || vflag)) || (argc==3 && tbandera) || (argc==4 && tbandera && bflag && vflag)) 
			path= ".";
		//En caso contrario recuperamos el argumento pertinente y lo guardamos como path
		else
			path= argv[optind];
			struct stat estructura;
    		int status;
    		status = stat(path,&estructura);
    		if(status==-1){
				perror("stat");
				return EXIT_FAILURE;
			}
		//Comprobamos si la estructura es un fichero regular
   	 	if(S_ISREG(estructura.st_mode)){
			//Si b está activado damos el tamaño el bloques
			if(bflag)
				printf("(F) %s : %lld\n",path,(long long)estructura.st_blocks);
			//Si b No está activado damos el tamaño en bytes
			else
				printf("(F) %s : %lld\n",path,(long long)estructura.st_size);
		}
		
		//Comprobamos si la estructura es un directorio y recorremos éste con nftw pasándole la función recorrer_directorio como parámetro
    	 	if(S_ISDIR(estructura.st_mode)){
				nftw(path, recorrer_directorio,20, 0);
				printf("(D) %s : %d\n",path,suma);
				suma = 0;
		}
	optind++;
	//Mientras que "queden argumentos"
    } while(optind < argc);
    	tbandera = 0;
	return 0;
	
}
// Declaración de funciones necesarias
int fork1(void);  // Fork but panics on failure.
void panic(char*);
struct cmd *parse_cmd(char*);

// Ejecuta un `cmd`. Nunca retorna, ya que siempre se ejecuta en un
// hijo lanzado con `fork()`.
void
run_cmd(struct cmd *cmd)
{
    int p[2];
    struct backcmd *bcmd;
    struct execcmd *ecmd;
    struct listcmd *lcmd;
    struct pipecmd *pcmd;
    struct redircmd *rcmd;

    if(cmd == 0)
        exit(0);

    switch(cmd->type)
    {
    default:
        panic("run_cmd");

        // Ejecución de una única orden.
    case EXEC:
        ecmd = (struct execcmd*)cmd;
        if (ecmd->argv[0] == 0)
            exit(0);
        if((strcmp(ecmd->argv[0],"pwd"))==0)
		run_pwd();
	else if((strcmp(ecmd->argv[0],"tee"))==0){
		int i = 0;
		while((ecmd->argv[i])!=NULL){
			i++;
		}
		run_tee(ecmd->argv,i);
	
	}

	else if((strcmp(ecmd->argv[0],"du"))==0){
		int i = 0;
		while((ecmd->argv[i])!=NULL){
			i++;
		}
		run_du(ecmd->argv,i);
	}
	else{
        	execvp(ecmd->argv[0], ecmd->argv);
		fprintf(stderr, "exec %s failed\n", ecmd->argv[0]);
        	exit (1);
	}
        // Si se llega aquí algo falló
        
        break;

    case REDIR:
        rcmd = (struct redircmd*)cmd;
        close(rcmd->fd);
        if (open(rcmd->file, rcmd->mode,S_IRWXU) < 0)
        {
            fprintf(stderr, "open %s failed\n", rcmd->file);
            exit(1);
        }
        run_cmd(rcmd->cmd);
        break;

    case LIST:
        lcmd = (struct listcmd*)cmd;
        if (fork1() == 0)
            run_cmd(lcmd->left);
        wait(NULL);
        run_cmd(lcmd->right);
        break;

    case PIPE:
        pcmd = (struct pipecmd*)cmd;
        if (pipe(p) < 0)
            panic("pipe");

        // Ejecución del hijo de la izquierda
        if (fork1() == 0)
        {
            close(1);
            dup(p[1]);
            close(p[0]);
            close(p[1]);
            run_cmd(pcmd->left);
        }

        // Ejecución del hijo de la derecha
        if (fork1() == 0)
        {
            close(0);
            dup(p[0]);
            close(p[0]);
            close(p[1]);
            run_cmd(pcmd->right);
        }
        close(p[0]);
        close(p[1]);

        // Esperar a ambos hijos
        wait(NULL);
        wait(NULL);
        break;

    case BACK:
        bcmd = (struct backcmd*)cmd;
        if (fork1() == 0)
            run_cmd(bcmd->cmd);
        break;
    }

    // Salida normal, código 0.
    exit(0);
}

// Muestra un *prompt* y lee lo que el usuario escribe usando la
// librería readline. Ésta permite almacenar en el historial, utilizar
// las flechas para acceder a las órdenes previas, búsquedas de
// órdenes, etc.
char*
getcmd()
{
    char *buf;
    char buf2[TAM_BUFF];
    int retval = 0;
    uid_t uid = getuid(); 
    struct passwd* usuario = getpwuid(uid);
    if (!usuario){
	perror("passwd");
	exit(EXIT_FAILURE);
    }
    char* nombre =  usuario -> pw_name;
    char* directorio = getcwd(buf2,TAM_BUFF);
    if(!directorio){
	perror("getcwd");
	exit(EXIT_FAILURE);
     }
    char* nombredir = basename(directorio);

    char prompt[TAM_BUFF];
    snprintf(prompt,TAM_BUFF,"%s@%s$ ",nombre,nombredir);
    

    // Lee la entrada del usuario
    buf = readline (prompt);
    
    // Si el usuario ha escrito algo, almacenarlo en la historia.
    if(buf)
        add_history (buf);

    return buf;
}


//Nuestro manejador de señal lleva la cuenta del número de veces que se lanza
static int n = 1;
static void manejador(){
	
	n++;
	
}
int main()
{
    int pid;
    //Bloqueo de signals
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set,SIGINT);
    sigaddset(&set,SIGCHLD);
	//Bloquea las señales del set y controla los errores
    if(sigprocmask(SIG_BLOCK,&set,NULL)<0){
		perror("sigprocmask");
		return EXIT_FAILURE;
    }
   struct sigaction accion;
    accion.sa_handler = manejador;
    //Instalamos el manejador de señales
    sigaction(SIGCHLD,&accion,NULL);
    	
    char* buf;
    // Bucle de lectura y ejecución de órdenes.
    while (NULL != (buf = getcmd()))
    {
	//Los comandos internos exit y cd se ejecutan desde el proceso padre
	struct cmd* comando = parse_cmd(buf);
	struct execcmd *ecmd;
	ecmd = (struct execcmd*)comando;
	if (ecmd->argv[0] == 0){
	   perror("main");
	   return EXIT_FAILURE;
	}
	if((strcmp(ecmd->argv[0],"exit"))==0)
		run_exit();

	if((strcmp(ecmd->argv[0],"cd"))==0) {
	  if((run_cd(ecmd->argv[1]))==-1){
		perror("cd");
		return EXIT_FAILURE;	
		}	
	}
        //Para el resto de comandos se crea un hijo para ejecutarlo
	else{
		
		sigdelset(&set,SIGINT);
		//Bloquea las señales del set y controla los errores
		if(sigprocmask(SIG_UNBLOCK,&set,NULL)<0){
			perror("sigprocmask");
			return EXIT_FAILURE;
    		}
		
		pid = fork1();
        	if(pid == 0){
         	   run_cmd(comando);
			}
		else{
			sigset_t setch;
	    	sigemptyset(&setch);
			sigaddset(&setch,SIGCHLD);
			struct timespec timeout;
			timeout.tv_sec= TIMEOUT;
			timeout.tv_nsec = 0;
			//Esperamos al timeout
			int valor;
			valor = sigtimedwait(&setch,NULL,&timeout);
			//En caso de que se alcance el timeout matamos al proceso hijo
			if(valor<0){
				if(errno == EAGAIN){
					fprintf(stderr,"%d Matado proceso hijo PID %d	\n",n,pid);
					timeout.tv_sec= TIMEOUT;
					timeout.tv_nsec = 0;
					kill(pid,SIGKILL);
				}
				else {		
					perror("sigtimedwait");
					return EXIT_FAILURE;
				}	
			}
			//Comprobamos si el hijo ha acabado correctamente o no
			if(waitpid(pid,NULL,0) < 0){
				perror("waitpid");
				return EXIT_FAILURE;
			}
		    }
	   }

   }
     free ((void*)buf);
    return 0;
}

void
panic(char *s)
{
    fprintf(stderr, "%s\n", s);
    exit(-1);
}

// Como `fork()` salvo que muestra un mensaje de error si no se puede
// crear el hijo.
int
fork1(void)
{
    int pid;

    pid = fork();
    if(pid == -1)
        panic("fork");
    return pid;
}

// Constructores de las estructuras `cmd`.
// ----

// Construye una estructura `EXEC`.
struct cmd*
execcmd(void)
{
    struct execcmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = EXEC;
    return (struct cmd*)cmd;
}

// Construye una estructura de redirección.
struct cmd*
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
    struct redircmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = REDIR;
    cmd->cmd = subcmd;
    cmd->file = file;
    cmd->efile = efile;
    cmd->mode = mode;
    cmd->fd = fd;
    return (struct cmd*)cmd;
}

// Construye una estructura de tubería (*pipe*).
struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
    struct pipecmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = PIPE;
    cmd->left = left;
    cmd->right = right;
    return (struct cmd*)cmd;
}

// Construye una estructura de lista de órdenes.
struct cmd*
listcmd(struct cmd *left, struct cmd *right)
{
    struct listcmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = LIST;
    cmd->left = left;
    cmd->right = right;
    return (struct cmd*)cmd;
}

// Construye una estructura de ejecución que incluye una ejecución en
// segundo plano.
struct cmd*
backcmd(struct cmd *subcmd)
{
    struct backcmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = BACK;
    cmd->cmd = subcmd;
    return (struct cmd*)cmd;
}

// Parsing
// ----

const char whitespace[] = " \t\r\n\v";
const char symbols[] = "<|>&;()";

// Obtiene un *token* de la cadena de entrada `ps`, y hace que `q` apunte a
// él (si no es `NULL`).
int
gettoken(char **ps, char *end_of_str, char **q, char **eq)
{
    char *s;
    int ret;

    s = *ps;
    while (s < end_of_str && strchr(whitespace, *s))
        s++;
    if (q)
        *q = s;
    ret = *s;
    switch (*s)
    {
    case 0:
        break;
    case '|':
    case '(':
    case ')':
    case ';':
    case '&':
    case '<':
        s++;
        break;
    case '>':
        s++;
        if (*s == '>')
        {
            ret = '+';
            s++;
        }
        break;

    default:
        // El caso por defecto (no hay caracteres especiales) es el de un
        // argumento de programa. Se retorna el valor `'a'`, `q` apunta al
        // mismo (si no era `NULL`), y `ps` se avanza hasta que salta todos
        // los espacios **después** del argumento. `eq` se hace apuntar a
        // donde termina el argumento. Así, si `ret` es `'a'`:
        //
        //     |-----------+---+---+---+---+---+---+---+---+---+-----------|
        //     | (espacio) | a | r | g | u | m | e | n | t | o | (espacio) |
        //     |-----------+---+---+---+---+---+---+---+---+---+-----------|
        //                   ^                                   ^
        //                   |q                                  |eq
        //
        ret = 'a';
        while (s < end_of_str && !strchr(whitespace, *s) && !strchr(symbols, *s))
            s++;
        break;
    }

    // Apuntar `eq` (si no es `NULL`) al final del argumento.
    if (eq)
        *eq = s;

    // Y finalmente saltar los espacios en blanco y actualizar `ps`.
    while(s < end_of_str && strchr(whitespace, *s))
        s++;
    *ps = s;

    return ret;
}

// La función `peek()` recibe un puntero a una cadena, `ps`, y un final de
// cadena, `end_of_str`, y un conjunto de tokens (`toks`). El puntero
// pasado, `ps`, es llevado hasta el primer carácter que no es un espacio y
// posicionado ahí. La función retorna distinto de `NULL` si encuentra el
// conjunto de caracteres pasado en `toks` justo después de los posibles
// espacios.
int
peek(char **ps, char *end_of_str, char *toks)
{
    char *s;

    s = *ps;
    while(s < end_of_str && strchr(whitespace, *s))
        s++;
    *ps = s;

    return *s && strchr(toks, *s);
}

// Definiciones adelantadas de funciones.
struct cmd *parse_line(char**, char*);
struct cmd *parse_pipe(char**, char*);
struct cmd *parse_exec(char**, char*);
struct cmd *nulterminate(struct cmd*);

// Función principal que hace el *parsing* de una línea de órdenes dada por
// el usuario. Llama a la función `parse_line()` para obtener la estructura
// `cmd`.
struct cmd*
parse_cmd(char *s)
{
    char *end_of_str;
    struct cmd *cmd;

    end_of_str = s + strlen(s);
    cmd = parse_line(&s, end_of_str);

    peek(&s, end_of_str, "");
    if (s != end_of_str)
    {
        fprintf(stderr, "restante: %s\n", s);
        panic("syntax");
    }

    // Termina en `'\0'` todas las cadenas de caracteres de `cmd`.
    nulterminate(cmd);

    return cmd;
}

// *Parsing* de una línea. Se comprueba primero si la línea contiene alguna
// tubería. Si no, puede ser un comando en ejecución con posibles
// redirecciones o un bloque. A continuación puede especificarse que se
// ejecuta en segundo plano (con `&`) o simplemente una lista de órdenes
// (con `;`).
struct cmd*
parse_line(char **ps, char *end_of_str)
{
    struct cmd *cmd;

    cmd = parse_pipe(ps, end_of_str);
    while (peek(ps, end_of_str, "&"))
    {
        gettoken(ps, end_of_str, 0, 0);
        cmd = backcmd(cmd);
    }

    if (peek(ps, end_of_str, ";"))
    {
        gettoken(ps, end_of_str, 0, 0);
        cmd = listcmd(cmd, parse_line(ps, end_of_str));
    }

    return cmd;
}

// *Parsing* de una posible tubería con un número de órdenes.
// `parse_exec()` comprobará la orden, y si al volver el siguiente *token*
// es un `'|'`, significa que se puede ir construyendo una tubería.
struct cmd*
parse_pipe(char **ps, char *end_of_str)
{
    struct cmd *cmd;

    cmd = parse_exec(ps, end_of_str);
    if (peek(ps, end_of_str, "|"))
    {
        gettoken(ps, end_of_str, 0, 0);
        cmd = pipecmd(cmd, parse_pipe(ps, end_of_str));
    }

    return cmd;
}


// Construye los comandos de redirección si encuentra alguno de los
// caracteres de redirección.
struct cmd*
parse_redirs(struct cmd *cmd, char **ps, char *end_of_str)
{
    int tok;
    char *q, *eq;

    // Si lo siguiente que hay a continuación es una redirección...
    while (peek(ps, end_of_str, "<>"))
    {
        // La elimina de la entrada
        tok = gettoken(ps, end_of_str, 0, 0);

        // Si es un argumento, será el nombre del fichero de la
        // redirección. `q` y `eq` tienen su posición.
        if (gettoken(ps, end_of_str, &q, &eq) != 'a')
            panic("missing file for redirection");

        switch(tok)
        {
        case '<':
            cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
            break;
        case '>':
            cmd = redircmd(cmd, q, eq, O_RDWR|O_CREAT|O_TRUNC, 1);
            break;
        case '+':  // >>
            cmd = redircmd(cmd, q, eq, O_RDWR|O_CREAT|O_APPEND, 1);
            break;
        }
    }

    return cmd;
}

// *Parsing* de un bloque de órdenes delimitadas por paréntesis.
struct cmd*
parse_block(char **ps, char *end_of_str)
{
    struct cmd *cmd;

    // Esperar e ignorar el paréntesis
    if (!peek(ps, end_of_str, "("))
        panic("parse_block");
    gettoken(ps, end_of_str, 0, 0);

    // Parse de toda la línea hsta el paréntesis de cierre
    cmd = parse_line(ps, end_of_str);

    // Elimina el paréntesis de cierre
    if (!peek(ps, end_of_str, ")"))
        panic("syntax - missing )");
    gettoken(ps, end_of_str, 0, 0);

    // ¿Posibles redirecciones?
    cmd = parse_redirs(cmd, ps, end_of_str);

    return cmd;
}

// Hace en *parsing* de una orden, a no ser que la expresión comience por
// un paréntesis. En ese caso, se inicia un grupo de órdenes para ejecutar
// las órdenes de dentro del paréntesis (llamando a `parse_block()`).
struct cmd*
parse_exec(char **ps, char *end_of_str)
{
    char *q, *eq;
    int tok, argc;
    struct execcmd *cmd;
    struct cmd *ret;

    // ¿Inicio de un bloque?
    if (peek(ps, end_of_str, "("))
        return parse_block(ps, end_of_str);

    // Si no, lo primero que hay una línea siempre es una orden. Se
    // construye el `cmd` usando la estructura `execcmd`.
    ret = execcmd();
    cmd = (struct execcmd*)ret;

    // Bucle para separar los argumentos de las posibles redirecciones.
    argc = 0;
    ret = parse_redirs(ret, ps, end_of_str);
    while (!peek(ps, end_of_str, "|)&;"))
    {
        if ((tok=gettoken(ps, end_of_str, &q, &eq)) == 0)
            break;

        // Aquí tiene que reconocerse un argumento, ya que el bucle para
        // cuando hay un separador
        if (tok != 'a')
            panic("syntax");

        // Apuntar el siguiente argumento reconocido. El primero será la
        // orden a ejecutar.
        cmd->argv[argc] = q;
        cmd->eargv[argc] = eq;
        argc++;
        if (argc >= MAXARGS)
            panic("too many args");

        // Y de nuevo apuntar posibles redirecciones
        ret = parse_redirs(ret, ps, end_of_str);
    }

    // Finalizar las líneas de órdenes
    cmd->argv[argc] = 0;
    cmd->eargv[argc] = 0;

    return ret;
}

// Termina en NUL todas las cadenas de `cmd`.
struct cmd*
nulterminate(struct cmd *cmd)
{
    int i;
    struct backcmd *bcmd;
    struct execcmd *ecmd;
    struct listcmd *lcmd;
    struct pipecmd *pcmd;
    struct redircmd *rcmd;

    if(cmd == 0)
        return 0;

    switch(cmd->type)
    {
    case EXEC:
        ecmd = (struct execcmd*)cmd;
        for(i=0; ecmd->argv[i]; i++)
            *ecmd->eargv[i] = 0;
        break;

    case REDIR:
        rcmd = (struct redircmd*)cmd;
        nulterminate(rcmd->cmd);
        *rcmd->efile = 0;
        break;

    case PIPE:
        pcmd = (struct pipecmd*)cmd;
        nulterminate(pcmd->left);
        nulterminate(pcmd->right);
        break;

    case LIST:
        lcmd = (struct listcmd*)cmd;
        nulterminate(lcmd->left);
        nulterminate(lcmd->right);
        break;

    case BACK:
        bcmd = (struct backcmd*)cmd;
        nulterminate(bcmd->cmd);
        break;
    }

    return cmd;
}

/*
 * Local variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 75
 * eval: (auto-fill-mode t)
 * End:
 */
