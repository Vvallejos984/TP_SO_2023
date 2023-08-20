#include "memoria.h"

t_log* logger;
void* memoria;
int* espacios;
int socket_memoria;
t_memoria_config* config;
t_list* listaDisp;
t_list* tablasDeSegmentos;
t_list* tablaSegmGlobal;
entradaTablaSegmentos* entradaSegm0;
sem_t mutex_escritura, compact;

void asignarEspaciosMemoria(int inicio, int size, int valor){
    for(int i=inicio; i<inicio+size; i++)
        espacios[i] = valor;
}

void leerTablaGlobal(){
    log_info(logger, "Tabla global tiene (%d) entradas", list_size(tablaSegmGlobal));
    for(int i=0; i<list_size(tablaSegmGlobal); i++){
        entradaGlSegmMemoria* entradaGl = list_get(tablaSegmGlobal, i);
        entradaTablaSegmentos* entrada = entradaGl->entrada;
        log_info(logger, "(%d) -> [%d] (%d|%d|%d)",
                i,
                entradaGl->pid,
                entrada->idSegmento,
                entrada->dirBase,
                entrada->size);
    }
}

void leerTablaSegmentos(int id){
    tablaSegmMemoria* tablaMem = list_get(tablasDeSegmentos, id);
    log_info(logger, "Leyendo la tabla del proceso (%d)", id);
    for(int i=0; i<list_size(tablaMem->tabla); i++){
        log_info(logger, "Linea (%d)", i);
        entradaTablaSegmentos* entrada = list_get(tablaMem->tabla, i);
        log_info(logger, "(%d) -> [%d] (%d|%d|%d)",
                i,
                tablaMem->pid,
                entrada->idSegmento,
                entrada->dirBase,
                entrada->size);
    }
}

void testTablasSegm(){
    entradaTablaSegmentos* entrada11 = new_entrada_ts(1, 150, 13);
    entradaTablaSegmentos* entrada21 = new_entrada_ts(1, 170, 8);
    entradaTablaSegmentos* entrada22 = new_entrada_ts(2, 180, 5);
    entradaTablaSegmentos* entrada31 = new_entrada_ts(1, 200, 7);
    entradaTablaSegmentos* entrada32 = new_entrada_ts(2, 210, 3);
    entradaTablaSegmentos* entrada41 = new_entrada_ts(1, 230, 8);
    entradaTablaSegmentos* entrada42 = new_entrada_ts(2, 245, 7);

    tablaSegmMemoria* t1 = new_tabla_segmentos(1);
    tablaSegmMemoria* t2 = new_tabla_segmentos(2);
    tablaSegmMemoria* t3 = new_tabla_segmentos(3);
    tablaSegmMemoria* t4 = new_tabla_segmentos(4);

    list_add(t1->tabla, entrada11);
    list_add(t2->tabla, entrada21);
    list_add(t2->tabla, entrada22);
    list_add(t3->tabla, entrada31);
    list_add(t3->tabla, entrada32);
    list_add(t4->tabla, entrada41);
    list_add(t4->tabla, entrada42);

    list_add(tablasDeSegmentos, t1);
    list_add(tablasDeSegmentos, t2);
    list_add(tablasDeSegmentos, t3);
    list_add(tablasDeSegmentos, t4);
}

int main(int argc, char ** argv){
    config = inicializar_config(argv[1]);
    logger = log_create("./cfg/memoria.log", "MEMORIA", true, LOG_LEVEL_INFO);

    entradaSegm0 = new_entrada_ts(0,0,config->tam_segmento_0);


    listaDisp = list_create();
    tablasDeSegmentos = list_create();
    tablaSegmGlobal = list_create();

    //leerTablaSegmentos(2);
    
    //Aloco un bloque de memoria
    log_info(logger, "Asignando bloque de memoria de %i bytes", config->tam_memoria);

    espacios = malloc(sizeof(int)*config->tam_memoria);

    asignarEspaciosMemoria(0, config->tam_segmento_0, 1);

    log_info(logger, "AFTER MALLOC");

    updateEspacios();

    sem_init(&mutex_escritura, 0, 1);
    sem_init(&compact, 0, 1);

    /*testTablasSegm();
    actualizarTSGlobal();
    leerTablaGlobal();

    updateEspacios();

    crearSegmento(3, 3, 35);
    leerTablaGlobal();

    eliminarSegmento(2, 1);
    leerTablaGlobal();

    eliminarProceso(4);
    leerTablaGlobal();*/

    /*log_info(logger, "Espacios disponibles:");
    leerEspacios(listaDisp);
    log_info(logger, "Espacios ocupados:");
    leerEspacios(listaOcup);*/

    //leerTablaSegmentos(2);

    /*log_info(logger, "Espacios disponibles:");
    leerEspacios(listaDisp);
    log_info(logger, "Espacios ocupados:");
    leerEspacios(listaOcup);*/
    log_info(logger, "AFTER SEM");
    memoria = malloc(config->tam_memoria);
    log_info(logger, "Bloque de memoria asignado con exito");

    //Levanto servidor 
    socket_memoria = inicializar_servidor(config->puerto_escucha); 
    //Espero clientes 
    espera_de_clientes_memoria();
    return 0;
}

