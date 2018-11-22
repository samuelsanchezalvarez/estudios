/*
 * Shell `simplesh` (basado en el shell de xv6)
 *
 * Ampliación de Sistemas Operativos
 * Departamento de Ingeniería y Tecnología de Computadores
 * Facultad de Informática de la Universidad de Murcia
 *
 * Alumnos: BLEDA TORRES, LUIS (G3.1)
 *          SÁNCHEZ ÁLVAREZ, SAMUEL (G3.2)
 *
 * Convocatoria: FEBRERO/JUNIO/JULIO
 */


/*
 * Ficheros de cabecera
 */


#define _POSIX_C_SOURCE 200809L /* IEEE 1003.1-2008 (véase /usr/include/features.h) */
//#define NDEBUG                /* Traduce asertos y DMACROS a 'no ops' */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#include <libgen.h>
#include <signal.h>
// Biblioteca readline
#include <readline/readline.h>
#include <readline/history.h>


/******************************************************************************
 * Constantes, macros y variables globales
 ******************************************************************************/


static const char* VERSION = "0.18";

// Niveles de depuración
#define DBG_CMD   (1 << 0)
#define DBG_TRACE (1 << 1)
// . . .
static int g_dbg_level = 0;

#ifndef NDEBUG
#define DPRINTF(dbg_level, fmt, ...)                            \
    do {                                                        \
        if (dbg_level & g_dbg_level)                            \
            fprintf(stderr, "%s:%d:%s(): " fmt,                 \
                    __FILE__, __LINE__, __func__, ##__VA_ARGS__);       \
    } while ( 0 )

#define DBLOCK(dbg_level, block)                                \
    do {                                                        \
        if (dbg_level & g_dbg_level)                            \
            block;                                              \
    } while( 0 );
#else
#define DPRINTF(dbg_level, fmt, ...)
#define DBLOCK(dbg_level, block)
#endif

#define TRY(x)                                                  \
    do {                                                        \
        int __rc = (x);                                         \
        if( __rc < 0 ) {                                        \
            fprintf(stderr, "%s:%d:%s: TRY(%s) failed\n",       \
                    __FILE__, __LINE__, __func__, #x);          \
            fprintf(stderr, "ERROR: rc=%d errno=%d (%s)\n",     \
                    __rc, errno, strerror(errno));              \
            exit(EXIT_FAILURE);                                 \
        }                                                       \
    } while( 0 )


// Número máximo de argumentos de un comando
#define MAX_ARGS 16
#define MIN_T 1 //Tamaño minimo de bloque
#define MAX_T 1048576 //Tamaño maximo de bloque
#define MAX_PROC_BACK 8
#define MAX_LINEA 4096	

// Delimitadores
static const char WHITESPACE[] = " \t\r\n\v";
// Caracteres especiales
static const char SYMBOLS[] = "<|>&;()";


/******************************************************************************
 * Funciones auxiliares
 ******************************************************************************/
int run_src(char** comando, int num_argc);
// Imprime el mensaje
void info(const char *fmt, ...)
{
    va_list arg;

    fprintf(stdout, "%s: ", __FILE__);
    va_start(arg, fmt);
    vfprintf(stdout, fmt, arg);
    va_end(arg);
}


// Imprime el mensaje de error
void error(const char *fmt, ...)
{
    va_list arg;

    fprintf(stderr, "%s: ", __FILE__);
    va_start(arg, fmt);
    vfprintf(stderr, fmt, arg);
    va_end(arg);
}


// Imprime el mensaje de error y aborta la ejecución
void panic(const char *fmt, ...)
{
    va_list arg;

    fprintf(stderr, "%s: ", __FILE__);
    va_start(arg, fmt);
    vfprintf(stderr, fmt, arg);
    va_end(arg);

    exit(EXIT_FAILURE);
}


// `fork()` que muestra un mensaje de error si no se puede crear el hijo
int fork_or_panic(const char* s)
{
    int pid;

    pid = fork();
    if(pid == -1)
        panic("%s failed: errno %d (%s)", s, errno, strerror(errno));
    return pid;
}


/******************************************************************************
 * Estructuras de datos `cmd`
 ******************************************************************************/


// Las estructuras `cmd` se utilizan para almacenar información que servirá a
// simplesh para ejecutar líneas de órdenes con redirecciones, tuberías, listas
// de comandos y tareas en segundo plano. El formato es el siguiente:

//     |----------+--------------+--------------|
//     | (1 byte) | ...          | ...          |
//     |----------+--------------+--------------|
//     | type     | otros campos | otros campos |
//     |----------+--------------+--------------|

// Nótese cómo las estructuras `cmd` comparten el primer campo `type` para
// identificar su tipo. A partir de él se obtiene un tipo derivado a través de
// *casting* forzado de tipo. Se consigue así polimorfismo básico en C.

// Valores del campo `type` de las estructuras de datos `cmd`
enum cmd_type { EXEC=1, REDR=2, PIPE=3, LIST=4, BACK=5, SUBS=6, INV=7 };

struct cmd { enum cmd_type type; };

// Comando con sus parámetros
struct execcmd {
    enum cmd_type type;
    char* argv[MAX_ARGS];
    char* eargv[MAX_ARGS];
    int argc;
};

// Comando con redirección
struct redrcmd {
    enum cmd_type type;
    struct cmd* cmd;
    char* file;
    char* efile;
    int flags;
    mode_t mode;
    int fd;
};

// Comandos con tubería
struct pipecmd {
    enum cmd_type type;
    struct cmd* left;
    struct cmd* right;
};

// Lista de órdenes
struct listcmd {
    enum cmd_type type;
    struct cmd* left;
    struct cmd* right;
};

// Tarea en segundo plano (background) con `&`
struct backcmd {
    enum cmd_type type;
    struct cmd* cmd;
};

// Subshell
struct subscmd {
    enum cmd_type type;
    struct cmd* cmd;
};

struct procesos_background{
	int procesos[MAX_PROC_BACK];
	int posicion;
};
struct procesos_background* procesosback;

/******************************************************************************
 * Funciones para construir las estructuras de datos `cmd`
 ******************************************************************************/


// Construye una estructura `cmd` de tipo `EXEC`
struct cmd* execcmd(void)
{
    struct execcmd* cmd;

    if ((cmd = malloc(sizeof(*cmd))) == NULL)
    {
        perror("execcmd: malloc");
        exit(EXIT_FAILURE);
    }
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = EXEC;

    return (struct cmd*) cmd;
}

// Construye una estructura `cmd` de tipo `REDR`
struct cmd* redrcmd(struct cmd* subcmd,
        char* file, char* efile,
        int flags, mode_t mode, int fd)
{
    struct redrcmd* cmd;

    if ((cmd = malloc(sizeof(*cmd))) == NULL)
    {
        perror("redrcmd: malloc");
        exit(EXIT_FAILURE);
    }
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = REDR;
    cmd->cmd = subcmd;
    cmd->file = file;
    cmd->efile = efile;
    cmd->flags = flags;
    cmd->mode = mode;
    cmd->fd = fd;

    return (struct cmd*) cmd;
}

// Construye una estructura `cmd` de tipo `PIPE`
struct cmd* pipecmd(struct cmd* left, struct cmd* right)
{
    struct pipecmd* cmd;

    if ((cmd = malloc(sizeof(*cmd))) == NULL)
    {
        perror("pipecmd: malloc");
        exit(EXIT_FAILURE);
    }
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = PIPE;
    cmd->left = left;
    cmd->right = right;

    return (struct cmd*) cmd;
}

// Construye una estructura `cmd` de tipo `LIST`
struct cmd* listcmd(struct cmd* left, struct cmd* right)
{
    struct listcmd* cmd;

    if ((cmd = malloc(sizeof(*cmd))) == NULL)
    {
        perror("listcmd: malloc");
        exit(EXIT_FAILURE);
    }
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = LIST;
    cmd->left = left;
    cmd->right = right;

    return (struct cmd*)cmd;
}

// Construye una estructura `cmd` de tipo `BACK`
struct cmd* backcmd(struct cmd* subcmd)
{
    struct backcmd* cmd;

    if ((cmd = malloc(sizeof(*cmd))) == NULL)
    {
        perror("backcmd: malloc");
        exit(EXIT_FAILURE);
    }
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = BACK;
    cmd->cmd = subcmd;

    return (struct cmd*)cmd;
}

// Construye una estructura `cmd` de tipo `SUB`
struct cmd* subscmd(struct cmd* subcmd)
{
    struct subscmd* cmd;

    if ((cmd = malloc(sizeof(*cmd))) == NULL)
    {
        perror("subscmd: malloc");
        exit(EXIT_FAILURE);
    }
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = SUBS;
    cmd->cmd = subcmd;

    return (struct cmd*) cmd;
}

struct procesos_background* proback(){
	struct procesos_background* procesos;
	if ((procesos = malloc(sizeof(struct procesos_background))) == NULL)
    	{
        	perror("procesos_background: malloc");
        	exit(EXIT_FAILURE);
    	}
	procesos->posicion=0;
	return procesos;
}

/******************************************************************************
 * Funciones para realizar el análisis sintáctico de la línea de órdenes
 ******************************************************************************/


// `get_token` recibe un puntero al principio de una cadena (`start_of_str`),
// otro puntero al final de esa cadena (`end_of_str`) y, opcionalmente, dos
// punteros para guardar el principio y el final del token, respectivamente.
//
// `get_token` devuelve un *token* de la cadena de entrada.

int get_token(char** start_of_str, char* end_of_str,
        char** start_of_token, char** end_of_token)
{
    char* s;
    int ret;

    // Salta los espacios en blanco
    s = *start_of_str;
    while (s < end_of_str && strchr(WHITESPACE, *s))
        s++;

    // `start_of_token` apunta al principio del argumento (si no es NULL)
    if (start_of_token)
        *start_of_token = s;

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

            // El caso por defecto (cuando no hay caracteres especiales) es el
            // de un argumento de un comando. `get_token` devuelve el valor
            // `'a'`, `start_of_token` apunta al argumento (si no es `NULL`),
            // `end_of_token` apunta al final del argumento (si no es `NULL`) y
            // `start_of_str` avanza hasta que salta todos los espacios
            // *después* del argumento. Por ejemplo:
            //
            //     |-----------+---+---+---+---+---+---+---+---+---+-----------|
            //     | (espacio) | a | r | g | u | m | e | n | t | o | (espacio)
            //     |
            //     |-----------+---+---+---+---+---+---+---+---+---+-----------|
            //                   ^                                   ^
            //            start_o|f_token                       end_o|f_token

            ret = 'a';
            while (s < end_of_str &&
                    !strchr(WHITESPACE, *s) &&
                    !strchr(SYMBOLS, *s))
                s++;
            break;
    }

    // `end_of_token` apunta al final del argumento (si no es `NULL`)
    if (end_of_token)
        *end_of_token = s;

    // Salta los espacios en blanco
    while (s < end_of_str && strchr(WHITESPACE, *s))
        s++;

    // Actualiza `start_of_str`
    *start_of_str = s;

    return ret;
}


// `peek` recibe un puntero al principio de una cadena (`start_of_str`), otro
// puntero al final de esa cadena (`end_of_str`) y un conjunto de caracteres
// (`delimiter`).
//
// El primer puntero pasado como parámero (`start_of_str`) avanza hasta el
// primer carácter que no está en el conjunto de caracteres `WHITESPACE`.
//
// `peek` devuelve un valor distinto de `NULL` si encuentra alguno de los
// caracteres en `delimiter` justo después de los caracteres en `WHITESPACE`.

int peek(char** start_of_str, char* end_of_str, char* delimiter)
{
    char* s;

    s = *start_of_str;
    while (s < end_of_str && strchr(WHITESPACE, *s))
        s++;
    *start_of_str = s;

    return *s && strchr(delimiter, *s);
}


// Definiciones adelantadas de funciones
struct cmd* parse_line(char**, char*);
struct cmd* parse_pipe(char**, char*);
struct cmd* parse_exec(char**, char*);
struct cmd* parse_subs(char**, char*);
struct cmd* parse_redr(struct cmd*, char**, char*);
struct cmd* null_terminate(struct cmd*);


// `parse_cmd` realiza el *análisis sintáctico* de la línea de órdenes
// introducida por el usuario.
//
// `parse_cmd` utiliza `parse_line` para obtener una estructura `cmd`.

struct cmd* parse_cmd(char* start_of_str)
{
    char* end_of_str;
    struct cmd* cmd;

    DPRINTF(DBG_TRACE, "STR\n");

    end_of_str = start_of_str + strlen(start_of_str);

    cmd = parse_line(&start_of_str, end_of_str);

    // Comprueba que se ha alcanzado el final de la línea de órdenes
    peek(&start_of_str, end_of_str, "");
    if (start_of_str != end_of_str)
        error("%s: error sintáctico: %s\n", __func__);

    DPRINTF(DBG_TRACE, "END\n");
   
    return cmd;
}


// `parse_line` realiza el análisis sintáctico de la línea de órdenes
// introducida por el usuario.
//
// `parse_line` comprueba en primer lugar si la línea contiene alguna tubería.
// Para ello `parse_line` llama a `parse_pipe` que a su vez verifica si hay
// bloques de órdenes y/o redirecciones.  A continuación, `parse_line`
// comprueba si la ejecución de la línea se realiza en segundo plano (con `&`)
// o si la línea de órdenes contiene una lista de órdenes (con `;`).

struct cmd* parse_line(char** start_of_str, char* end_of_str)
{
    struct cmd* cmd;
    int delimiter;

    cmd = parse_pipe(start_of_str, end_of_str);

    while (peek(start_of_str, end_of_str, "&"))
    {
        // Consume el delimitador de tarea en segundo plano
        delimiter = get_token(start_of_str, end_of_str, 0, 0);
        assert(delimiter == '&');

        // Construye el `cmd` para la tarea en segundo plano
        cmd = backcmd(cmd);
    }

    if (peek(start_of_str, end_of_str, ";"))
    {
        if (cmd->type == EXEC && ((struct execcmd*) cmd)->argv[0] == 0)
            error("%s: error sintáctico: no se encontró comando\n", __func__);

        // Consume el delimitador de lista de órdenes
        delimiter = get_token(start_of_str, end_of_str, 0, 0);
        assert(delimiter == ';');

        // Construye el `cmd` para la lista
        cmd = listcmd(cmd, parse_line(start_of_str, end_of_str));
    }

    return cmd;
}


// `parse_pipe` realiza el análisis sintáctico de una tubería de manera
// recursiva si encuentra el delimitador de tuberías '|'.
//
// `parse_pipe` llama a `parse_exec` y `parse_pipe` de manera recursiva para
// realizar el análisis sintáctico de todos los componentes de la tubería.

struct cmd* parse_pipe(char** start_of_str, char* end_of_str)
{
    struct cmd* cmd;
    int delimiter;

    cmd = parse_exec(start_of_str, end_of_str);

    if (peek(start_of_str, end_of_str, "|"))
    {
        if (cmd->type == EXEC && ((struct execcmd*) cmd)->argv[0] == 0)
            error("%s: error sintáctico: no se encontró comando\n", __func__);

        // Consume el delimitador de tubería
        delimiter = get_token(start_of_str, end_of_str, 0, 0);
        assert(delimiter == '|');

        // Construye el `cmd` para la tubería
        cmd = pipecmd(cmd, parse_pipe(start_of_str, end_of_str));
    }

    return cmd;
}


// `parse_exec` realiza el análisis sintáctico de un comando a no ser que la
// expresión comience por un paréntesis, en cuyo caso se llama a `parse_subs`.
//
// `parse_exec` reconoce las redirecciones antes y después del comando.

struct cmd* parse_exec(char** start_of_str, char* end_of_str)
{
    char* start_of_token;
    char* end_of_token;
    int token, argc;
    struct execcmd* cmd;
    struct cmd* ret;

    // ¿Inicio de un bloque?
    if (peek(start_of_str, end_of_str, "("))
        return parse_subs(start_of_str, end_of_str);

    // Si no, lo primero que hay en una línea de órdenes es un comando

    // Construye el `cmd` para el comando
    ret = execcmd();
    cmd = (struct execcmd*) ret;

    // ¿Redirecciones antes del comando?
    ret = parse_redr(ret, start_of_str, end_of_str);

    // Bucle para separar los argumentos de las posibles redirecciones
    argc = 0;
    while (!peek(start_of_str, end_of_str, "|)&;"))
    {
        if ((token = get_token(start_of_str, end_of_str,
                        &start_of_token, &end_of_token)) == 0)
            break;

        // El siguiente token debe ser un argumento porque el bucle
        // para en los delimitadores
        if (token != 'a')
            error("%s: error sintáctico: se esperaba un argumento\n", __func__);

        // Almacena el siguiente argumento reconocido. El primero es
        // el comando
        cmd->argv[argc] = start_of_token;
        cmd->eargv[argc] = end_of_token;
        cmd->argc = ++argc;
        if (argc >= MAX_ARGS)
            panic("%s: demasiados argumentos\n", __func__);

        // ¿Redirecciones después del comando?
        ret = parse_redr(ret, start_of_str, end_of_str);
    }

    // El comando no tiene más parámetros
    cmd->argv[argc] = 0;
    cmd->eargv[argc] = 0; 
    
    return ret;
}


// `parse_subs` realiza el análisis sintáctico de un bloque de órdenes
// delimitadas por paréntesis o `subshell` llamando a `parse_line`.
//
// `parse_subs` reconoce las redirecciones después del bloque de órdenes.

struct cmd* parse_subs(char** start_of_str, char* end_of_str)
{
    int delimiter;
    struct cmd* cmd;
    struct cmd* scmd;

    // Consume el paréntesis de apertura
    if (!peek(start_of_str, end_of_str, "("))
        error("%s: error sintáctico: se esperaba '('", __func__);
    delimiter = get_token(start_of_str, end_of_str, 0, 0);
    assert(delimiter == '(');

    // Realiza el análisis sintáctico hasta el paréntesis de cierre
    scmd = parse_line(start_of_str, end_of_str);

    // Construye el `cmd` para el bloque de órdenes
    cmd = subscmd(scmd);

    // Consume el paréntesis de cierre
    if (!peek(start_of_str, end_of_str, ")"))
        error("%s: error sintáctico: se esperaba ')'", __func__);
    delimiter = get_token(start_of_str, end_of_str, 0, 0);
    assert(delimiter == ')');

    // ¿Redirecciones después del bloque de órdenes?
    cmd = parse_redr(cmd, start_of_str, end_of_str);

    return cmd;
}


// `parse_redr` realiza el análisis sintáctico de órdenes con
// redirecciones si encuentra alguno de los delimitadores de
// redirección ('<' o '>').

struct cmd* parse_redr(struct cmd* cmd, char** start_of_str, char* end_of_str)
{
    int delimiter;
    char* start_of_token;
    char* end_of_token;

    // Si lo siguiente que hay a continuación es delimitador de
    // redirección...
    while (peek(start_of_str, end_of_str, "<>"))
    {
        // Consume el delimitador de redirección
        delimiter = get_token(start_of_str, end_of_str, 0, 0);
        assert(delimiter == '<' || delimiter == '>' || delimiter == '+');

        // El siguiente token tiene que ser el nombre del fichero de la
        // redirección entre `start_of_token` y `end_of_token`.
        if ('a' != get_token(start_of_str, end_of_str, &start_of_token, &end_of_token))
            error("%s: error sintáctico: se esperaba un fichero", __func__);

        // Construye el `cmd` para la redirección
        switch(delimiter)
        {
            case '<':
                cmd = redrcmd(cmd, start_of_token, end_of_token, O_RDONLY, 0, 0);
                break;
            case '>':
                cmd = redrcmd(cmd, start_of_token, end_of_token, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU, 1);
                break;
            case '+': // >>
                cmd = redrcmd(cmd, start_of_token, end_of_token, O_RDWR|O_CREAT|O_APPEND, S_IRWXU, 1);
                break;
        }
    }

    return cmd;
}


// Termina en NULL todas las cadenas de las estructuras `cmd`
struct cmd* null_terminate(struct cmd* cmd)
{
    struct execcmd* ecmd;
    struct redrcmd* rcmd;
    struct pipecmd* pcmd;
    struct listcmd* lcmd;
    struct backcmd* bcmd;
    struct subscmd* scmd;
    int i;

    if(cmd == 0)
        return 0;

    switch(cmd->type)
    {
        case EXEC:
            ecmd = (struct execcmd*) cmd;
            for(i = 0; ecmd->argv[i]; i++)
                *ecmd->eargv[i] = 0;
            break;

        case REDR:
            rcmd = (struct redrcmd*) cmd;
            null_terminate(rcmd->cmd);
            *rcmd->efile = 0;
            break;

        case PIPE:
            pcmd = (struct pipecmd*) cmd;
            null_terminate(pcmd->left);
            null_terminate(pcmd->right);
            break;

        case LIST:
            lcmd = (struct listcmd*) cmd;
            null_terminate(lcmd->left);
            null_terminate(lcmd->right);
            break;

        case BACK:
            bcmd = (struct backcmd*) cmd;
            null_terminate(bcmd->cmd);
            break;

        case SUBS:
            scmd = (struct subscmd*) cmd;
            null_terminate(scmd->cmd);
            break;

        case INV:
        default:
            panic("%s: estructura `cmd` desconocida\n", __func__);
    }

    return cmd;
}

/******************************************************************************
 * Comandos internos *
 ******************************************************************************/
int write_all(int fd, char* buf, size_t num){
	int total=0;
        while(total!=num){
              
                  total+=write(fd,buf+total,num-total);
        }
	return total;
}
void add_proceso_background(struct procesos_background* procesos, int pid){
	while(procesos->procesos[procesos->posicion] != 0) 
		procesos->posicion=(procesos->posicion+1)%MAX_PROC_BACK;
	procesos->procesos[procesos->posicion]=pid;
		
	
}
void delete_proceso_background(struct procesos_background* procesos, int pid){
	for(int i=0;i<MAX_PROC_BACK;i++){
		if(pid==procesos->procesos[i]){
			procesos->procesos[i]=0;
		}
	}
}
void handle_sigchld(int sig) {
  	int saved_errno = errno;
	pid_t pid;
  	while ((pid=waitpid((pid_t)(-1), 0, WNOHANG)) > 0) {
		char buf[8];
		sprintf(buf,"[%d]",pid);
		write_all(STDOUT_FILENO,buf,strlen(buf));
		delete_proceso_background(procesosback,pid);
	}
  	errno = saved_errno;
}

int is_interno(char* comando){
   if(comando!=NULL)
   	return ((strcmp(comando,"cwd") == 0) || (strcmp(comando,"exit") == 0) || (strcmp(comando,"cd") == 0) || (strcmp(comando,"hd") == 0) || (strcmp(comando,"src") == 0) || (strcmp(comando,"bjobs") == 0));
   return 0;

}
void run_cwd(){
    char ruta[PATH_MAX];
    if(!getcwd(ruta,PATH_MAX)){
	perror("getcwd");
	exit(EXIT_FAILURE);
    }
    printf("cwd: %s\n",ruta);
}
int run_cd(char* path){
	//TODO Preguntar por test 16
	char dir_actual[PATH_MAX];
	if(path==NULL){
				
		path=getenv("HOME");
 	}
	else if((strcmp(path,"-")==0)){
		if(getenv("OLDPWD")==NULL){
			printf("run_cd: Variable OLDPWD no definida\n");
			return 0;
		}
		else
			path=getenv("OLDPWD");
	}
	setenv("OLDPWD",getcwd(dir_actual,PATH_MAX),1);			
	int retorno=chdir(path);
	if(retorno==-1){
	  printf("run_cd: No existe el directorio '%s'\n",path);
	  return 0;
 	}
	
	return 0;
}
void run_exit(struct execcmd* cmd){
    free(procesosback);
    free(cmd);
    exit(EXIT_SUCCESS);
}

int run_hd(char** comando, int num_argc){
	 int opt, flag, n;
	 int fd=STDIN_FILENO;
	 int num_lineas=3;
	 int bytes=1;
	 int valor=num_argc;
	 int bloque=1024; //Preguntar por que con tama 64 falla la prueba
   	 flag = n = 0;
	 optind = 1;
	 while ((opt = getopt(num_argc, comando, "l:b:t:h")) != -1) {
		switch (opt) {
		    case 'l':
			if(flag==2){
				printf("hd: Opciones incompatibles\n");
				return 0;
                        }
                        num_lineas = atoi(optarg);
			flag=1;
		        break;
		    case 'b':
		      if(flag==1){
				printf("hd: Opciones incompatibles\n");
				return 0;
                        }
			bytes=atoi(optarg);
			flag=2;
		        break;
	            case 't':
			bloque=atoi(optarg);
			if(bloque< MIN_T || bloque > MAX_T){
				printf("hd: Opción no válida\n");
				return 0;
			}
			break;
		    case 'h':
		        printf("Uso: hd [-l NLINES] [-b NBYTES] [-t BSIZE] [FILE1] [FILE2 ]...\n\tOpciones:\n\t-l NLINES Número máximo de líneas a mostrar.\n\t-b NBYTES Número máximo de  bytes a mostrar.\n\t-t BSIZE   Tamaño en  bytes  de los  bloques  leídos de [FILEn] o stdin.\n\t-h help\n");
			
			return 0;
		       
		}
	    }

	  int num_ficheros=num_argc-optind;
	  do{

		
				int lineas_leidas=0;
				int bytes_leidos=1;
				int bytes_a_escribir=0;
				int bytes_restantes=bytes;
				if(num_ficheros!=0)
					fd=open(comando[optind],O_RDONLY);
				while(bytes_leidos!=0){
					int i=0;
					char* buf=malloc(sizeof(char)*bloque);
					bytes_leidos=read(fd,buf,bloque);
					if(bytes_leidos==-1){
						printf("hd: No se encontró el archivo '%s'\n",comando[optind]);
						return 0;
					}
					if(bytes_leidos<bloque){
						bytes_a_escribir=bytes_leidos;
					}
					else
						bytes_a_escribir=bloque;
					if(flag==2){
					
						if(bytes_restantes>bloque){
							bytes_restantes-=write_all(STDOUT_FILENO,buf,bytes_a_escribir);
						
						}
						else{
							write_all(STDOUT_FILENO,buf,bytes_restantes);
							bytes_leidos=0;
						}
					}
					else{
			
						while( i<bytes_leidos && ((flag<2 && lineas_leidas!=num_lineas) || (flag==2 && i<bytes))){
						
							char c=buf[i];						
							if(flag!=2 && c=='\n')
								lineas_leidas++;
							i++;						
						}					
						if(bytes_leidos!=0){
							if(i<bloque){
								write_all(STDOUT_FILENO,buf,i);
							}
							else 
								write_all(STDOUT_FILENO,buf,bytes_a_escribir);
						}
						if(lineas_leidas==num_lineas)
							bytes_leidos=0;	
						free(buf);				
						}	
				}
				if(num_ficheros!=0)
					close(fd);
				optind++;

		
	
	  }while(optind<valor);
	 
	  return 0;

}

int run_bjobs(char** comando,int num_argc){
	int opt, flag, n;
	flag = n = 0;
	 optind = 1;
	 while ((opt = getopt(num_argc, comando, "sch")) != -1) {
		switch (opt) {
		    case 's':
			if(flag==2){
				printf("bjobs: Opciones incompatibles\n");
				return 0;
                        }
                
			flag=1;
		        break;
		    case 'c':
		      if(flag==1){
				printf("bjobs: Opciones incompatibles\n");
				return 0;
                        }
			flag=2;
		        break;
		    case 'h':
		        printf("Uso: bjobs [-s] [-c] [-h]\n\t Opciones:\n\t -s Suspende  todos  los  procesos  en  segundo  plano.\n\t-c Reanuda  todos  los  procesos  en  segundo  plano.\n\t-h help\n");
			
			return 0;
		       
		}
	    }
	for (int i=0; i<MAX_PROC_BACK;i++){
		if(flag==1){
			if(procesosback->procesos[i]!=0)
				kill(procesosback->procesos[i],SIGSTOP);
		}
		else if(flag==2){
			if(procesosback->procesos[i]!=0)
				kill(procesosback->procesos[i],SIGCONT);
		}
		else
			if(procesosback->procesos[i]!=0)
				printf("[%d]\n",procesosback->procesos[i]);
	}
	return 0;
}
void run_interno(struct execcmd* cmd){
	if((strcmp(cmd->argv[0],"cwd")==0)){
		run_cwd();
	}
	else if((strcmp(cmd->argv[0],"exit")==0)){
		run_exit(cmd);
	}
	else if((strcmp(cmd->argv[0],"cd")==0)){
		if(cmd->argc>2){
			printf("run_cd: Demasiados argumentos\n");
		}
		else{
			run_cd(cmd->argv[1]);
		}
	}
	else if((strcmp(cmd->argv[0],"hd")==0)){
		run_hd(cmd->argv, cmd->argc);
	}
	else if((strcmp(cmd->argv[0],"src")==0)){
		run_src(cmd->argv, cmd->argc);
	}
	else if((strcmp(cmd->argv[0],"bjobs")==0)){
		run_bjobs(cmd->argv,cmd->argc);
	}
	    
}

/******************************************************************************
 * Funciones para la ejecución de la línea de órdenes
 ******************************************************************************/


void exec_cmd(struct execcmd* ecmd)
{
    
    assert(ecmd->type == EXEC);

    if (ecmd->argv[0] == 0) exit(EXIT_SUCCESS);

    execvp(ecmd->argv[0], ecmd->argv);

    panic("no se encontró el comando '%s'\n", ecmd->argv[0]);
}


void run_cmd(struct cmd* cmd)
{
    struct execcmd* ecmd;
    struct redrcmd* rcmd;
    struct listcmd* lcmd;
    struct pipecmd* pcmd;
    struct backcmd* bcmd;
    struct subscmd* scmd;
    int p[2];
    int fd;
    int pid;

    DPRINTF(DBG_TRACE, "STR\n");

    if(cmd == 0) return;
    sigset_t  blocked_signals;

            sigemptyset (& blocked_signals);

            sigaddset (& blocked_signals , SIGCHLD);

    switch(cmd->type)
    {
        case EXEC:
	    if (sigprocmask(SIG_BLOCK , &blocked_signals , NULL) ==  -1) {
                perror("sigprocmask");

                exit(EXIT_FAILURE);
        }

            ecmd = (struct execcmd*) cmd;
		
	    if(is_interno(ecmd->argv[0])!=0){
		run_interno(ecmd);
	     }
	    else{
                    pid=fork_or_panic("fork EXEC");
		    if (pid == 0)
		        exec_cmd(ecmd);
		    TRY(waitpid(pid,NULL,0));
            }
	      if (sigprocmask(SIG_UNBLOCK , &blocked_signals , NULL) ==  -1) {
                perror("sigprocmask");

                exit(EXIT_FAILURE);
        }

            break;
	
        case REDR:
	    if (sigprocmask(SIG_BLOCK , &blocked_signals , NULL) ==  -1) {
                perror("sigprocmask");

                exit(EXIT_FAILURE);
	        }

	     rcmd = (struct redrcmd*) cmd;
	      ecmd=(struct execcmd*) rcmd->cmd;
              if(is_interno(ecmd->argv[0])){
		if((strcmp(ecmd->argv[0],"hd") == 0) || strcmp(ecmd->argv[0],"src")==0)
		      TRY( close(rcmd->fd) );
		       if ((fd = open(rcmd->file, rcmd->flags,rcmd->mode)) < 0)
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }

                     run_interno(ecmd);

               }
            else{

            pid=fork_or_panic("fork REDR");
            if (pid == 0)
            {
                TRY( close(rcmd->fd) );
                if ((fd = open(rcmd->file, rcmd->flags,rcmd->mode)) < 0)
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }

                if (rcmd->cmd->type == EXEC){
		    ecmd=(struct execcmd*) rcmd->cmd;
                    exec_cmd((ecmd));
                }
                    
		
                else
                    run_cmd(rcmd->cmd);
                exit(EXIT_SUCCESS);
            }
            TRY(waitpid(pid,NULL,0));
	     if (sigprocmask(SIG_UNBLOCK , &blocked_signals , NULL) ==  -1) {
                perror("sigprocmask");

                exit(EXIT_FAILURE);
       		 }
	}
            break;

        case LIST:
            lcmd = (struct listcmd*) cmd;
            run_cmd(lcmd->left);
            run_cmd(lcmd->right);
            break;

        case PIPE:
            pcmd = (struct pipecmd*)cmd;
            if (pipe(p) < 0)
            {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
	
	
	if (sigprocmask(SIG_BLOCK , &blocked_signals , NULL) ==  -1) {
		perror("sigprocmask");

		exit(EXIT_FAILURE);
	}
	    int pid_I=fork_or_panic("fork PIPE left");
            // Ejecución del hijo de la izquierda
            if (pid_I == 0)
            {
                TRY( close(1) );
                TRY( dup(p[1]) );
                TRY( close(p[0]) );
                TRY( close(p[1]) );
                if (pcmd->left->type == EXEC){
		    ecmd=(struct execcmd*) pcmd->left;
		    if(is_interno(ecmd->argv[0])){
			run_interno(ecmd);
	    	    }	
		   else
                    exec_cmd((struct execcmd*) pcmd->left);
		}
	        else
                    run_cmd(pcmd->left);
             exit(EXIT_SUCCESS);
		
	     }
            
            
		
            // Ejecución del hijo de la derecha
 	    int pid_D=fork_or_panic("fork PIPE right");
            if (pid_D == 0)
            {
                TRY( close(0) );
                TRY( dup(p[0]) );
                TRY( close(p[0]) );
                TRY( close(p[1]) );
                if (pcmd->right->type == EXEC){
                   ecmd=(struct execcmd*) pcmd->right;
		    if(is_interno(ecmd->argv[0])){
			run_interno(ecmd);
	    	    }	
		   else{
                    exec_cmd((struct execcmd*) pcmd->right);
		}
	     }
            	else
                    run_cmd(pcmd->right);
                exit(EXIT_SUCCESS);
            
	    }
	    TRY(close(p[0]));
	    TRY(close(p[1]));
            // Esperar a ambos hijos
            TRY( waitpid(pid_I,NULL,0) );
            TRY( waitpid(pid_D,NULL,0) );
	    if (sigprocmask(SIG_UNBLOCK , &blocked_signals , NULL) ==  -1) {
		perror("sigprocmask");

		exit(EXIT_FAILURE);
	}
	    
            break;

        case BACK:
            bcmd = (struct backcmd*)cmd;
	    pid=fork_or_panic("fork BACK");
            if (pid == 0)
            {
		
		
                if (bcmd->cmd->type == EXEC){
		   //TODO: Revisar procesos en background (test 19 tutoría profesor)
                    ecmd=(struct execcmd*) bcmd->cmd;
		    if(is_interno(ecmd->argv[0])){
			run_interno(ecmd);
	    	    }	
		   else{
                    exec_cmd((struct execcmd*) bcmd->cmd);
		}
		
		}
                else{
		    
                    run_cmd(bcmd->cmd);
		}
                exit(EXIT_SUCCESS);
            }
	    add_proceso_background(procesosback,pid);
	    printf("[%d]\n",pid);
	    
            break;

        case SUBS:
            scmd = (struct subscmd*) cmd;
            pid=fork_or_panic("fork SUBS");
            if (pid == 0)
            {
                run_cmd(scmd->cmd);
		
                exit(EXIT_SUCCESS);
            }
            TRY( waitpid(pid,NULL,0) );
            break;

        case INV:
        default:
            panic("%s: estructura `cmd` desconocida\n", __func__);
    }

    DPRINTF(DBG_TRACE, "END\n");
}


