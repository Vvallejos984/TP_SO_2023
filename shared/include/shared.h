#ifndef SHARED_H
#define SHARED_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <commons/string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <ctype.h>
#include <dirent.h> 

#include <semaphore.h>

#define MAX_BUFFER_SIZE 1024
#define SIZE_ENTRADA_TABLA_SEGM 3 * sizeof(int)

/*Enum para poder identificar lo que se solicita entre kernel y FS  */
typedef enum e_codigo_paquete_fs
{
    REQ_ABRIR_ARCHIVO,    // K a FS
    OK_ABRIR_ARCHIVO,     // FS a K
    ERROR_ABRIR_ARCHIVO,  // FS a K
    REQ_CREAR_ARCHIVO,    // K a FS
    OK_CREAR_ARCHIVO,     // FS a K
    REQ_LEER_ARCHIVO,     // K a FS
    RES_LEER_ARCHIVO,     // en este mensaje hay que mandarle a memoria un void* de lo que pidio y a kernel el ok
    OK_LEER_ARCHIVO,      // el ok a kernel
    REQ_ESCRIBIR_ARCHIVO, // K a FS
    OK_ESCRIBIR_ARCHIVO,  // FS a K  el ok de que se escribio bien el archivo
    REQ_TRUNCAR_ARCHIVO,   // K a FS
    REQ_OP_MEMORIA,
    OK_OP_MEMORIA,
    OK_TRUNCAR,
    ERROR_GENERICO
} e_codigo_paquete_fs;

typedef struct data_segmento
{
    uint32_t id;
    uint32_t dir_base;
    uint32_t size;
} data_segmento;

typedef struct tablaSegmMemoria
{
    int pid;
    t_list *tabla;
} tablaSegmMemoria;

typedef struct t_registros_cpu
{

    char ax[4];
    char bx[4];
    char cx[4];
    char dx[4];

    char eax[8];
    char ebx[8];
    char ecx[8];
    char edx[8];

    char rax[16];
    char rbx[16];
    char rcx[16];
    char rdx[16];

} t_registros_cpu;

typedef enum idProtocolo
{
    P_CONSOLA,
    P_KERNEL,
    P_CPU,
    P_MEMORIA,
    P_FILESYSTEM
} idProtocolo;

typedef struct instrucciones
{
    uint32_t cantParam;
    char *comando;
    char *parametro1;
    char *parametro2;
    char *parametro3;
} INSTRUCCIONES;

//Rename de t_config para usar en los archivos
//typedef t_config t_fcb;

/*Enum para poder identificar lo que se solicita entre la cpu
y el Kernel  */
typedef enum e_codigo_cpu_kernel
{
    REQ_OPEN_FILE,
    REQ_CLOSE_FILE,
    REQ_SEEK_FILE,
    REQ_READ_FILE,
    REQ_WRITE_FILE,
    REQ_TRUNCATE_FILE,
    REQ_MOV_IN,
    REQ_MOV_OUT,
    REQ_WAIT,
    REQ_SIGNAL,
    REQ_I_O,
    REQ_YIELD,
    REQ_EXIT,
    REQ_CREATE_SEGMENT,
    REQ_DELETE_SEGMENT,
    ERR_FILE_OPENED,
    ERR_FILE_NOT_OPEN,
    ERR_SEGMENTATION_FAULT,
    ERR_INVALID_REG,
    ERR_INVALID_SEGMENT,
    ERR_READ_MEMORY,
    ERR_WRITE_MEMORY,
    NUEVO_PCB
} e_codigo_cpu_kernel;

typedef struct entradaTablaSegmentos
{
    uint32_t idSegmento;
    uint32_t dirBase;
    uint32_t size;
} entradaTablaSegmentos;

typedef enum pcbEstado
{
    S_NEW,
    S_READY,
    S_RUNNING,
    S_BLOCKED,
    S_EXIT
} pcbEstado;

typedef enum mensajeKernel
{
    READY_PCB,
    WAIT,
    SIGNAL,
    EXEC_PCB,
    FINALIZED_PCB,
    BLOCKED_PCB
} mensajeKernel;