int estaEnSuSegmento(int pid, int sizeBuffer, int idSegmento, int offset){
    int index = encontrarPorPID(pid);
    //log_info(logger, "Proceso %d en index %d", pid, index);

    if(index==-1){
        log_error(logger, "Tabla de segmentos del proceso %d no encontrada", pid);
        return -1;
    }

    tablaSegmMemoria* entradaTSM = list_get(tablasDeSegmentos, index);
    t_list* tablaDelProceso = entradaTSM->tabla;

    int indexSegm = encontrarPorIDSegm(tablaDelProceso, idSegmento);

    if(indexSegm==-1){
        log_error(logger, "Segmento de ID %d no encontrado para el proceso %d", idSegmento, pid);
        return -1;
    }

    entradaTablaSegmentos* segmento = list_get(tablaDelProceso, indexSegm);

    if(offset+sizeBuffer > segmento->size){
        log_error(logger, "Segmentation fault (Se intenta acceder a espacio de memoria no permitido)");
        //Retornar al Kernel código de error por Segmentation Fault
        return -1;
    }
    return 0;
}


int leerEnMemoria(int socket, int operacion){
    int direccion_fisica = -1, size = -1, pid;
    int resultLectura = -1;
    e_cod_cpu_mem resultado = OK_LECTURA;

    recv(socket, &pid, sizeof(int), MSG_WAITALL);
    log_debug(logger, "PID: %d", pid);
    recv(socket, &direccion_fisica, sizeof(int), MSG_WAITALL);
    log_debug(logger, "DIR: %d", direccion_fisica);
    recv(socket, &size, sizeof(int), MSG_WAITALL);
    log_debug(logger, "SIZE: %d", size);

    log_debug(logger, "INICIANDO LECTURA: (DIR %d) (SIZE %d)", direccion_fisica, size);

    char* contenido_leido = malloc(size);
    resultLectura = memcpy(contenido_leido,memoria+direccion_fisica, size);
    log_debug(logger, "MEMCPY -> %d", resultLectura);
    contenido_leido[size] = '\0';

    if(resultLectura == -1){
        log_error(logger, "No se escribieron bytes en memoria");
        resultado = ERR_LECTURA;
    }

    milisleep(config->retardo_memoria);
    
    if(resultado == OK_LECTURA){
        void* buffer = serializar_contenido_leido(contenido_leido, size);
        if(operacion == MOD_CPU){
            log_info(logger, "PID: %d - Acción: LECTURA - Dirección física: %d - Tamaño: %d - Origen: CPU", pid, direccion_fisica, size);
        }else{
            log_info(logger, "PID: %d - Acción: LECTURA - Dirección física: %d - Tamaño: %d - Origen: FS", pid, direccion_fisica, size);
        }
        
        log_debug(logger, "Contenido leido: (%d) %s", resultLectura, contenido_leido);

        int res = send(socket, buffer, 2*sizeof(int)+size, NULL);

        free(buffer);

        return res;
    }
    else{
        void* buffer = malloc(sizeof(int));
        memcpy(buffer,&resultado, sizeof(int));
        int res = send(socket, buffer, sizeof(int), NULL);
        free(buffer);
        return res;
    }
    

}

int escribirEnMemoria(int socket, e_cod_esc_lect operacion){
    
    int pid, size, dir;
    e_cod_cpu_mem resultado = OK_ESCRITURA;
    log_debug(logger, "ESCRIBIENDO EN MEMORIA");
    recv(socket, &pid, sizeof(int), MSG_WAITALL);
    log_debug(logger, "PID: %d", pid);
    recv(socket, &dir, sizeof(int), MSG_WAITALL);
    log_debug(logger, "DIR: %d", dir);
    recv(socket, &size, sizeof(int), MSG_WAITALL);
    log_debug(logger, "SIZE: %d", size);
    char* contenido = malloc(size);
    recv(socket, contenido, size, MSG_WAITALL);
    contenido[size] = '\0';
    log_debug(logger, "CONTENIDO: %s", contenido);
    
    log_debug(logger, "DESERIALIZADO %d %d %s", pid, dir, contenido);
    sem_wait(&mutex_escritura);
    int result = memcpy(memoria+dir, contenido, size);
    char* escrito;
    memcpy(escrito, memoria+dir, size);
    escrito[size]='\0';
    log_debug(logger, "ESCRITURA COMPLETADA: (%d) %s", result, escrito);
    sem_post(&mutex_escritura);

    if(result == -1){
        log_error(logger, "No se escribieron bytes en memoria");
        resultado = ERR_ESCRITURA;
    }

    if(operacion == MOD_CPU){
        log_info(logger, "PID: %d - Acción: ESCRIBIR - Dirección física: %d - Tamaño: %d - Origen: CPU", pid, dir, size);
    }else{
        log_info(logger, "PID: %d - Acción: ESCRIBIR - Dirección física: %d - Tamaño: %d - Origen: FS", pid, dir, size);
    }
    //log_info(logger, "Se escribieron %d bytes exitosamente", result);


    //Retardo de memoria
    log_debug(logger, "INICIO RETARDO (%d ms)", config->retardo_memoria);
    milisleep(config->retardo_memoria);
    log_debug(logger, "FIN RETARDO");
    void* buffer = malloc(sizeof(int));
    memcpy(buffer, &resultado, sizeof(int));
    int resSend = send(socket, buffer, sizeof(int), NULL);
    //Enviar OK de escritura
    log_debug(logger, "Enviados %d bytes", resSend);
    free(buffer);
    return resSend;
}