void print_cmd(struct cmd* cmd)
{
    struct execcmd* ecmd;
    struct redrcmd* rcmd;
    struct listcmd* lcmd;
    struct pipecmd* pcmd;
    struct backcmd* bcmd;
    struct subscmd* scmd;

    if(cmd == 0) return;

    switch(cmd->type)
    {
        default:
            panic("%s: estructura `cmd` desconocida\n", __func__);

        case EXEC:
            ecmd = (struct execcmd*) cmd;
            if (ecmd->argv[0] != 0)
                printf("fork( exec( %s ) )", ecmd->argv[0]);
            break;

        case REDR:
            rcmd = (struct redrcmd*) cmd;
            printf("fork( ");
            if (rcmd->cmd->type == EXEC)
                printf("exec ( %s )", ((struct execcmd*) rcmd->cmd)->argv[0]);
            else
                print_cmd(rcmd->cmd);
            printf(" )");
            break;

        case LIST:
            lcmd = (struct listcmd*) cmd;
            print_cmd(lcmd->left);
            printf(" ; ");
            print_cmd(lcmd->right);
            break;

        case PIPE:
            pcmd = (struct pipecmd*) cmd;
            printf("fork( ");
            if (pcmd->left->type == EXEC)
                printf("exec ( %s )", ((struct execcmd*) pcmd->left)->argv[0]);
            else
                print_cmd(pcmd->left);
            printf(" ) => fork( ");
            if (pcmd->right->type == EXEC)
                printf("exec ( %s )", ((struct execcmd*) pcmd->right)->argv[0]);
            else
                print_cmd(pcmd->right);
            printf(" )");
            break;

        case BACK:
            bcmd = (struct backcmd*) cmd;
            printf("fork( ");
            if (bcmd->cmd->type == EXEC)
                printf("exec ( %s )", ((struct execcmd*) bcmd->cmd)->argv[0]);
            else
                print_cmd(bcmd->cmd);
            printf(" )");
            break;

        case SUBS:
            scmd = (struct subscmd*) cmd;
            printf("fork( ");
            print_cmd(scmd->cmd);
            printf(" )");
            break;
    }
}


