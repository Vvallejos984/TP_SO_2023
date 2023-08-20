#ifndef CONSOLA_H
#define CONSOLA_H
#include <stdio.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include "cliente.h"
#include "servidor.h"
#include "shared.h"

#define TAM_BUFFER_CONSOLA 500
#define CANT_PARAM_PSEUDO_LINE 4

typedef struct t_consola_config{
    char* ip_kernel;
    char* puerto_kernel;
}t_consola_config;

void leerLista(t_list* lista);
t_list* parseo_archivo(char* path);
t_consola_config* inicializar_config(char* path);
t_list* leer_archivo_pseudocodigo(char *path);
void esperar_fin(int socket_cliente);
void enviarInstrucciones(t_list* instrucciones, int socket);
void* serializarInstruccion(INSTRUCCIONES* instr, int* fullsize);
void* serializarListaInstrucciones(t_list* instrucciones, int* fullsize);

#endif