//Plasma en el vector de int los bytes ocupados (1) y disponibles (0)
void plasmarTablaGlobal(){
    asignarEspaciosMemoria(0, config->tam_memoria, 0);
    for(int i=0; i<list_size(tablaSegmGlobal); i++){
        entradaGlSegmMemoria* entradaGl = list_get(tablaSegmGlobal, i);
        asignarEspaciosMemoria(entradaGl->entrada->dirBase, entradaGl->entrada->size, 1);
        //mostrarMemoria(espacios);
    }
}

//Ante creación de un nuevo proceso
int new_tabla_segmentos(int socket){

    int pid;
    codigoKernelMemoria resultado = OK_INICIAR_PROCESO;

    recv(socket, &pid, sizeof(int), MSG_WAITALL);
    log_info(logger, "RECIBIO PID");
    tablaSegmMemoria* tabla = malloc(sizeof(tablaSegmMemoria));
    t_list* tablaSegmentos = list_create();
    list_add(tablaSegmentos, entradaSegm0);
    log_info(logger, "SIZE SEGM0 = %d", entradaSegm0->size);
    log_info(logger, "1");
    tabla->pid = pid;
    //log_error(logger, "pid agregado: %d", tabla->pid);
    tabla->tabla = tablaSegmentos;
    log_info(logger, "2");
    //EnviarTablaSegmentos(tabla)
    list_add(tablasDeSegmentos, tabla);

    int sizeTabla;
    void* tablaSerializada = serializarTablaSegmentos(tabla->tabla, &sizeTabla);

    int sizeBuffer = sizeof(int) + sizeTabla;
    void* buffer = malloc(sizeBuffer);
    memcpy(buffer, &resultado, sizeof(int));
    memcpy(buffer+sizeof(int), tablaSerializada, sizeTabla);
    send(socket, buffer, sizeBuffer, NULL);
    
    log_info(logger, "Creacion de Proceso PID: %d", pid);

    free(buffer);

    return 0;
}

void listGl_add_all_but_first(t_list* dest, tablaSegmMemoria* origin){
    t_list* origin_copy = origin->tabla;
    //log_info(logger, "Agregando %d entradas a la global", list_size(origin_copy)-1);
    for(int i=1; i<list_size(origin_copy); i++){
        entradaTablaSegmentos* entrada = list_get(origin_copy, i);
        entradaGlSegmMemoria* entradaGl = malloc(sizeof(entradaGlSegmMemoria));
        entradaGl->pid = origin->pid;
        entradaGl->entrada = entrada;
        list_add(dest, entradaGl);
    }
}

void actualizarTSGlobal(){
    entradaGlSegmMemoria* entradaGl0 = malloc(sizeof(entradaGlSegmMemoria));
    entradaGl0->entrada = entradaSegm0;
    entradaGl0->pid = 0;
    list_clean(tablaSegmGlobal);
    list_add(tablaSegmGlobal, entradaGl0);
    for(int i=0; i<list_size(tablasDeSegmentos);i++){
        tablaSegmMemoria* tablaSegm = list_get(tablasDeSegmentos,i);
        listGl_add_all_but_first(tablaSegmGlobal, tablaSegm);
    }
    plasmarTablaGlobal();
}

void leerEspacios(t_list* lista){
    for(int i=0; i<list_size(lista); i++){
        espacioMemoria* espacio = list_get(lista, i);
        printf("\n(%d) %d %d", i, espacio->base, espacio->size);
    }
    printf("\n\n");
}

int baseMenorCercana(int baseActual){
    int nuevaBase=0;
    //log_info(logger, "Buscando base anterior a -> (%d)", baseActual);
    for(int i=0; i<list_size(tablaSegmGlobal); i++){
        entradaGlSegmMemoria* entradaGl = list_get(tablaSegmGlobal, i);
        int baseAnterior = entradaGl->entrada->dirBase+entradaGl->entrada->size;
        if(baseActual>=baseAnterior){
            nuevaBase = baseAnterior;
        }
    }
    log_info(logger, "Nueva base (%d)->(%d)", baseActual, nuevaBase);
    return nuevaBase;
}

