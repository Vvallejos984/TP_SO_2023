#include "filesystem.h"

#include <commons/log.h>
#include <commons/config.h>
#include <commons/bitarray.h>

#include <commons/collections/list.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <math.h>
#include <errno.h> 

t_log* logger;
t_dictionary* dict_fcbs;
t_filesystem_config* config;
t_superbloque* superbloque;
t_bitarray* bitmap;
FILE* archivo_de_bloques;
int tam_max_archivo;
int bloques_libres;
sem_t semaforo;

//Solo para compilar
int socket_memoria;
int socket_kernel;


char* normalizar_path(char* nombre_archivo){
    char* path_arch = malloc(strlen(config-> path_FCB) + strlen(nombre_archivo) + 1); //+1 para el /
    log_debug(logger, "LEVANTO FILE: '%s'", nombre_archivo);
    strcpy(path_arch, config-> path_FCB);
    strcat(path_arch, "/");
    strcat(path_arch, nombre_archivo);
    return path_arch;
}

void inicializar_puntero_indirecto(char* nombre_archivo, t_fcb* fcb){
    int puntero_indirecto = config_get_int_value(fcb, "PUNTERO_INDIRECTO");
    int tamanio_archivo = config_get_int_value(fcb, "TAMANIO_ARCHIVO");
    int cant_bloques_completo = tamanio_archivo / superbloque -> block_size - 1;
    int bytes_restantes = tamanio_archivo % superbloque -> block_size;
    if (bytes_restantes > 0)
        cant_bloques_completo += 1;
    for (int i = 0; i < cant_bloques_completo; i++){
        int bloque_libre = buscarBloqueLibre();
        ocuparBloque(bloque_libre);
        escribirEnBloqueDePunteros(i, bloque_libre, puntero_indirecto);
        
    }
}

int inicializar_fcb(t_fcb* fcb){
    int puntero_indirecto = config_get_int_value(fcb, "PUNTERO_INDIRECTO");
    int puntero_directo = config_get_int_value(fcb, "PUNTERO_DIRECTO");
    int tamanio_archivo = config_get_int_value(fcb, "TAMANIO_ARCHIVO");
    return 0;
}  

// Podría haber archivos que tengan un tamanio mayor al del maximo seteado por config?
int inicializar_dict_fcb(){
    dict_fcbs = dictionary_create();
    DIR * directory = opendir(config -> path_FCB); // open the path
    if(directory == NULL) 
        return 1;
    struct dirent* dir_entry;
    //log_debug(logger, "PRE WHILE");
    while((dir_entry = readdir(directory)) != NULL){
        if(dir_entry->d_type==DT_REG){
            t_fcb* fcb = leer_file_fcb(normalizar_path(dir_entry->d_name));
            dictionary_put(dict_fcbs, buscarEnConfig(fcb, "NOMBRE_ARCHIVO") ,fcb);
            inicializar_fcb(fcb);
        }
    }
    //dictionary_iterator(dict_fcbs, inicializar_puntero_indirecto);
    return 0;
}

t_fcb* leer_file_fcb(char* path){
    t_fcb* fcb = config_create(path);
    return fcb;   
}    


//Hacer la misma funcion para las otras keys
int actualizar_tam_archivo(t_fcb* fcb, uint32_t nuevo_tam){
    //Chequear que config_set_value pise el valor
    config_set_value(fcb, "TAMANIO_ARCHIVO", string_itoa(nuevo_tam)); 
    config_save(fcb);
    return 0;
}


int abrirArchivo(char* nombre_archivo){
    //log_debug(logger, "ENTRO A abrirArchivo");
    bool existe_el_archivo = dictionary_has_key(dict_fcbs, nombre_archivo);
    if(!existe_el_archivo){
        responderAKernel(ERROR_ABRIR_ARCHIVO);
        log_error(logger, "NO EXISTE EL ARCHIVO: %s", nombre_archivo);
        return -1;
    }
    log_info(logger,"Abrir Archivo: %s", nombre_archivo);
    responderAKernel(OK_ABRIR_ARCHIVO);
    return 0;
}


int truncarArchivo(char* nombre_archivo, uint32_t nuevo_tamanio){
    if(nuevo_tamanio > tam_max_archivo){
        log_error(logger, "EL NUEVO TAMANIO ES MAYOR AL TAMANIO MAXIMO DE ARCHIVO");
        responderAKernel(OK_ABRIR_ARCHIVO);
        return -1;
    }
    bool existe_el_archivo = dictionary_has_key(dict_fcbs, nombre_archivo);
    if(!existe_el_archivo){
        responderAKernel(ERROR_GENERICO);
        log_error(logger, "NO EXISTE EL ARCHIVO: %s", nombre_archivo);
        return -1;
    }
    log_info(logger, "Truncar Archivo: %s - Tamaño: %d", nombre_archivo, nuevo_tamanio);
    t_fcb* fcb_file = dictionary_get(dict_fcbs, nombre_archivo);
    int tamanio_viejo = (int) config_get_int_value(fcb_file, "TAMANIO_ARCHIVO");

    int nueva_cantidad_de_bloques;
    if(nuevo_tamanio == 0)
        nueva_cantidad_de_bloques = 0;
    else
        nueva_cantidad_de_bloques = nuevo_tamanio / superbloque -> block_size + 1;
    
    if(nueva_cantidad_de_bloques > bloques_libres){
        responderAKernel(ERROR_GENERICO);
        log_error(logger, "NO HAY SUFICIENTES BLOQUES LIBRES PARA TRUNCAR: %s", nombre_archivo);
        return -1;
    }


    int cant_bloques_actual = tamanio_viejo / superbloque -> block_size + 1;
    //Contemplar el caso que no hay que agregar ni quitar bloques
    if(nuevo_tamanio > tamanio_viejo){
        truncarMasBloques(nombre_archivo, nueva_cantidad_de_bloques - cant_bloques_actual, nuevo_tamanio);
        actualizar_tam_archivo(fcb_file, nuevo_tamanio);
    }
    else if (nuevo_tamanio < tamanio_viejo){
        truncarMenosBloques(nombre_archivo, cant_bloques_actual - nueva_cantidad_de_bloques, nuevo_tamanio, cant_bloques_actual);
        actualizar_tam_archivo(fcb_file, nuevo_tamanio);
    }

    /*log_debug(logger, "%s %s %s", config_get_string_value(fcb_file, "TAMANIO_ARCHIVO"),
                config_get_string_value(fcb_file, "PUNTERO_DIRECTO"),
                config_get_string_value(fcb_file, "PUNTERO_INDIRECTO"));*/
    responderAKernel(OK_TRUNCAR);

    return 0;
}