void free_cmd(struct cmd* cmd)
{
    struct execcmd* ecmd;
    struct redrcmd* rcmd;
    struct listcmd* lcmd;
    struct pipecmd* pcmd;
    struct backcmd* bcmd;
    struct subscmd* scmd;

    if(cmd == 0) return;

    switch(cmd->type)
    {
        case EXEC:
	    free(cmd);
            break;

        case REDR:
            rcmd = (struct redrcmd*) cmd;
            free_cmd(rcmd->cmd);

            free(rcmd);
            break;

        case LIST:
            lcmd = (struct listcmd*) cmd;

            free_cmd(lcmd->left);
            free_cmd(lcmd->right);

            free(lcmd);
            
            break;

        case PIPE:
            pcmd = (struct pipecmd*) cmd;

            free_cmd(pcmd->left);
            free_cmd(pcmd->right);

            free(pcmd);
          
            break;

        case BACK:
            bcmd = (struct backcmd*) cmd;

            free_cmd(bcmd->cmd);

            free(bcmd);
            break;

        case SUBS:
            scmd = (struct subscmd*) cmd;

            free_cmd(scmd->cmd);

            free(scmd);
            break;

        case INV:
        default:
            panic("%s: estructura `cmd` desconocida\n", __func__);
    }
}

/******************************************************************************
 * Lectura de la línea de órdenes con la biblioteca libreadline
 ******************************************************************************/