void mostrarMemoria(){
    printf("\n");
    for(int i=0; i<config->tam_memoria; i++){
        printf("%d", espacios[i]);
    }
    printf("\n");
}

int calcularBytesDisponibles(){
    int bytesDisp = 0;
    for(int i=0; i<list_size(listaDisp); i++){
        espacioMemoria* espacio = list_get(listaDisp, i);
        bytesDisp += espacio->size;
    }
    return bytesDisp;
}

int recalcularEspacioViable(int size){
    int bytesDisponibles = calcularBytesDisponibles();

    log_error(logger, "Espacio no encontrado: (%d) (%d)", size, bytesDisponibles);
    if(size<=bytesDisponibles){
        log_info(logger, "Bytes disponibles suficientes, compactar");
        return -2;
    }
        
    else{
        log_error(logger, "Bytes disponibles NO suficientes, abortar");
        return -1;
    }
}

/*Returns:
-1 cuando no se asignó el espacio
-2 cuando se solicita compactar
Nueva base cuando se encuentra una viable*/
int buscarEspacioViableFirst(int size){
    log_debug(logger, "FIRST");
    for(int i=0; i<list_size(listaDisp); i++){
        espacioMemoria* espacio = list_get(listaDisp, i);
        log_debug(logger, "(%d) <= (%d)", size, espacio->size);
        if(size<=espacio->size){
            return espacio->base;
        }
    }

    return recalcularEspacioViable(size);
}

/*Returns:
-1 cuando no se asignó el espacio
-2 cuando se solicita compactar
Nueva base cuando se encuentra una viable*/
int buscarEspacioViableBest(int size){
    log_debug(logger, "BEST");
    int nuevaBase = -1;
    int nuevaBaseSize = 0;
    for(int i=0; i<list_size(listaDisp); i++){
        espacioMemoria* espacio = list_get(listaDisp, i);
        if(nuevaBase==-1){
            if(size <= espacio->size){ //La primera
                nuevaBase = espacio->base;
                nuevaBaseSize = espacio->size;
            }
        }else{
            if(size <= espacio->size && espacio->size < nuevaBaseSize){ //Las siguientes
                nuevaBase = espacio->base;
                nuevaBaseSize = espacio->size;
            }
        log_debug(logger, "BASE: %d", nuevaBase);
        }
    }
    if(nuevaBase==-1){
        return recalcularEspacioViable(size);
    }
    return nuevaBase;
}

/*Returns:
-1 cuando no se asignó el espacio
-2 cuando se solicita compactar
Nueva base cuando se encuentra una viable*/
int buscarEspacioViableWorst(int size){
    log_debug(logger, "WORST");
    int nuevaBase = -1;
    int nuevaBaseSize = 0;
    for(int i=0; i<list_size(listaDisp); i++){
        espacioMemoria* espacio = list_get(listaDisp, i);
        if(size <= espacio->size && espacio->size > nuevaBaseSize){
            nuevaBase = espacio->base;
            nuevaBaseSize = espacio->size;
        }
    }
    if(nuevaBase==-1){
        return recalcularEspacioViable(size);
    }

    return nuevaBase;

}

int encontrarPorPID(int pid){
    log_info(logger, "Hay %d tablas de segmentos", list_size(tablasDeSegmentos));

    for(int i=0; i<list_size(tablasDeSegmentos); i++){
        tablaSegmMemoria* tablaMem = list_get(tablasDeSegmentos, i);

        log_info(logger, "Comparando PID (%d)=(%d)?", pid, tablaMem->pid);
        if(tablaMem->pid == pid){
            log_info(logger, "ENCONTRADO (%d)=(%d) en (%d)", pid, tablaMem->pid, i);
            return i;
        }       
    }
    return -1;
}

int encontrarPorIDSegm(t_list* tabla, int idSegm){
    for(int i=0; i<list_size(tabla); i++){
        entradaTablaSegmentos* entrada = list_get(tabla, i);
        log_info(logger, "Comparando idSegm (%d)=(%d)?", idSegm, entrada->idSegmento);
        if(entrada->idSegmento == idSegm)
            return i;
    }
    return -1;
}