int crearArchivo(char * nombre_archivo_fcb){
    char* path_arch = normalizar_path(nombre_archivo_fcb);
    FILE* file = fopen(path_arch, "w");
    log_info(logger, "Crear Archivo: %s", nombre_archivo_fcb);
    if(!file){
        log_error(logger, "ERROR CREANDO ARCHIVO DE FCB");
        return 1;
    }
    txt_write_in_file(file, "EMPTY_KEY=VALUE");
    fclose(file);
    t_fcb* fcb_confg = config_create(path_arch);
    config_remove_key(fcb_confg, "EMPTY_KEY");
    config_set_value(fcb_confg, "NOMBRE_ARCHIVO", nombre_archivo_fcb);
    config_set_value(fcb_confg, "TAMANIO_ARCHIVO","0");
    config_set_value(fcb_confg, "PUNTERO_DIRECTO", "-1");
    config_set_value(fcb_confg, "PUNTERO_INDIRECTO", "-1");
    dictionary_put(dict_fcbs, nombre_archivo_fcb, fcb_confg); //Agrego el archivo al dict de FCB's
    config_save(fcb_confg);
    responderAKernel(OK_CREAR_ARCHIVO);

    return 0;
}

int retardoAccesoBloque(){
    //log_debug(logger, "INICIANDO RETARDO ACCESO A BLOQUE (%d ms)", config->retardo_acceso_bloque);
    milisleep(config->retardo_acceso_bloque);
    //log_debug(logger, "FIN DEL RETARDO");
}

char* leerArchDesdePunteroIndirecto(int puntero, int tamanio_a_leer, int puntero_indirecto, char* nombreArchivo){
    int posicion_punt_direct = puntero_indirecto * superbloque -> block_size + puntero;
    int primer_bloque_a_leer = puntero / superbloque -> block_size - 1; // Menos 1 para acomodar el indice (el primer bloque es el puntero directo)
    int bloques_a_leer;
    bloques_a_leer = tamanio_a_leer / superbloque -> block_size;
    int bytes_finales_a_leer = tamanio_a_leer % superbloque -> block_size;
    if (bytes_finales_a_leer > 0)
        bloques_a_leer = bloques_a_leer + 1;

    int* lista_de_punteros_a_bloques = leerEnBloqueDePunteros(puntero_indirecto);
    
    char* data_leida = malloc(tamanio_a_leer);
    for(int i = 0; i < bloques_a_leer; i++){
        char* data_bloque = malloc(sizeof(superbloque -> block_size));
        log_info(logger, "Acceso Bloque - Archivo: %s - Bloque Archivo: (BLOQUE DE PUNTEROS) - Bloque File System %d",
                nombreArchivo, puntero_indirecto);
        data_bloque = leerUnBloqueDeDatos((lista_de_punteros_a_bloques[i])*superbloque -> block_size);
        retardoAccesoBloque();
        strcat(data_leida, data_bloque);
        free(data_bloque);
    }

    return data_leida;

}

char* leerArchDesdePunteroDirecto(int puntero, int tamanio_a_leer, int puntero_directo, int puntero_indirecto, char* nombreArchivo){
    char* data_leida = malloc(tamanio_a_leer);
    int posicion_punt_direct = puntero_directo * superbloque -> block_size + puntero;
    if(superbloque -> block_size - puntero - tamanio_a_leer > 0){
        data_leida = leerUnBloqueDeDatos(posicion_punt_direct);
        log_debug(logger, "RETARDO ACCESO A BLOQUE DE DATOS");
        retardoAccesoBloque();
        log_info(logger, "Acceso Bloque - Archivo: %s - Bloque Archivo: 0 - Bloque File System %d",
                nombreArchivo, puntero_directo);
        return data_leida;
    }
    else{
        int tamanio_a_leer_en_primer_bloque = superbloque -> block_size - puntero;
        char* data_primer_bloque = leerUnBloqueDeDatos(posicion_punt_direct);
        retardoAccesoBloque();
        log_debug(logger, "RETARDO ACCESO A BLOQUE DE DATOS");
        log_info(logger, "Acceso Bloque - Archivo: %s - Bloque Archivo: 0 - Bloque File System %d",
                nombreArchivo, puntero_directo);
        char* data_siguientes_bloques = leerArchDesdePunteroIndirecto(superbloque -> block_size, tamanio_a_leer - tamanio_a_leer_en_primer_bloque, puntero_indirecto, nombreArchivo);
        strcat(data_leida, data_primer_bloque);
        strcat(data_leida, data_siguientes_bloques);
        return data_leida;
    }

}



