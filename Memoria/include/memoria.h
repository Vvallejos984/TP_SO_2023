#ifndef MEMORIA_H
#define MEMORIA_H
#include <stdio.h>
#include <commons/log.h>
#include <commons/config.h>
#include "cliente.h"
#include "servidor.h"
#include "shared.h"


//logger
extern t_log* logger_memoria;

//bloques de memoria
extern void* memoria;

typedef struct espacioMemoria{
    int base;
    int size;
}espacioMemoria;


typedef struct entradaGlSegmMemoria{
    int pid;
    entradaTablaSegmentos* entrada;
}entradaGlSegmMemoria;

typedef struct t_memoria_config{
    char* puerto_escucha;
    uint32_t tam_memoria;
    uint32_t tam_segmento_0;
    uint32_t cant_segmentos;
    uint32_t retardo_memoria;
    uint32_t retardo_compactacion;
    char* algoritmo_asignacion;
    char* ip_memoria; 
}t_memoria_config;

typedef struct {
    uint32_t size;
    void *stream;
} t_buffer;

//Estructuras
typedef enum {
    CODIGO_INDEFINIDO,
	NUEVO_CLIENTE,
    CLIENTE_DESCONECTADO,
    
    //MEMORIA
    INICIALIZAR_PROCESO,
    SUSPENDER_PROCESO,
    ACCESO_TABLA_PAGINA_NIVEL_1,
    ACCESO_TABLA_PAGINA_NIVEL_2,
    ACCESO_TABLA_CONTENIDO,
    LECTURA_DATO,
    ESCRITURA_DATO,
    HANDSHAKE,
    COPIAR_DATO,
    ESCRITURA_REALIZADA,
    LECTURA_REALIZADA,
    FINALIZAR_PROCESO,

    //KERNEL
    RETORNO_BLOCK,
    RETORNO_INTERRUPT,
    RETORNO_EXIT,
    INTERRUPT,
    PCB_RECIBIDO,
    DISPATCH_PCB
} op_code;

typedef struct {
    op_code codigo_operacion;
    t_buffer *buffer;
} t_paquete;

typedef struct {
   int proceso;
   t_list* segmentos;
} t_tabla_segmentos;

typedef struct {
    int base;
    int limit;
} t_segmento;

t_list* tablasDeSegmentos; // t_tabla_segmentos[]

//Funciones de inicialización

t_memoria_config* inicializar_config(char*);
void iniciar_logger_memoria();

void levantar_servidor();
void espera_de_clientes();
void *ejecutar_operacion_cliente(int *socket_client_p);
char* traducirOperacion(int operacion);
t_tabla_segmentos* CrearTablaSegmentos(int* proceso);

void updateEspacios();
void leerEspacios(t_list* lista);
espacioMemoria* newEspacioMem(int base, int size);
int new_tabla_segmentos(int socket);

void actualizarTSGlobal();
void plasmarTablaGlobal();

int buscarEspacioViableWorst(int size);
int buscarEspacioViableBest(int size);
int buscarEspacioViableFirst(int size);

//Gestión de errores de los buscadores de espacios viables
int recalcularEspacioViable(int size);

void* serializarTablas(int* tamanio);

void* serializar_contenido_leido(char* contenido, int tamanio);

#endif