/*Crea un segmento para el pid indicado
Retorna la dir base del segmento o -1 en caso de no asignarse*/
int crearSegmento(int socket){

    int pid, idSegm, size;

    codigoKernelMemoria resultado = OK_CREAR_SEGMENTO;

    recv(socket, &pid, sizeof(int), MSG_WAITALL);
    recv(socket, &idSegm, sizeof(int), MSG_WAITALL);
    recv(socket, &size, sizeof(int), MSG_WAITALL);

    int index = encontrarPorPID(pid);
    log_info(logger, "Proceso %d en index %d", pid, index);
    if(index==-1){
        log_error(logger, "Tabla de segmentos del proceso %d no encontrada", pid);
        resultado = ERROR_CREAR_SEGMENTO;
    }
    int nuevaBaseEncontrada = -3;

    log_debug(logger, "ALGORITMO: %s (%d)", config->algoritmo_asignacion, strlen(config->algoritmo_asignacion));

    log_debug(logger, "CANT ESPACIOS DISP: %d", list_size(listaDisp));

    if(strcmp(config->algoritmo_asignacion, "BEST")==0)
        nuevaBaseEncontrada = buscarEspacioViableBest(size);
    else if(strcmp(config->algoritmo_asignacion, "WORST")==0)
        nuevaBaseEncontrada = buscarEspacioViableWorst(size);
    else if(config->algoritmo_asignacion, "FIRST")
        nuevaBaseEncontrada = buscarEspacioViableFirst(size);
    
    if(nuevaBaseEncontrada==-3){
        log_error(logger, "El algoritmo %s es inexistente", config->algoritmo_asignacion);
        //Nunca debería pasar, implicaría un error en archivo de config de memoria
        resultado = ERROR_CONFIGURACION;
    }

    if(nuevaBaseEncontrada==-2){
        //log_error(logger, "Se solicita compactacion");
        log_info(logger, "Solicitud de Compactación");
        //Enviar al Kernel el codigo de solicitud de compactación
        resultado = REQ_PEDIR_COMPACTACION;
    }

    if(nuevaBaseEncontrada==-1){
        log_error(logger, "No se pudo crear un segmento de %d bytes (Memoria insuficiente)", size);
        //Enviar al Kernel el codigo de error de Out of Memory
        resultado = ERROR_CREAR_SEGMENTO;
    }

    if(resultado == OK_CREAR_SEGMENTO){
        
        log_info(logger, "PID: %d - Crear Segmento: %d - Base: %d - TAMAÑO: %d", pid, idSegm, nuevaBaseEncontrada,size);        
        //log_info(logger, "Segmento de %d bytes creado con base en %d", size, nuevaBaseEncontrada);
        entradaTablaSegmentos* entrada = new_entrada_ts(idSegm, nuevaBaseEncontrada, size);
        log_debug(logger, "ENTRADA");
        tablaSegmMemoria* tablaDelProceso = list_get(tablasDeSegmentos, index);
        log_debug(logger, "LIST GET");
        list_add(tablaDelProceso->tabla, entrada);
        log_debug(logger, "LIST ADD");
        actualizarTSGlobal();
        updateEspacios();
        log_debug(logger, "ACTUALIZAR TSG");

        int sizeTabla;
        void* tablaSerializada = serializarTablaSegmentos(tablaDelProceso->tabla, &sizeTabla);
        log_debug(logger, "SERIALIZAR TABLA");

        int sizeBuffer = sizeof(int) + sizeTabla;
        void* buffer = malloc(sizeBuffer);
        memcpy(buffer, &resultado, sizeof(int));
        memcpy(buffer+sizeof(int), tablaSerializada, sizeTabla);
        send(socket, buffer, sizeBuffer, NULL);
        log_debug(logger, "BYTES DISPONIBLES CREATE SEGMENT %d", calcularBytesDisponibles());
        free(buffer);
        return 0;
    }else{
        void* buffer = malloc(sizeof(int));
        memcpy(buffer,&resultado, sizeof(int));
        send(socket, buffer, sizeof(int), NULL);
        free(buffer);
        return -1;
    }
}



int eliminarProceso(int socket){
    int pid;
    codigoKernelMemoria resultado = OK_FIN_PROCESO;
    recv(socket, &pid, sizeof(int), MSG_WAITALL);
    int index = encontrarPorPID(pid);
    if(index==-1){
        log_error(logger, "Tabla de segmentos del proceso %d no encontrada", pid);
        resultado = ERROR_FIN_PROCESO;
    }

    if(resultado == OK_FIN_PROCESO){
        tablaSegmMemoria* tablaDelProceso = list_remove(tablasDeSegmentos, index);
        log_info(logger, "Eliminación de Proceso PID: %d", pid);
        actualizarTSGlobal();
    }
    
    void* buffer = malloc(sizeof(int));
    memcpy(buffer,&resultado, sizeof(int));
    
    send(socket, buffer, sizeof(int), NULL);
    free(buffer);

    return 0;
}

