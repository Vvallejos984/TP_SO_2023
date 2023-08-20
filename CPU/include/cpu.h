#ifndef CPU_H
#define CPU_H
#include <stdio.h>
#include <commons/log.h>
#include <commons/config.h>
#include "cliente.h"
#include "servidor.h"
#include "shared.h"

typedef struct t_cpu_config{
    char* ip_memoria;
    char* puerto_memoria;
    char* puerto_escucha;
    uint32_t tam_max_segmento;
    uint32_t retardo_instruccion;
}t_cpu_config;

t_cpu_config* inicializar_config(char*);
t_dictionary* inicializar_diccionario();
t_dictionary* init_dict_registros();
entradaTablaSegmentos* buscarSegmentoPorID(t_list*, int);
int enviar_pcb(void* buffer, int tamanio);
void* serializar_contexto_e_int(char* entero, e_codigo_cpu_kernel codigo, int* tam_total);
void* serializar_contexto_y_char(char* string, e_codigo_cpu_kernel codigo, int* tam_total);
void* serializar_contexto(e_codigo_cpu_kernel codigo, int* tam_total);
void* serializar_contexto_char_e_int(char* string, char* entero, e_codigo_cpu_kernel codigo, int* tam_total);
void* serializar_contexto_char_y_2_int(char* string, int entero_a, char* entero_b, e_codigo_cpu_kernel codigo, int* tam_total);
void* serializar_contexto_int_e_int(char* entero_a, char* entero_b, e_codigo_cpu_kernel codigo, int* tam_total);


int ciclo_ejec();


void log_error_segmentation_fault(int segmento, int desplazamiento, int tamanio);
void log_acceso_memoria(char* accion, int segmento, int dir_fisica, char* valor);

//Funciones de deserializas (Se van a los respectivos modulos)
//Kernel
t_pcb_cpu* deserializar_request_cpu_kenel(int socket);
t_pcb_cpu* deserializar_contexto_y_char(int socket, char* string);
t_pcb_cpu* deserializar_contexto_e_int(int socket, int* entero);
t_pcb_cpu* deserializar_contexto_char_e_int(int socket, char* string, int* entero);
t_pcb_cpu* deserializar_contexto_char_y_2_int(int socket, char* string, int* entero_a, int* entero_b);
t_pcb_cpu* deserializar_contexto_int_e_int(int socket, int* entero_a, int* entero_b);

//Mem
int deserializar_request_cpu_mem(int socket);

int mmu(int dir_logica, entradaTablaSegmentos* entrada);

void conexionKernel(int socket_servidor);
void conexionMemoria();

#endif