// `get_cmd` muestra un *prompt* y lee lo que el usuario escribe usando la
// biblioteca readline. Ésta permite mantener el historial, utilizar las flechas
// para acceder a las órdenes previas del historial, búsquedas de órdenes, etc.

char* get_cmd()
{
    char* buf;
    char ruta[PATH_MAX];
    uid_t uid=getuid();
    struct passwd* pw=getpwuid(uid);
    if(!pw){
	perror("getpwuid");
	exit(EXIT_FAILURE);
    }
    
    if(!getcwd(ruta,PATH_MAX)){
	perror("getcwd");
	exit(EXIT_FAILURE);
    }
    
    /*ADD mostrar usuario@directorio> */
    char *directorio=basename(ruta);
    int tama=strlen(pw->pw_name)+strlen("@")+strlen(directorio)+strlen("> ");
    char nombreRuta[tama]; //Lo que se mostrara por el prompt
    sprintf(nombreRuta,"%s@%s> ",pw->pw_name,directorio);
    // Lee la orden tecleada por el usuario
    buf = readline(nombreRuta);

    // Si el usuario ha escrito una orden, almacenarla en la historia.
    if(buf)
        add_history(buf);

    return buf;
}

/******************************************************************************
 * Bucle principal de `simplesh`
 ******************************************************************************/