int eliminarSegmento(int socket){

    int pid, idSegm;

    codigoKernelMemoria resultado = OK_ELIMINAR_SEGMENTO;

    //VALIDAR NO ELIMINAR EL SEGMENTO 0, IDSEGM = 0

    recv(socket, &pid, sizeof(int), MSG_WAITALL);
    recv(socket, &idSegm, sizeof(int), MSG_WAITALL);

    int index = encontrarPorPID(pid);

    if(idSegm==0){
        log_error(logger, "No se puede eliminar segmento 0");
        resultado = ERROR_ELIMINAR_SEGMENTO;
    }

    log_debug(logger, "PID: %d, Index %d", pid, index);

    if(index==-1){
        log_error(logger, "Tabla de segmentos del proceso %d no encontrada", pid);
        resultado = ERROR_ELIMINAR_SEGMENTO;
    }

    tablaSegmMemoria* tablaDelProceso = list_get(tablasDeSegmentos, index);
    log_debug(logger, "Tabla identificada (Size = %d)", list_size(tablaDelProceso->tabla));

    int indexSegm = encontrarPorIDSegm(tablaDelProceso->tabla, idSegm);
    //log_error(logger, "idSegm: %d, indexSegm: %d", idSegm, indexSegm);
    if(indexSegm==-1){
        log_error(logger, "Segmento de ID %d no encontrado para el proceso %d", idSegm, pid);
        resultado = ERROR_ELIMINAR_SEGMENTO;
    }

    if(resultado == OK_ELIMINAR_SEGMENTO){
        entradaTablaSegmentos* segmentoABorrar = list_remove(tablaDelProceso->tabla, indexSegm);
        
        //log_info(logger, "Eliminado segmento %d del proceso %d", idSegm, pid);
        log_info(logger, "PID: %d - Eliminar Segmento: %d - Base: %d - TAMAÑO: %d", pid, idSegm, segmentoABorrar->dirBase, segmentoABorrar->size);

        free(segmentoABorrar);
        //Retornar tabla de segmentos actualizada

        actualizarTSGlobal();

        int sizeTabla;
        void* tablaSerializada = serializarTablaSegmentos(tablaDelProceso->tabla, &sizeTabla);

        int sizeBuffer = sizeof(int) + sizeTabla;
        void* buffer = malloc(sizeBuffer);
        memcpy(buffer, &resultado, sizeof(int));
        memcpy(buffer+sizeof(int), tablaSerializada, sizeTabla);
        send(socket, buffer, sizeBuffer, NULL);
        updateEspacios();
        log_debug(logger, "BYTES DISPONIBLES DELETE SEGMENT %d", calcularBytesDisponibles());
        free(buffer);
        return 0;
    }else{
        void* buffer = malloc(sizeof(int));
        memcpy(buffer,&resultado, sizeof(int));
        send(socket, buffer, sizeof(int), NULL);
        free(buffer);
        return -1;
    }
    
}

int moverEspacio(int base, int size){
    asignarEspaciosMemoria(base, size, 0);
    /*log_info(logger, "Recien borrado:");
    mostrarMemoria(espacios);*/
    int nuevaBase = baseMenorCercana(base);
    //log_info(logger, "Nueva base: %d", nuevaBase);
    asignarEspaciosMemoria(nuevaBase, size, 1);
    /*log_info(logger, "Recien escrito:");
    mostrarMemoria(espacios);*/

    updateEspacios(espacios);
    //log_info(logger, "Espacios ocupados:");

    return nuevaBase;
}

void compactarEspacios(int socket){

    codigoKernelMemoria resultado = OK_COMPACTACION;

    log_info(logger, "Iniciando compactacion...");
    t_list* oldList = list_duplicate(tablaSegmGlobal);
    for(int i=1; i<list_size(oldList); i++){
        //leerTablaGlobal();
        entradaGlSegmMemoria* entradaGl = list_get(oldList, i);
        //log_info(logger, "Tomando entrada %d de tabla global", i);
        int aux = entradaGl->entrada->dirBase;
        //log_info(logger, "Dir base: %d", entradaGl->entrada->dirBase);
        int nuevaBase = moverEspacio(entradaGl->entrada->dirBase, entradaGl->entrada->size);
        entradaGl->entrada->dirBase = nuevaBase;

        memcpy(memoria+nuevaBase, memoria+aux, entradaGl->entrada->size); //Testear valores después de compactar

        log_info("PID: %d - Segmento: %d - Base: %d - Tamaño %d", entradaGl->pid, entradaGl->entrada->idSegmento, entradaGl->entrada->dirBase, entradaGl->entrada->size);
        
    }
    //leerTablaGlobal();
    log_debug(logger, "Empieza Milisleep de %d", config->retardo_compactacion);
    milisleep(config->retardo_compactacion);

    int size;

    void* subBuffer = serializarTablas(&size);

    void* buffer = malloc(sizeof(int) + size);
    memcpy(buffer, &resultado, sizeof(int));

    memcpy(buffer+sizeof(int), subBuffer, size);

    send(socket, buffer, sizeof(int) + size, NULL); //Faltaría testear
    free(buffer);
    log_info(logger, "Compactacion exitosa");
}