typedef struct t_file_pcb
{
    char *filename;
    int pointer;
} t_file_pcb;

typedef struct t_file_kernel
{
    char *filename;
    bool open;
    t_queue *pcbs;
} t_file_kernel;

/*Estructura para un PCB alojado en el módulo Kernel*/
typedef struct t_pcb_kernel
{
    uint32_t pid;               // Process ID único para cada proceso
    t_list *instrucciones;      // List de "INSTRUCCIONES"
    uint32_t program_counter;   // Program counter
    t_registros_cpu *registros; // Registros del CPU (De 4, 8 y 16 bytes)
    t_list *tabla_segmentos;    // List de "entradaTablaSegmentos"
    double est_prox_rafaga;        // Estimador para HRRN (No va al CPU)
    time_t llegada_ready;       // Timestamp de llegada a ready (No va al CPU)
    t_list *archivos_abiertos;  // Lista de t_file_pcb
    pcbEstado estado;           // Estado actual (Ready, Running, Blocked) (No va al CPU)
} t_pcb_kernel;

/*Estructura para un PCB alojado en el módulo CPU
(Aka. Contexto de ejecución)*/
typedef struct t_pcb_cpu
{
    uint32_t pid;               // Process ID único para cada proceso
    t_list *instrucciones;      // List de "INSTRUCCIONES"
    uint32_t program_counter;   // Program counter
    t_registros_cpu *registros; // Registros del CPU (De 4, 8 y 16 bytes)
    t_list *tabla_segmentos;    // List de "entradaTablaSegmentos"
    char *archivos_abiertos;  // ¡¡A definir como se va a implementar esta tabla!!
} t_pcb_cpu;

typedef struct dataHiloConexion
{
    void (*funcion)(void *);
    int socket;
} dataHiloConexion;

/*Enum identificador de registros_cpu.
(Para referenciarlos fácilmente con un diccionario).*/
typedef enum idRegistro
{
    //   A    B    C    D
    AX,
    BX,
    CX,
    DX, // Registro de 4 Bytes
    EAX,
    EBX,
    ECX,
    EDX, // Registro de 8 Bytes
    RAX,
    RBX,
    RCX,
    RDX // Registro de 16 Bytes
} idRegistro;

/*Enum para poder identificar lo que se solicita entre la cpu
y el modulo de Memoria*/

typedef enum
{
    REQ_LECTURA,
    REQ_ESCRITURA,
    OK_LECTURA,
    ERR_LECTURA,
    OK_ESCRITURA,
    ERR_ESCRITURA
} e_cod_cpu_mem;

/*
typedef enum {
    REQ_LECTURA_FS,
    REQ_ESCRITURA_FS,
    OK_LECTURA_FS,
    ERR_LECTURA_FS,
    OK_ESCRITURA_FS,
    ERR_ESCRITURA_FS
}e_cod_fs_mem;
*/
typedef e_cod_cpu_mem e_cod_fs_mem;

typedef enum{
    MOD_CPU,
    MOD_FS
}e_cod_esc_lect;

typedef enum codigoKernelMemoria{
    REQ_INICIAR_PROCESO,
    OK_INICIAR_PROCESO,
    REQ_FIN_PROCESO,
    REQ_CREAR_SEGMENTO,
    OK_CREAR_SEGMENTO,
    REQ_ELIMINAR_SEGMENTO,
    OK_ELIMINAR_SEGMENTO,
    REQ_PEDIR_COMPACTACION, //Lo manda la Memoria (Pide permiso)
    OK_COMPACTAR, //Lo manda el Kernel (Confirma)
    ENVIAR_TABLA_SEGM, //Ante OK de Iniciar proceso y Crear o Eliminar segmento
    OK_FIN_PROCESO,
    ERROR_FIN_PROCESO,
    ERROR_CREAR_SEGMENTO,
    ERROR_CONFIGURACION,
    ERROR_ELIMINAR_SEGMENTO,
    OK_COMPACTACION
} codigoKernelMemoria;