char* leerEnMemoria(int direccion_fisica, int tamanio, int pid){
    int tam_buffer;
    void* buffer = serializar_3_int(REQ_LECTURA , pid, direccion_fisica, tamanio, &tam_buffer);
    int response = enviar_y_recibir_codigo(socket_memoria, buffer, tam_buffer);
    //log_debug(logger, "Respuesta: %d", response);
    if(response == ERR_LECTURA){
        log_error(logger, "ERROR LEYENDO EN MEMORIA LA DIRECCION %d", direccion_fisica);
        return NULL;
    }
    char* data_leida = malloc(tamanio);
    deserializar_char(socket_memoria, data_leida);
    //log_debug(logger, "DATA LEIDA: %s", data_leida);
    return data_leida;
}




int escribirEnMemoria(char* data, int tamanio, int dir_fisica, int pid){
    int tam_buffer;
    log_debug(logger, "Data: '%s' (%d)", data, tamanio);
    void* buffer = serializar_char_y_2_int(REQ_ESCRITURA , pid, dir_fisica, data, tamanio, &tam_buffer);
    log_info(logger, "TAM BUFFER: %d",tam_buffer);
    int response = enviar_y_recibir_codigo(socket_memoria, buffer, tam_buffer);
    log_debug(logger, "RESPUESTA DE MEMORIA: %d", response);
    if(response == ERR_ESCRITURA){
        log_error(logger, "ERROR ESCRIBIENDO EN MEMORIA LA DIRECCION %d", dir_fisica);
        return -1;
    }
    return response;
}

int escribirArchDesdePunteroDirecto(int puntero, int tamanio_a_escribir, int puntero_directo, int puntero_indirecto, char* bytes_a_escribir, char* nombre_archivo, int primer_bloque_a_escribir){
    int posicion_punt_direct = puntero_directo * superbloque -> block_size + puntero;
    int posicion_a_escribir = superbloque -> block_size - puntero - tamanio_a_escribir;
    log_debug(logger, "escribirArchDesdePunteroDirecto pos_punt_direct: %d pos_a_escribir: %d", posicion_punt_direct, posicion_a_escribir);
    if(posicion_a_escribir > 0){
        log_debug(logger, "ESCRIBIEIDNO AL PRINCIPIO DEL ARCH");
        escribirEnBloqueDeDatos(bytes_a_escribir, posicion_punt_direct, tamanio_a_escribir, nombre_archivo, primer_bloque_a_escribir);
        return 0;
    }
    else{
        log_info(logger, "ELSE escrbirArchDesdePunteroDirecto");
        int tamanio_a_escribir_en_primer_bloque = superbloque -> block_size - puntero;
        log_debug(logger, "SIGUE EN ELSE");
        char* bytes_primer_bloque = string_substring_until(bytes_a_escribir, tamanio_a_escribir_en_primer_bloque);
        char* bytes_siguientes_bloques = string_substring_from(bytes_a_escribir, tamanio_a_escribir_en_primer_bloque);
        log_debug(logger, "SIGUE EN ELSE");
        escribirEnBloqueDeDatos(bytes_primer_bloque, posicion_punt_direct, tamanio_a_escribir_en_primer_bloque, nombre_archivo, primer_bloque_a_escribir);
        escribirArchDesdePunteroIndirecto(superbloque -> block_size, tamanio_a_escribir - tamanio_a_escribir_en_primer_bloque, puntero_indirecto, bytes_siguientes_bloques, nombre_archivo, primer_bloque_a_escribir + 1);
        return 0;
    }
}
int escribirArchDesdePunteroIndirecto(int puntero, int tamanio_a_escribir, int puntero_indirecto, char* bytes_a_escribir, char* nombre_archivo, int bloque_a_escribir){
    log_debug(logger, "Entro en escribirArchDesdePunteroIndirecto");
    int posicion_punt_direct = puntero_indirecto * superbloque -> block_size + puntero;
    int primer_bloque_a_escribir = puntero / superbloque -> block_size - 1; // Menos 1 para acomodar el indice (el primer bloque es el puntero directo)
    int bloques_completos_a_escribir = tamanio_a_escribir / superbloque -> block_size;
    int bytes_finales_a_escribir = tamanio_a_escribir % superbloque -> block_size;
    int bloques_a_escribir;
    if (bytes_finales_a_escribir > 0)
        bloques_a_escribir = bloques_completos_a_escribir + 1;
  
    int* lista_de_punteros_a_bloques = leerEnBloqueDePunteros(puntero_indirecto);
    
    int desplazamiento_en_bytes_a_escribir = 0;
    int i;
    for(i = 0; i < bloques_completos_a_escribir; i++){
        log_debug(logger, "BLOQUES TEST %d - %d", bloques_completos_a_escribir, i);
        char* bytes_a_escribir_en_bloque = string_substring(bytes_a_escribir, desplazamiento_en_bytes_a_escribir, superbloque -> block_size);
        log_debug(logger, "LISTA PUNTEROS %d", lista_de_punteros_a_bloques[i]);
        escribirEnBloqueDeDatos(bytes_a_escribir_en_bloque, (lista_de_punteros_a_bloques[i])*(superbloque -> block_size), superbloque -> block_size, nombre_archivo, bloque_a_escribir + i);
        desplazamiento_en_bytes_a_escribir += superbloque -> block_size;
    }

    if (bytes_finales_a_escribir > 0){
        char* bytes_a_escribir_en_bloque_final = string_substring(bytes_a_escribir, desplazamiento_en_bytes_a_escribir, bytes_finales_a_escribir);
        escribirEnBloqueDeDatos(bytes_a_escribir_en_bloque_final, (lista_de_punteros_a_bloques[bloques_completos_a_escribir])*superbloque -> block_size, bytes_finales_a_escribir, nombre_archivo, bloque_a_escribir + i);
    }

    return 0;

}