void* serializarTablas(int* tamanio){
    log_debug(logger, "ENTRO A SERIALIZAR TABLAS");
    int cantidad = list_size(tablasDeSegmentos);
    int tamanioBuffer = sizeof(int);
    void* buffer = malloc(tamanioBuffer);
    int despl = 0;
    log_debug(logger, "CANTIDAD DE TABLAS %d", cantidad);
    memcpy(buffer+despl, &cantidad, sizeof(int));
    despl+=sizeof(int);

    for(int i = 0; i < cantidad; i++){
        
        tablaSegmMemoria* entrada = list_get(tablasDeSegmentos, i);

        tamanioBuffer += sizeof(int);
        buffer = realloc(buffer, tamanioBuffer);

        memcpy(buffer+despl, &(entrada->pid), sizeof(int)); //PID
        despl+=sizeof(int);

        int tam_tabla;

        void* subBuffer = serializarTablaSegmentos(entrada->tabla, &tam_tabla);

        tamanioBuffer += tam_tabla;

        buffer = realloc(buffer, tamanioBuffer);

        memcpy(buffer+despl, subBuffer, tam_tabla);
        despl+=tam_tabla;
    }

    *(tamanio) = despl;
    log_debug(logger, "TERMINO DE SERIALIZAR TABLAS");
    return buffer;
}


espacioMemoria* newEspacioMem(int base, int size){
    espacioMemoria* espacio = malloc(sizeof(espacioMemoria));
    espacio->base = base;
    espacio->size = size;

    return espacio;
}

void destruirEspacio(espacioMemoria* espacio){
    free(espacio);
}


//Actualiza la lista de espacios disponibles
void updateEspacios(){
    list_clean(listaDisp);

    int finMemoria = config->tam_memoria;
    for(int i=0; i<finMemoria;i++){
        if(espacios[i]==0 || i==finMemoria-1){
            for(int j=i+1; j<finMemoria; j++){
                if(espacios[j]!=0 || j==finMemoria-1){
                    int aux = j;
                    if(j==finMemoria-1)
                        aux++;
                    int base = i;
                    int size = aux-i;
                    espacioMemoria* espacioDisp = newEspacioMem(base, size);
                    list_add(listaDisp, espacioDisp);
                    //log_info(logger, "Espacio disponible -> (%d|%d)", base, size);
                    i = j;
                    break;
                }
            }
        }
    }
}

t_memoria_config* inicializar_config(char* path){
    t_config* raw;
    raw = config_create(path);

    t_memoria_config* config = malloc(sizeof(t_memoria_config));

    config->puerto_escucha = buscarEnConfig(raw, "PUERTO_ESCUCHA");
    config->tam_memoria = textoAuint32(buscarEnConfig(raw, "TAM_MEMORIA"));
    config->tam_segmento_0 = textoAuint32(buscarEnConfig(raw, "TAM_SEGMENTO_0"));
    config->cant_segmentos = textoAuint32(buscarEnConfig(raw, "CANT_SEGMENTOS"));
    config->retardo_memoria = textoAuint32(buscarEnConfig(raw, "RETARDO_MEMORIA"));
    config->retardo_compactacion = textoAuint32(buscarEnConfig(raw, "RETARDO_COMPACTACION"));
    config->algoritmo_asignacion = buscarEnConfig(raw, "ALGORITMO_ASIGNACION");
    config->ip_memoria = buscarEnConfig(raw, "IP_MEMORIA");
    
    return config;
}

int atenderKernel(int socket){
    log_info(logger, "CONECTADO AL KERNEL");
    codigoKernelMemoria codigo;
    while(1){
        recv(socket, &codigo, sizeof(codigoKernelMemoria), MSG_WAITALL);
        switch(codigo){
            case REQ_CREAR_SEGMENTO:
                crearSegmento(socket);
                break;
            case REQ_ELIMINAR_SEGMENTO:
                eliminarSegmento(socket);
                break;
            case REQ_INICIAR_PROCESO:
                log_info(logger, "ENTRO A REQ INICIAR PROCESO");
                new_tabla_segmentos(socket);
                break;
            case REQ_FIN_PROCESO:
                eliminarProceso(socket);
            break;
            case OK_COMPACTAR:
                compactarEspacios(socket);
            break;
             
        }
    } 
}

int atenderCPU(int socket){
    log_info(logger, "CONECTADO AL CPU");
    e_cod_cpu_mem codigo = -5;
    log_debug(logger, "COD: %d", codigo);
    while(1){
        recv(socket, &codigo, sizeof(e_cod_cpu_mem), MSG_WAITALL);
        int resultado;
        log_debug(logger, "CODIGO %d", codigo);
        switch(codigo){
            case REQ_LECTURA:
                resultado = leerEnMemoria(socket, MOD_CPU);
                //HACER VALIDACION DE RESULT DE LECTURA
            break;
            case REQ_ESCRITURA:
                resultado = escribirEnMemoria(socket, MOD_CPU); 
            break;
            default: log_error(logger, "CODIGO INCORRECTO: %d", codigo);

        }
    }
}

int atenderFilesystem(int socket){
    log_info(logger, "CONECTADO AL FILESYSTEM");
    e_cod_fs_mem codigo;
    while(1){
        recv(socket, &codigo, sizeof(e_cod_fs_mem), MSG_WAITALL);
        int resultado;
        log_info(logger, "RESULT: %d", codigo);
        switch(codigo){
            case REQ_LECTURA:
                resultado = leerEnMemoria(socket, MOD_FS);
                //HACER VALIDACION DE RESULT DE LECTURA
            break;
            case REQ_ESCRITURA:
                resultado = escribirEnMemoria(socket, MOD_FS);
            break;

        }
    }
}