void help(int argc, char **argv)
{
    info("Usage: %s [-d N] [-h]\n\
         shell simplesh v%s\n\
         Options: \n\
         -d set debug level to N\n\
         -h help\n\n",
         argv[0], VERSION);
}


void parse_args(int argc, char** argv)
{
    int option;

    // Bucle de procesamiento de parámetros
    while((option = getopt(argc, argv, "d:h")) != -1) {
        switch(option) {
            case 'd':
                g_dbg_level = atoi(optarg);
                break;
            case 'h':
            default:
                help(argc, argv);
                exit(EXIT_SUCCESS);
                break;
        }
    }
}
int run_src(char** comando, int num_argc){
	int opt, flag, n;
	int valor=num_argc;
	int fd=STDIN_FILENO;
	flag = n = 0;
	 optind = 1;
	char delim='%';
	 while ((opt = getopt(num_argc, comando, "d:h")) != -1) {
		switch (opt) {
		    case 'd':
			if(strlen(optarg)>1){
				printf("src: Opción no válida\n");
				return 0;
			}
			delim=*optarg;
		   	break;
		    case 'h':
		        printf("Uso: src [-d DELIM] [FILE1] [FILE2 ]...\n\tOpciones:\n\t-d DELIM Carácter de  inicio  de  comentarios.\n\t-h help\n");
			
			return 0;
		       
		}

	    }
	int num_ficheros=num_argc-optind;
	do {

		if(num_ficheros!=0)
			fd=open(comando[optind],O_RDONLY);
		int bytes_leidos=1;
		while(bytes_leidos!=0){
			char* buf=malloc(sizeof(char)*MAX_LINEA);

			bytes_leidos=read(fd,buf,MAX_LINEA);
			 if(bytes_leidos==-1){
                         	printf("src: No se encontró el archivo '%s'\n",comando[optind]);
                                return 0;
                         }
			int i=0;
			char* comando=malloc(sizeof(char)*MAX_LINEA);
		 	while(i<bytes_leidos && i<MAX_LINEA && buf[i]!='\0'){
				if(buf[i]=='\n'){
					
					if(buf[0]!=delim){
						strncpy(comando,buf,i);
						struct cmd* cmd;
    						DPRINTF(DBG_TRACE, "STR\n");
   					      
        					cmd = parse_cmd(comando);
	      	
       						null_terminate(cmd);
			
        					DBLOCK(DBG_CMD, {
            					info("%s:%d:%s: print_cmd: ",
                	 			__FILE__, __LINE__, __func__);
        	    				print_cmd(cmd); printf("\n"); fflush(NULL); } );
        
       		 				run_cmd(cmd);        
       						free_cmd(cmd);
						strcpy(comando,"\0");
						memmove(buf,buf+(i+1),strlen(buf));
			
						bytes_leidos-=i;
						i=0;
			
				
					}
					else{
						memmove(buf,buf+(i+1),strlen(buf));
                                                bytes_leidos-=i;
						i=0;

					}
				}
				else
					i++;
			if(buf[i]=='\0'){
				bytes_leidos=read(fd,buf+i,MAX_LINEA-i);
				if(bytes_leidos==0){
						struct cmd* cmd;
                                                DPRINTF(DBG_TRACE, "STR\n");

                                                cmd = parse_cmd(buf);

                                                null_terminate(cmd);

                                                DBLOCK(DBG_CMD, {
                                                info("%s:%d:%s: print_cmd: ",
                                                __FILE__, __LINE__, __func__);
                                                print_cmd(cmd); printf("\n"); fflush(NULL); } );

                                                run_cmd(cmd);
                                                free_cmd(cmd);

				}		
			}
			}		
		    free(comando);						
		}
		if(num_ficheros!=0)
			close(fd);
		
		
		optind++;	
	} while(optind<valor);
	return 0;
}