void responderAKernel(int codigo){
    void* buffer = malloc(sizeof(int));
    memcpy(buffer, &codigo, sizeof(int));
    send(socket_kernel, buffer, sizeof(int), 0);
}

//Primero debo enviar a memoria para obtener los bytes leidos para escribir 
// en el archivo
int escribirArchivo(int puntero, int tamanio_a_escribir, char* nombre_archivo, int direccion_fisica, int pid){
    //log_debug(logger, "PUNTERO: %d, SIZE: %d, NOMBRE: %s, DIR: %d", puntero, tamanio_a_escribir, nombre_archivo, direccion_fisica);
    t_fcb* fcb_file = dictionary_get(dict_fcbs, nombre_archivo);
    int tamanio_archivo = (int) config_get_int_value(fcb_file, "TAMANIO_ARCHIVO");
    int puntero_directo = (int) config_get_int_value(fcb_file, "PUNTERO_DIRECTO");
    int puntero_indirecto = (int) config_get_int_value(fcb_file, "PUNTERO_INDIRECTO");
    //log_debug(logger, "OBTENGO FCB");

    sem_wait(&semaforo);
    //log_debug(logger, "ENTRO A SEMAFORO");
    char* bytes_a_escribir = leerEnMemoria(direccion_fisica, tamanio_a_escribir, pid); //mock_memoria(direccion_fisica);
    bytes_a_escribir[tamanio_a_escribir] = '\0';
    sem_post(&semaforo);
    //log_debug(logger, "DATA A ESCRIBIR: %s", bytes_a_escribir);
    if(tamanio_a_escribir > tamanio_archivo){
        log_error(logger, "EL TAMANIO DEL ARCHIVO ES MENOR A LO QUE SE QUIERE ESCRIBIR");
        responderAKernel(ERROR_GENERICO);
        return -1;
    }
    else if(puntero > tamanio_archivo){
        log_error(logger, "EL PUNTERO APUNTA POR FUERA DEL ARCHIVO");
        responderAKernel(ERROR_GENERICO);
        return -1;
    }
    log_info(logger, "Escribir Archivo: %s - Puntero: %d - Memoria: %d- Tamaño: %d", nombre_archivo, puntero, direccion_fisica, tamanio_a_escribir);

    if(tamanio_archivo <= superbloque -> block_size){
        log_debug(logger, "ENTRO A TAMANIO ARCHIVO MENOR A BLOQUE");
        int posicion_punt_direct = puntero_directo * (superbloque -> block_size);
        log_debug(logger, "ESCRIBIENDO: DATA: %s, POS_REAL: %d, SIZE: %d, ARCHIVO: %s", bytes_a_escribir, posicion_punt_direct + puntero, tamanio_a_escribir, nombre_archivo);
        escribirEnBloqueDeDatos(bytes_a_escribir, posicion_punt_direct + puntero, tamanio_a_escribir, nombre_archivo, 0); //Porque siempre estoy escribiendo en el primer bloque
    }
    else{
        int primer_bloque_a_escribir = puntero / superbloque -> block_size;
        log_info(logger, "PRIMER BLOQUE: %d", primer_bloque_a_escribir);
        if(primer_bloque_a_escribir == 0){
            log_debug(logger, "ENTRO A PRIMER BLOQUE A ESCRBIR 0");
            escribirArchDesdePunteroDirecto(puntero, tamanio_a_escribir, puntero_directo, puntero_indirecto, bytes_a_escribir, nombre_archivo, primer_bloque_a_escribir);
        }
        else{
            //log_debug(logger, "ENTRO A ESCRIBIR DESDE PUNTERO INDIRECTO");
            escribirArchDesdePunteroIndirecto(puntero, tamanio_a_escribir, puntero_indirecto, bytes_a_escribir, nombre_archivo, primer_bloque_a_escribir);

        }
           
    }
    responderAKernel(OK_ESCRIBIR_ARCHIVO);
        
    return 0;    
}