int filtrarConexiones(int socket){
    log_info(logger, "Filtrando conexion");
    idProtocolo idConectado = handshake_servidor_multiple(socket, P_KERNEL, P_CPU, P_FILESYSTEM);
    log_info(logger, "Conexion filtrada: %d", idConectado);
    switch(idConectado){
        case P_KERNEL: atenderKernel(socket);
            break;
        case P_CPU: atenderCPU(socket);//Funcion de atender CPU
            break;
        case P_FILESYSTEM: atenderFilesystem(socket);//Funcion de atender filesystem
            break;
        case -1: log_error(logger, "Conexion fallida"); return -1; break;
    }
}

void espera_de_clientes_memoria() {
    dataHiloConexion* dataConexion = malloc(sizeof(dataConexion));
    dataConexion->socket = socket_memoria;
    dataConexion->funcion = filtrarConexiones;
    pthread_t atenderConexiones;
    log_info(logger, "Esperando clientes");
    pthread_create(&atenderConexiones, NULL, crear_hilo_conexion, dataConexion);
    pthread_join(atenderConexiones, NULL);
}

void* serializar_contenido_leido(char* contenido, int tamanio){
    void* buffer = malloc(2*sizeof(int) + tamanio);
    int codigo = OK_LECTURA;

    int offset = 0;

    memcpy(buffer + offset, &codigo, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &tamanio, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, contenido, tamanio);
    offset += tamanio;

    return buffer;

}

/*void *ejecutar_operacion_cliente(int *socket_client_p) {
    int socket_cliente = *socket_client_p;
    bool cliente_conectado = true;
    int pid = -1;

    while (1 && cliente_conectado) { 
        t_paquete *paquete = recibir_paquete(socket_cliente);
        char* operacion = traducirOperacion(paquete->codigo_operacion);
        t_paquete *paquete_a_enviar = NULL;
        uint32_t indice_t1 = -1;
    
        switch (paquete->codigo_operacion) {
            case HANDSHAKE:
                //HANDSHAKE
                //SERIALIZAR, ESTABLECER EL CODIGO DE LA OP Y ENVIAR PAQUETE CON LA FUNCION
                log_trace(logger_memoria, "%s recibido de %i", operacion, socket_cliente);

                indice_t1 = inicializarEstructuras(paquete);
                //paquete_a_enviar= FUNCION GENERICA SERIALIZACION serializar(2, INT,indice_t1);
                paquete_a_enviar->codigo_operacion = HANDSHAKE;
                enviar_paquete(paquete_a_enviar, socket_cliente);
                
                break;
            case INICIALIZAR_PROCESO:

                //Crear tabla

                //tablaSegmentos* tablasDeSegmentos = CrearTablaSegmentos(nroProceso);
                //entradaTablaSegmentos *segmento0 = (uint32_t*)malloc(sizeof(entradaTablaSegmentos));
                //list_add(tablasDeSegmentos->segmentos, segmento0);

                EnviarTablaInicial(1,1); //CREA TABLA CON SEGMENTO 0, SERIALIZA Y ENVIA
                
                //Serializar
                //serializarTablaSegmentos(tablasDeSegmentos->segmentos, sizeof(tablasDeSegmentos->segmentos));
                //Agrego codigo a paquete
                paquete_a_enviar->codigo_operacion = HANDSHAKE;

                //enviar paquete
                //enviar_paquete(paquete_a_enviar, socket_cliente);

                break;
            case REQ_LECTURA:
                uint32_t valorLeido = leerValor(paquete);
                //Serializar valorLeido en paquete_a_enviar
                paquete_a_enviar = serializar_int_e_int(valorLeido);   //******CONTROLAR********
                paquete_a_enviar->codigo_operacion = OK_LECTURA;
                //enviar paquete
                break;
            case REQ_ESCRITURA:
                escribirValor(paquete);
                paquete_a_enviar = malloc(sizeof(t_paquete));
                paquete_a_enviar->buffer = malloc(sizeof(t_buffer));
                paquete_a_enviar->buffer->size = 0;
                paquete_a_enviar->buffer->stream = NULL;
                paquete_a_enviar->codigo_operacion = OK_ESCRITURA;
                
                //enviar paquete
                break;
            default:
                log_warning(logger, "Codigo de operacion desconocido [%d]", paquete->codigo_operacion);
                break;
            case FINALIZAR_PROCESO:
                //FINALIZAR PROCESO
                //finalizarProcesoParticular();
                break;
    }
    if(paquete->buffer->stream != NULL) {
            free(paquete->buffer->stream);
        }
        free(paquete->buffer);
        free(paquete);
    }
    free(socket_client_p);    
    return NULL;
    
}*/