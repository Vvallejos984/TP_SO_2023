#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include <stdio.h>
#include <commons/log.h>
#include <commons/config.h>
#include <stdlib.h>
#include "cliente.h"
#include "servidor.h"
#include "shared.h"

typedef struct t_filesystem_config{
    char* ip_memoria;
    char* puerto_memoria;
    char* puerto_escucha;
    char* path_superbloque;
    char* path_bitmap;
    char* path_bloques;
    char* path_FCB;
    uint32_t retardo_acceso_bloque;
}t_filesystem_config;

typedef struct t_inodo{
    uint32_t puntero_directo;
    uint32_t puntero_indirecto;
} t_inodo;

typedef t_config t_fcb;

typedef struct t_superbloque {
	uint32_t block_size;
	uint32_t block_count;
} t_superbloque;



t_filesystem_config* inicializar_config(char*);
t_fcb* leer_file_fcb(char* path);

char* normalizar_path(char* nombre_archivo);
int inicializar_dict_fcb();
t_fcb* leer_file_fcb(char* path);
int actualizar_tam_archivo(t_fcb* fcb, uint32_t nuevo_tam);
int abrirArchivo(char* nombre_archivo);
int truncarArchivo(char* nombre_archivo, uint32_t nuevo_tamanio);
int crearArchivo(char * nombre_archivo_fcb);
char* leerArchDesdePunteroIndirecto(int puntero, int tamanio_a_leer, int puntero_indirecto, char* nombreArchivo);
char* leerArchDesdePunteroDirecto(int puntero, int tamanio_a_leer, int puntero_directo, int puntero_indirecto, char* nombreArchivo);
char* leerEnMemoria(int direccion_fisica, int tamanio, int pid);
int escribirEnMemoria(char* data, int tamanio, int dir_fisica, int pid);
int escribirArchDesdePunteroDirecto(int puntero, int tamanio_a_escribir, int puntero_directo, int puntero_indirecto, char* bytes_a_escribir, char* nombre_archivo, int primer_bloque_a_escribir);
int escribirArchDesdePunteroIndirecto(int puntero, int tamanio_a_escribir, int puntero_indirecto, char* bytes_a_escribir, char* nombre_archivo, int primer_bloque_a_escribir);
int escribirArchivo(int puntero, int tamanio_a_escribir, char* nombre_archivo, int direccion_fisica, int pid);
int leerArchivo(int puntero, int tamanio_a_leer, char* nombre_archivo, int direccion_fisica, int pid);
int ocuparBloque(int bloque);
int desocuparBloque(int bloque);
int buscarBloqueLibre();
int escribirEnBloqueDeDatos(char *data, int puntero, int tamanio, char *nombre_archivo, int bloque_archivo);
char* leerUnBloqueDeDatos(int puntero);
int escribirEnBloqueDePunteros(int indice_del_bloque_de_punteros, int valor_puntero, int bloque_a_escribir);
int* leerEnBloqueDePunteros(int puntero);
int truncarMasBloques(char* nombre_archivo, int cant_bloques, int nuevoTamanio);
int truncarMenosBloques(char* nombre_archivo, int cant_bloques_a_borrar, int nuevo_tamanio, int cant_bloques_actual);
int levantar_archivo_de_bloques();
t_superbloque* inicializar_superbloque();
void atenderReqKernel();
void deserializarDataArchivo(int socket, int* dir_fisica, int* puntero, int* tamanio_lectura_escritura, int* pid, char* nombre_archivo);


char* mock_memoria(int data);
#endif