//Despues de esta funcion, debo a enviar a memoria los bloques
int leerArchivo(int puntero, int tamanio_a_leer, char* nombre_archivo, int direccion_fisica, int pid){
    t_fcb* fcb_file = dictionary_get(dict_fcbs, nombre_archivo);
    char* nombre = config_get_string_value(fcb_file, "NOMBRE_ARCHIVO");
    int tamanio_archivo = (int) config_get_int_value(fcb_file, "TAMANIO_ARCHIVO");
    int puntero_directo = (int) config_get_int_value(fcb_file, "PUNTERO_DIRECTO");
    int puntero_indirecto = (int) config_get_int_value(fcb_file, "PUNTERO_INDIRECTO");

    if(tamanio_archivo == 0){
        log_error(logger, "EL ARCHIVO ESTA VACIO, NO SE PUEDE LEER");
        responderAKernel(ERROR_GENERICO);
        return -1;
    }
    else if(tamanio_a_leer > tamanio_archivo){
        log_error(logger, "LA CANTIDAD DE BYTES A LEER ES MAYOR QUE EL TAMANIO DEL ARCHIVO");
        responderAKernel(ERROR_GENERICO);
        return -1;
    }
    else if(puntero > tamanio_archivo){
        log_error(logger, "EL PUNTERO APUNTA POR FUERA DEL ARCHIVO");
        responderAKernel(ERROR_GENERICO);
        return -1;
    }
    log_info(logger, "Leer Archivo: %s - Puntero: %d - Memoria: %d- Tamaño: %d", nombre_archivo, puntero, direccion_fisica, tamanio_a_leer);
    char* data_leida = malloc(sizeof(tamanio_a_leer));
    
    if(tamanio_archivo <= superbloque -> block_size){
        int posicion_punt_direct = puntero_directo * superbloque -> block_size;
        data_leida = leerUnBloqueDeDatos(posicion_punt_direct + puntero);
    }
    else{
        int primer_bloque_a_leer = puntero / superbloque -> block_size;
        if(primer_bloque_a_leer == 0)
            data_leida = leerArchDesdePunteroDirecto(puntero, tamanio_a_leer, puntero_directo, puntero_indirecto, nombre);
        else
            data_leida = leerArchDesdePunteroIndirecto(puntero, tamanio_a_leer, puntero_indirecto, nombre);
    }
    sem_wait(&semaforo);
    log_debug(logger, "Data: %s (%d) -> (%d)", data_leida, tamanio_a_leer, direccion_fisica);
    escribirEnMemoria(data_leida, tamanio_a_leer, direccion_fisica, pid);
    sem_post(&semaforo);
    responderAKernel(OK_LEER_ARCHIVO);

    return 0;    
}


int ocuparBloque(int bloque){
    bitarray_set_bit(bitmap, bloque);
    bloques_libres--;
    return 0;
}

int desocuparBloque(int bloque){
    bitarray_clean_bit(bitmap, bloque);
    bloques_libres++;
    return 0;
}

int buscarBloqueLibre(){
    for(int i = 0; i < superbloque->block_count; i++){
        bool estado = bitarray_test_bit(bitmap, i);
        log_info(logger,"Acceso a Bitmap - Bloque: %d - Estado: %d",i,estado);
        if(!estado)
            return i;
    }
    log_error(logger, "NO HAY BLOQUES LIBRES");
    return -1;
}

//Recibe el puntero ya en la posicion del archivo de bloques
int escribirEnBloqueDeDatos(char* data, int puntero, int tamanio, char* nombre_archivo, int bloque_archivo){
    char* data_a_escribir = malloc(tamanio);
    strcpy(data_a_escribir, data);
    log_debug(logger,"ESCRIBIENDO BLOQUE DE DATOS --> PUNTERO %d , TAMANIO %d , DATA %s, TAMANIOSRLEN: %d", puntero, tamanio, data, strlen(data));
    int bloque_fs = puntero / superbloque -> block_size;
    log_info(logger, "Acceso Bloque - Archivo: %s - Bloque Archivo: %d - Bloque File System: %d", nombre_archivo, bloque_archivo, bloque_fs);
    int seek = fseek(archivo_de_bloques, puntero, SEEK_SET);
    retardoAccesoBloque();
    if(seek != 0)
        log_error(logger, "ERROR FSEEK");
    int wr = fwrite(data_a_escribir, tamanio, 1, archivo_de_bloques);
    int re = fread(data_a_escribir, tamanio, 1, archivo_de_bloques);
    if(wr != 1)
        log_error(logger, "ERROR FWRITE");
    free(data_a_escribir);
    return 0;
}


//Recibe el puntero ya en la posicion del archivo de bloques y siempre lee un bloque
char* leerUnBloqueDeDatos(int puntero){
    char* data_leida = malloc(superbloque -> block_size);
    int seek = fseek(archivo_de_bloques, puntero, SEEK_SET);
    retardoAccesoBloque();
    if(seek != 0)
        log_error(logger, "ERROR FSEEK");
    int wr = fread(data_leida, superbloque -> block_size, 1, archivo_de_bloques);
    if(wr != 1)
        log_error(logger, "ERROR FWRITE");
    return data_leida;
}


int escribirEnBloqueDePunteros(int indice_del_bloque_de_punteros, int valor_puntero, int bloque_a_escribir){
    int posicion = superbloque -> block_size * bloque_a_escribir;
    //log_debug(logger, "ESCRIBIENDO EN PUNTERO DE BLOQUES -> INDICE: %d PUNTERO:%d BLOQUE: %d POSICION: %d", indice_del_bloque_de_punteros, valor_puntero, bloque_a_escribir, posicion);
    int seek = fseek(archivo_de_bloques, posicion + indice_del_bloque_de_punteros * sizeof(uint32_t), SEEK_SET);
    retardoAccesoBloque();
    if(seek != 0)
        log_error(logger, "ERROR FSEEK");
    int wr = fwrite(&valor_puntero, sizeof(int), 1, archivo_de_bloques);
    if(wr != 1)
        log_error(logger, "ERROR FWRITE escribirEnBloqueDePunteros");
    return 0;
}