int testShared();
int crear_hilo_conexion(dataHiloConexion *data);
char *buscarEnConfig(t_config *, char *);
uint32_t textoAuint32(char *);
double textoAdouble(char *);
t_list *listarTexto(char *);
void showList(t_list *);

void leerInstr(INSTRUCCIONES *instr);
void leerLista(t_list *lista);

/*Crea una instrucción con todos los char* en NULL y la cantidad en -1
(Evita que haya basura antes de usar la instrucción)*/
INSTRUCCIONES *new_null_instr();

/*Serializa la instrucción 'instr' y aloja el tamaño del buffer en 'fullsize'.
Retorna el buffer serializado.*/
void *serializarInstruccion(INSTRUCCIONES *instr, int *fullsize);

/*Serializa la lista de instrucciones 'instrucciones' y aloja el tamaño del buffer en 'fullsize'.
 *Retorna el buffer serializado.*/
void *serializarListaInstrucciones(t_list *instrucciones, int *fullsize);
void* serializar_int_e_int(int codigo, int entero_a, int entero_b, int* fullsize);
void* serializar_char_e_int(int codigo, int entero, char* registro, int* fullsize);
void* serializar_char(int codigo, char* string, int* fullsize);

/*Deserializa una instrucción trayéndola del 'socket'.
Retorna el struct de instrucción.*/
INSTRUCCIONES *deserializarInstruccion(int socket);

/*Deserializa una lista de instrucciones trayendola del 'socket'
Retorna la lista con todos los structs instrucción*/
t_list *deserializarListaInstrucciones(int socket);

/*Deserializa un PCB trayendolo del 'socket'
Retorna el PCB en formato t_pcb_cpu*/
t_pcb_cpu *deserializarPCB(int socket);

void *serializarPCB(t_pcb_cpu *pcb, int *fullsize);

int deserializar_int_e_int(int socket, int* entero_a, int* entero_b);
int deserializar_char_e_int(int socket, int* entero_a, char* string);
int deserializar_char(int socket, char* string);

/*Crea un t_pcb_cpu a partir de un t_pcb_kernel.
Útil para unificar los envios entre Kernel y CPU.*/
t_pcb_cpu *new_cpu_pcb(t_pcb_kernel *base);

/*Crea una nueva entrada de tabla de segmentos por sus parámetros*/
entradaTablaSegmentos *new_entrada_ts(uint32_t idSegmento, uint32_t dirBase, uint32_t size);

/*Actualiza el contenido de 'pcb' a partir del de 'base'
(Ejecutar luego de la deserialización de PCB en Kernel)*/
void actualizarContextoEjec(t_pcb_cpu *base, t_pcb_kernel *pcb);

t_list *deserializarTablaSegmentos(int socket);

void* serializarDataArchivo(int codigo, int pid, int dir_fisica, int puntero, int tamanio_lectura_escritura, char* nombre_archivo, int* fullsize);
void* serializar_3_int(int codigo, int entero_a, int entero_b, int entero_c,  int* fullsize);
void *serializar_int_e_int(int codigo, int entero_a, int entero_b, int *fullsize);
void *serializar_int(int codigo, int entero_a, int *fullsize);
void *serializar_char_e_int(int codigo, int entero, char *registro, int *fullsize);
void* serializar_char_y_2_int(int codigo, int entero_a, int entero_b, char* string,int tamanio_string ,int* fullsize);
void *serializar_char(int codigo, char *string, int *fullsize);
void *serializar_codigo(int codigo, int *fullsize);

void* serializarTablaSegmentos(t_list* tabla, int* fullsize);

idProtocolo handshake_servidor_multiple(int socket, idProtocolo idProt1, idProtocolo idProt2, idProtocolo idProt3);

dataHiloConexion *new_dataHiloConexion(void *funcion(void *), int socket);

void milisleep(uint32_t retardo);

int enviar_y_recibir_codigo(int socket, void* buffer, int tamanio);

char* listaPCBATexto(t_list* lista);

#endif