int main(int argc, char** argv)
{
	procesosback=proback();
    	sigset_t  blocked_signals;
	
	sigemptyset (& blocked_signals);
	
	sigaddset (& blocked_signals , SIGINT);
	
	
	if (sigprocmask(SIG_BLOCK , &blocked_signals , NULL) ==  -1) {
		perror("sigprocmask");

		exit(EXIT_FAILURE);
	}
	struct  sigaction  sa;
	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sigemptyset (&sa.sa_mask);
	if (sigaction(SIGQUIT , &sa , NULL) ==  -1) {
		perror("sigprocmask");

		exit(EXIT_FAILURE);
	}
	
	struct sigaction controlador;
	controlador.sa_handler = &handle_sigchld;
	sigemptyset(&controlador.sa_mask);
	controlador.sa_flags = SA_RESTART | SA_NOCLDSTOP;
	if (sigaction(SIGCHLD, &controlador, 0) == -1) {
  		perror("sigaction");
  		exit(EXIT_FAILURE);
	}

    unsetenv("OLDPWD");
    char* buf;
    struct cmd* cmd;
    parse_args(argc, argv);
    
    DPRINTF(DBG_TRACE, "STR\n");

    // Bucle de lectura y ejecución de órdenes
    while ((buf = get_cmd()) != NULL)
    {
        // Realiza el análisis sintáctico de la línea de órdenes
        cmd = parse_cmd(buf);
	
        // Termina en `NULL` todas las cadenas de las estructuras `cmd`
        null_terminate(cmd);

        DBLOCK(DBG_CMD, {
            info("%s:%d:%s: print_cmd: ",
                 __FILE__, __LINE__, __func__);
            print_cmd(cmd); printf("\n"); fflush(NULL); } );

        // Ejecuta la línea de órdenes
        run_cmd(cmd);

        // Libera la memoria de las estructuras `cmd`
        free_cmd(cmd);
	

        // Libera la memoria de la línea de órdenes
        free(buf);

    }

    
    DPRINTF(DBG_TRACE, "END\n");
    
    return 0;
}