//Esta funcion deberia leer el bloque entero
int* leerEnBloqueDePunteros(int puntero){
    puntero = superbloque -> block_size * puntero;
    int seek = fseek(archivo_de_bloques, puntero, SEEK_SET);
    retardoAccesoBloque();
    if(seek != 0)
        log_error(logger, "ERROR FSEEK leerEnBloqueDePunteros");
    int punteros_a_leer = superbloque -> block_size / sizeof(uint32_t); //Cantidad de punteros por bloque
    int* punteros_leidos = malloc(superbloque -> block_size);
    int wr = fread(punteros_leidos, sizeof(uint32_t),punteros_a_leer, archivo_de_bloques);

    log_debug(logger, "TERMINO DE LEER OK");
 
    return punteros_leidos;
}



int truncarMasBloques(char* nombre_archivo, int cant_bloques, int nuevoTamanio){
    t_fcb* fcb = dictionary_get(dict_fcbs, nombre_archivo);
    int tam_actual =  config_get_int_value(fcb, "TAMANIO_ARCHIVO");
    log_debug(logger,"TRUNCANDO MAS BLOQUES");
    //Sino tiene puntero indirecto, lo asigno
    if(nuevoTamanio <= superbloque -> block_size){ //Cuando nuevo tamaño es menor a un bloque
        int bloque = buscarBloqueLibre();
        ocuparBloque(bloque);
        config_set_value(fcb, "PUNTERO_DIRECTO", string_itoa(bloque));
        return 0;
    }
    else if(tam_actual == 0){
        int bloque = buscarBloqueLibre();
        ocuparBloque(bloque);
        config_set_value(fcb, "PUNTERO_DIRECTO", string_itoa(bloque));
    }
    if(tam_actual <= superbloque -> block_size){ //Cuando el tamaño original era menor a un bloque (No tenía puntero ind)
        log_debug(logger,"TRUNCANDO ARCHIVO DE UN BLOQUE");
        int bloque = buscarBloqueLibre();
        ocuparBloque(bloque);
        config_set_value(fcb, "PUNTERO_INDIRECTO", string_itoa(bloque));
    }
    int indice = 0;
    if (tam_actual > 0)
        indice = tam_actual / superbloque -> block_size - 1; // Para corregir el puntero indirecto y comienza desde 0.
    int bloque_puntero_indirecto = config_get_int_value(fcb, "PUNTERO_INDIRECTO");
    log_debug(logger,"TRUNCANDO MAS BLOQUES");
    for(int i = 0; i < cant_bloques; i++){
        int bloque = buscarBloqueLibre();
        //log_debug(logger,"TRUNCANDO MB BLOQUE: %d", bloque);
        ocuparBloque(bloque);
        escribirEnBloqueDePunteros(indice, bloque, bloque_puntero_indirecto);
        //log_debug(logger,"ESCRIBIENDO INDICE: %d", indice);
        indice++;
    }
    return 0;
}

int truncarMenosBloques(char* nombre_archivo, int cant_bloques_a_borrar, int nuevo_tamanio, int cant_bloques_actual){
    t_fcb* fcb = dictionary_get(dict_fcbs, nombre_archivo);
    log_debug(logger, "TRUNCANDO MENOS BLOQUES");
    int tam_actual =  config_get_int_value(fcb, "TAMANIO_ARCHIVO");
    log_debug(logger, "TRUNCANDO MENOS BLOQUES");
    if(cant_bloques_actual == cant_bloques_a_borrar){
        int puntero_directo = config_get_int_value(fcb, "PUNTERO_DIRECTO");
        desocuparBloque(puntero_directo);
        cant_bloques_a_borrar-=1;
    }
    
    int puntero_indirecto = config_get_int_value(fcb, "PUNTERO_INDIRECTO");
    int posicion_en_bloq_punteros = cant_bloques_actual - cant_bloques_a_borrar - 1; //-1 para el directo
    int* punteros_de_bloques = leerEnBloqueDePunteros(puntero_indirecto);
    for(int i = cant_bloques_a_borrar; i > 0; i--){
        log_debug(logger, "BORRANDO BLOQUES");
        int puntero = punteros_de_bloques[i];
        log_debug(logger, "DESOCUPANDO BLOQUE %d", puntero);
        desocuparBloque(puntero);
    }
    if(posicion_en_bloq_punteros == 0){
        int puntero_indirecto = config_get_int_value(fcb, "PUNTERO_INDIRECTO");
        desocuparBloque(puntero_indirecto);
    }
    return 0;
}



