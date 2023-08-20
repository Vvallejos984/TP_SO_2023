#ifndef SRC_KERNEL_H_
#define SRC_KERNEL_H_
#include <stdio.h>
#include <stdlib.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <commons/collections/queue.h>
#include "cliente.h"
#include "servidor.h"
#include "shared.h"

typedef struct t_kernel_config
{
    char *ip_memoria;
    char *puerto_memoria;
    char *ip_filesystem;
    char *puerto_filesystem;
    char *ip_cpu;
    char *puerto_cpu;
    char *puerto_escucha;
    char *planificador;
    uint32_t estimacion_inicial;
    float hrrn_alfa;
    uint32_t grado_multiprogramacion;
    char *recursos;
    char *instancias_recursos;
} t_kernel_config;

typedef struct t_recurso
{
    char *recurso;
    int instancias;
    t_queue *colaBloqueados;
} t_recurso;

t_log *logger;
t_queue *cola;

void showLists();

void conexionMemoria();
void conexionFileSystem();
void conexionCPU();
void handlerConexionesEntrantes(int servidor_kernel);
void *handlerIO(void *elemento);
t_pcb_kernel *crearPcb(t_list *instrucciones);
void planificacionLargoPlazo();
void planificacionFIFO();
void planificacionHRRN();
void handlerConsola(int socketConsola);
int determinarRecursos();
void* comparar(t_pcb_kernel *pcb1, t_pcb_kernel *pcb2);
bool buscarRecurso(void *recurso);
bool buscarFile(void *elemento);
t_pcb_kernel *buscarPcbxPid(t_list *lista, uint32_t pidBuscado);
void finalizarPcb(t_pcb_kernel *pcb);
void actualizarTablasDeSegmentos(t_list *tablaGlobal);
void actualizarListaPcb(t_list *tablaGlobal, t_list *listaPcb);
void actualizarProximaRafaga(t_pcb_kernel *pcb);
void desbloquear_pcb(int pid);

t_pcb_cpu *deserializar_request_cpu_kenel(void *request, int socket);
t_pcb_cpu *deserializar_contexto_y_char(int socket);
t_pcb_cpu *deserializar_contexto_e_int(int socket);
t_pcb_cpu *deserializar_contexto_char_e_int(int socket, char *string, int *entero);
t_pcb_cpu *deserializar_contexto_char_y_2_int(int socket, char *string, int *entero_a, int *entero_b);
t_pcb_cpu *deserializar_contexto_int_e_int(int socket, int *entero_a, int *entero_b);

t_kernel_config *inicializar_config(char *path);
void atender_consola(int socket);
INSTRUCCIONES *deserializarInstruccion(int socket);
t_list *deserializarListaInstrucciones(int socket);

void conexionMemoria();
void conexionFileSystem();
void conexionCPU();
t_pcb_kernel *crearPcb(t_list *instrucciones);
void leerPCB(t_pcb_kernel *pcb);
void planificacionFIFO();
void planificacionHRRN();

#endif