t_bitarray* levantarBitmap() {
	int tamanoBitmap = superbloque->block_count / 8;
	if(superbloque->block_count % 8 != 0)
		tamanoBitmap++;

	int fd = open(config -> path_bitmap, O_CREAT | O_RDWR, 0664);
    
    if(fd == -1){
        int error = errno;
        log_error(logger, "NO FUE POSIBLE CREAR EL ARCHIO DE BITMAP, ERROR: %d", errno);
        exit(1);
    }
    ftruncate(fd, tamanoBitmap);

	void* mmap_bitmap = mmap(NULL, tamanoBitmap, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    //log_debug(logger, "SE HIZO EL MMAP EL ARCHIVO DE BITMAP");

	if (mmap_bitmap == MAP_FAILED) {
		log_error(logger, "ERROR DE MAPEO");
		close(fd);
		exit(1);
	}

    //log_debug(logger, "SE CHEQUEO EL MAPEO");

	t_bitarray* bit_arr = bitarray_create_with_mode((char*) mmap_bitmap, tamanoBitmap, MSB_FIRST);

    //log_debug(logger, "SE CREO EL BITARRAY");

	size_t tope = bitarray_get_max_bit(bit_arr);

    //log_debug(logger, "SE OBTUVO EL TOPE");
	for (int i = 0; i < tope; i++) {
		bitarray_clean_bit(bit_arr, i);
	}

    //log_debug(logger, "SE INICIALIZARON TODOS LOS BITS");

	msync(mmap_bitmap, tamanoBitmap, MS_SYNC);
    close(fd);
    //log_debug(logger, "MSYINC Y CLOSE");
	return bit_arr;
}

void actualizar_bitmap_file(){

}


int levantar_archivo_de_bloques(){
    //archivo_de_bloques = open(config -> path_bloques,O_CREAT | O_RDWR);
    archivo_de_bloques = fopen(config -> path_bloques,"r+");
    if(archivo_de_bloques == NULL){
        //log_debug(logger, "CREANDO ARCHIVO");
        archivo_de_bloques = fopen(config -> path_bloques,"w+");
        int fd = fileno(archivo_de_bloques);
        int tamanio = superbloque -> block_count * superbloque -> block_size;
        if(ftruncate(fd, tamanio) == -1)
            log_error(logger, "ERROR LEVANTANDO EL ARCHIVO DE BLOQUES");
        
    }
    return 0;
}

t_filesystem_config* inicializar_config(char* path){
    t_config* raw;
    raw = config_create(path);
    config = malloc(sizeof(t_filesystem_config));
    config->ip_memoria = buscarEnConfig(raw, "IP_MEMORIA");
    config->puerto_escucha = buscarEnConfig(raw, "PUERTO_ESCUCHA");
    config->puerto_memoria = buscarEnConfig(raw, "PUERTO_MEMORIA");
    config->path_superbloque = buscarEnConfig(raw, "PATH_SUPERBLOQUE");
    config->path_bitmap = buscarEnConfig(raw, "PATH_BITMAP");
    config->path_bloques = buscarEnConfig(raw, "PATH_BLOQUES");
    config->path_FCB = buscarEnConfig(raw, "PATH_FCB");
    config->retardo_acceso_bloque = textoAuint32(buscarEnConfig(raw, "RETARDO_ACCESO_BLOQUE"));

    return config;
}

t_superbloque* inicializar_superbloque(){
    t_config* raw = config_create(config->path_superbloque);
    //log_debug(logger, "SB 1");
    superbloque = malloc(sizeof(t_superbloque));
    //log_debug(logger, "SB 2");
    superbloque -> block_size = textoAuint32(buscarEnConfig(raw, "BLOCK_SIZE"));
    //log_debug(logger, "SB 3");
    superbloque -> block_count = textoAuint32(buscarEnConfig(raw, "BLOCK_COUNT"));
    //log_debug(logger, "SB 4");
    return superbloque;
}
//Deserializar desde Kernel

void atenderReqKernel(){
    int codigo;
    recv(socket_kernel, &codigo, sizeof(int), MSG_WAITALL);
    //log_debug(logger, "CODIGO KERNEL: %d", codigo);
    if(codigo == REQ_LEER_ARCHIVO || codigo == REQ_ESCRIBIR_ARCHIVO ){
        char* nombre_archivo = string_new();
        int dir_fisica, puntero, tamanio_lectura_escritura, pid;
        deserializarDataArchivo(socket_kernel, &dir_fisica, &puntero, &tamanio_lectura_escritura, &pid, nombre_archivo);
        if(codigo == REQ_LEER_ARCHIVO)
            leerArchivo(puntero, tamanio_lectura_escritura, nombre_archivo, dir_fisica, pid);
        else 
            escribirArchivo(puntero, tamanio_lectura_escritura, nombre_archivo, dir_fisica, pid);
    }
    else if(codigo == REQ_TRUNCAR_ARCHIVO){
        char* nombre_archivo = string_new();
        int tamanio;
        //log_debug(logger, "REQ_TRUNCAR_ARCHIVO");
        deserializar_char_e_int(socket_kernel, &tamanio, nombre_archivo);
        //log_debug(logger, "DESERIALIZADO: %d", tamanio);

        truncarArchivo(nombre_archivo, tamanio);
    }
    else if(codigo == REQ_OP_MEMORIA){
        sem_wait(&semaforo);
        responderAKernel(OK_OP_MEMORIA);
        sem_post(&semaforo);
    }
    else{
        
        if(codigo == REQ_ABRIR_ARCHIVO){
            char* nombre_archivo = string_new();
            //log_debug(logger, "ABRIR ARCHIVO");
            deserializar_char(socket_kernel, nombre_archivo);
            //log_debug(logger, "NOMBRE: %s", nombre_archivo);
            abrirArchivo(nombre_archivo);
        }
        else if(codigo == REQ_CREAR_ARCHIVO){
            char* nombre_archivo = string_new();
            //log_debug(logger, "CREAR ARCHIVO");
            deserializar_char(socket_kernel, nombre_archivo);
            //log_debug(logger, "NOMBRE: %s", nombre_archivo);
            crearArchivo(nombre_archivo);
        }
    }

}


void deserializarDataArchivo(int socket, int* dir_fisica, int* puntero, int* tamanio_lectura_escritura, int* pid, char* nombre_archivo){
        int tamanio_nombre_archivo;
        recv(socket, pid, sizeof(int), MSG_WAITALL);

        recv(socket, dir_fisica, sizeof(int), MSG_WAITALL);
        //log_debug(logger, "DES DIR: %d", *dir_fisica);
        recv(socket, puntero, sizeof(int), MSG_WAITALL);
        //log_debug(logger, "DES PUNTERO: %d", *puntero);
        recv(socket, tamanio_lectura_escritura, sizeof(int), MSG_WAITALL);
        //log_debug(logger, "DES SIZE: %d", *tamanio_lectura_escritura);
        recv(socket, &tamanio_nombre_archivo, sizeof(int), MSG_WAITALL);
        //log_debug(logger, "DES SIZE_NOMBRE: %d", tamanio_nombre_archivo);
        char* n_arch = malloc(tamanio_nombre_archivo);
        recv(socket, n_arch, tamanio_nombre_archivo, MSG_WAITALL);
        //log_debug(logger, "DES NOMBRE: %s", n_arch);
        strcpy(nombre_archivo, n_arch);
}

void conexionMemoria(){
    log_info(logger, "CONEXION MEMORIA: Inicializado hilo de conexion con Memoria.");

    char *ipMemoria = config->ip_memoria;
    char *puertoMemoria = config->puerto_memoria;

    socket_memoria = inicializar_cliente(ipMemoria, puertoMemoria);

    while(socket_memoria == -1){
        log_info(logger, "INTENTANDO CONECTAR A MEMORIA");
        sleep(5);
        socket_memoria = inicializar_cliente(ipMemoria, puertoMemoria);
    }

    int response = handshake_cliente(socket_memoria, P_FILESYSTEM);


    if(response != 0)
        log_error(logger, "NO ES POSIBLE CONECTARSE A MEMORIA");

    log_info(logger, "CONEXION MEMORIA: Inicializado cliente de conexion con Memoria, socket: %d.", socket_memoria);
}

void conexionKernel(int socket_servidor){
    log_info(logger, "ESPERANDO AL KERNEL");
    socket_kernel = accept(socket_servidor, NULL, NULL);
    handshake_servidor(socket_kernel, P_KERNEL);
    log_info(logger, "CONEXION EXITOSA CON KERNEL");
    while(1){
        //log_debug(logger, "ESPERANDO DATA DEL KERNEL");
        atenderReqKernel();
    }
}


char* mock_memoria(int data){
    char* video_games = malloc(16);
    char* consoles = malloc(80);
    strcpy(video_games,"SonicTheHedgehog");
    strcpy(consoles,"SonyPlaystation1SonyPlaystation2SonyPlaystation3SonyPlaystation4SonyPlaystation5");
    if(data == 0)
        return video_games;
    else if(data == 32)
        return consoles;
    else
        return "BASURA";

}
void test(){
    /*
    F_OPEN Consoles
    F_TRUNCATE Consoles 256
    F_SEEK Consoles 128
    F_WRITE Consoles 32 80
    F_CLOSE Consoles
    F_OPEN Videogames
    F_TRUNCATE Videogames 64
    F_SEEK Videogames 0
    F_WRITE Videogames 0 16
    F_CLOSE Videogames
    EXIT

    Reemplazar LeerMemoria por mock_memoria
    */
    crearArchivo("Consoles");
    truncarArchivo("Consoles", 256);
    escribirArchivo(128, 80, "Consoles", 32, 10);
    crearArchivo("Videogames");
    truncarArchivo("Videogames", 64);
    escribirArchivo(0, 16, "Videogames", 0, 10);
}

void test_persistencia(){
    /*
    F_OPEN Videogames
    F_SEEK Videogames 0
    F_READ Videogames 16 16

    //Comentar escribirEnMemoria --> ver dataLeida en LOG_DEBUG

    */
    leerArchivo(0, 16, "Videogames", 16, 11);
}

int main(int argc, char ** argv){
    config = inicializar_config(argv[1]);
    logger = log_create("./cfg/filesystem.log", "FILESYSTEM", true, LOG_LEVEL_DEBUG);
    //log_debug(logger, "TEST1");
    superbloque = inicializar_superbloque();
    //log_debug(logger, "TEST2");
    bitmap = levantarBitmap();
    bloques_libres = superbloque -> block_count;
    //log_debug(logger, "TEST3");
    tam_max_archivo = superbloque -> block_size * ((superbloque -> block_size / sizeof(uint32_t)) + 1);
    //log_debug(logger, "TEST4");
    levantar_archivo_de_bloques();
    //log_debug(logger, "TEST5");
    inicializar_dict_fcb();
    sem_init(&semaforo, 0, 1);

    
    int socket_servidor = inicializar_servidor(config -> puerto_escucha);
    pthread_t hiloKernel;
    pthread_create(&hiloKernel, NULL, conexionKernel, socket_servidor);
    //log_info(logger, "CREO EL HILO DE KERNEL");
    pthread_t hiloMemoria;
    pthread_create(&hiloMemoria, NULL, conexionMemoria, P_FILESYSTEM);
    pthread_join(hiloMemoria, NULL);
    pthread_join(hiloKernel, NULL);
    
    //test();
    //test_persistencia();
    return 0;
}




