#include "kernel.h"

int cantidadProcesos = 0;
int socketMemoria;
int socketCPU;
int socketFileSystem;
int socketconsola;

mensajeKernel mensajeACpu;
mensajeKernel mensajePlanificador;
codigoKernelMemoria mensajeAMemoria;
codigoKernelMemoria respuestaMemoria;
e_codigo_paquete_fs mensajeFileSystem;

int parametroInt1;
int parametroInt2;
int parametroIO;
char *parametroChar;

t_kernel_config *kernel_config;
t_config *config;

t_list *listaRecursos;
t_list *listaReady;
t_list *listaExec;
t_list *listaBlock;
t_list *listaExit;

t_queue *colaNew;

t_dictionary *diccionarioSemaforos;
t_dictionary *tablaArchivosAbiertos;

time_t tiempo1, tiempo2;

pthread_mutex_t SYNC_FIN, CONEXIONMEMORIA, LISTAEXIT, PCBNEW, PCBREADY, PCBEXEC, MENSAJEAMEMORIA, MENSAJEACPU, MENSAJEAFILESYSTEM, TABLASEGMENTOS, PCBENEJECUCION, PLANICORTOPLAZO, PLANILARGOPLAZO, MENSAJELARGOPLAZO, IOPARAM, FINALIZARPCB;
sem_t grado_programacion;

int main(int argc, char ** argv)
{
    logger = log_create("kernel.log", "Kernel-Log", 1, LOG_LEVEL_INFO);

    inicializar_config(argv[1]);

    diccionarioSemaforos = dictionary_create();
    tablaArchivosAbiertos = dictionary_create();

    char *puertoEscucha = kernel_config->puerto_escucha;

    parametroChar = malloc(50);

    int servidor_kernel;

    servidor_kernel = inicializar_servidor(puertoEscucha);

    //log_info(logger, "MAIN: Inicializado el servidor: %d", servidor_kernel);

    /*************************************
     *************************************
     INICIALIZAMOS SEMAFOROS
     *************************************
     ************************************/
    pthread_mutex_init(&PCBNEW, NULL);

    pthread_mutex_init(&PCBREADY, NULL);

    pthread_mutex_init(&LISTAEXIT, NULL);

    pthread_mutex_init(&SYNC_FIN, NULL);

    pthread_mutex_init(&MENSAJEAMEMORIA, NULL);
    pthread_mutex_lock(&MENSAJEAMEMORIA);

    pthread_mutex_init(&MENSAJEACPU, NULL);
    pthread_mutex_lock(&MENSAJEACPU);

    pthread_mutex_init(&MENSAJEAFILESYSTEM, NULL);
    pthread_mutex_lock(&MENSAJEAFILESYSTEM);

    pthread_mutex_init(&PCBEXEC, NULL);
    pthread_mutex_lock(&PCBEXEC);

    pthread_mutex_init(&TABLASEGMENTOS, NULL);
    pthread_mutex_lock(&TABLASEGMENTOS);

    pthread_mutex_init(&PLANILARGOPLAZO, NULL);
    pthread_mutex_lock(&PLANILARGOPLAZO);

    pthread_mutex_init(&PCBENEJECUCION, NULL);

    pthread_mutex_init(&PLANICORTOPLAZO, NULL);
    pthread_mutex_init(&MENSAJELARGOPLAZO, NULL);
    pthread_mutex_init(&CONEXIONMEMORIA, NULL);

    pthread_mutex_init(&IOPARAM, NULL);

    pthread_mutex_init(&FINALIZARPCB, NULL);

    sem_init(&grado_programacion, 0, 0);

    /*************************************
     *************************************
     INICIALIZAMOS COLAS Y LISTAS
     *************************************
     ************************************/
    colaNew = queue_create();
    //log_info(logger, "MAIN: Creada Cola de procesos NEW en cero");
    listaReady = list_create();
    //log_info(logger, "MAIN: Creada Lista de procesos Ready en cero");
    listaExec = list_create();
    //log_info(logger, "MAIN: Creada Lista de procesos Execute en cero");
    listaBlock = list_create();
    //log_info(logger, "MAIN: Creada Lista de procesos Block en cero");
    //log_info(logger, "MAIN: Creada Cola de procesos EXIT en cero");
    listaExit = list_create();

    /*************************************
     *************************************
     INICIALIZAMOS COLAS DE RECURSOS
     *************************************
     ************************************/
    int cantidadRecursos = 0;
    cantidadRecursos = determinarRecursos();

    /*************************************
     *************************************
     CREAMOS HILOS DE CONEXIONES
     *************************************
     ************************************/
    pthread_t hiloMemoria, hiloFileSystem, hiloCPU, hiloHandlerConexionesEntrantes;
    pthread_create(&hiloMemoria, NULL, conexionMemoria, NULL);
    pthread_create(&hiloFileSystem, NULL, conexionFileSystem, NULL);
    pthread_create(&hiloCPU, NULL, conexionCPU, NULL);

    /*************************************
     *************************************
     CREAMOS HILO DE CONEXIONES ENTRANTES
     *************************************
     ************************************/
    pthread_create(&hiloHandlerConexionesEntrantes, NULL, handlerConexionesEntrantes, servidor_kernel);

    /*************************************
     *************************************
     CREAMOS HILO DE PLANIFICADOR DE LARGO PLAZO
     *************************************
     ************************************/
    pthread_t hiloPlanificadorLargo;
    pthread_create(&hiloPlanificadorLargo, NULL, planificacionLargoPlazo, NULL);

    /*************************************
     *************************************
     ELEGIMOS PLANIFICADOR
     *************************************
     ************************************/

    if (strcmp("HRRN", kernel_config->planificador) == 0)
    {

        pthread_t hiloHRRN;
        pthread_create(&hiloHRRN, NULL, planificacionHRRN, NULL);
        pthread_join(hiloHRRN, NULL);
    }
    else if (strcmp("FIFO", kernel_config->planificador) == 0)
    {

        pthread_t hiloFIFO;
        pthread_create(&hiloFIFO, NULL, &planificacionFIFO, NULL);
        pthread_join(hiloFIFO, NULL);
    }
    else
    {
        log_error(logger, "No existe el planificador.");
        exit(EXIT_FAILURE);
    };

    /*************************************
     *************************************
     LIBERAMOS RECURSOS
     *************************************
     ************************************/
    log_destroy(logger);
    config_destroy(config);
    free(kernel_config);
    sem_destroy(&grado_programacion);
    queue_destroy(colaNew);
    list_destroy(listaReady);

    return 0;
}

t_kernel_config *inicializar_config(char *path)
{
    config = config_create(path);

    kernel_config = malloc(sizeof(t_kernel_config));

    kernel_config->ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    kernel_config->puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");
    kernel_config->ip_filesystem = config_get_string_value(config, "IP_FILESYSTEM");
    kernel_config->puerto_filesystem = config_get_string_value(config, "PUERTO_FILESYSTEM");
    kernel_config->ip_cpu = config_get_string_value(config, "IP_CPU");
    kernel_config->puerto_cpu = config_get_string_value(config, "PUERTO_CPU");
    kernel_config->puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");
    kernel_config->planificador = config_get_string_value(config, "ALGORITMO_PLANIFICACION");
    kernel_config->estimacion_inicial = config_get_double_value(config, "ESTIMACION_INICIAL");
    kernel_config->hrrn_alfa = (float)config_get_double_value(config, "HRRN_ALFA");
    kernel_config->grado_multiprogramacion = (uint32_t)config_get_int_value(config, "GRADO_MAX_MULTIPROGRAMACION");
    kernel_config->recursos = config_get_string_value(config, "RECURSOS");
    kernel_config->instancias_recursos = config_get_string_value(config, "INSTANCIAS_RECURSOS");

    return kernel_config;
}

/*************************************
 *************************************
 CONEXION MEMORIA
 *************************************
 ************************************/
void conexionMemoria()
{

    log_debug(logger, "CONEXION MEMORIA: Inicializado hilo de conexion con Memoria.");

    char *ipMemoria = kernel_config->ip_memoria;
    char *puertoMemoria = kernel_config->puerto_memoria;

    while ((socketMemoria = inicializar_cliente(ipMemoria, puertoMemoria)) == -1)
    {
        log_debug(logger, "CONEXION MEMORIA: Error al conectarse con Memoria.");
        sleep(5);
    }

    if (handshake_cliente(socketMemoria, P_KERNEL) != 0)
    {
        log_error(logger, "CONEXION MEMORIA: Falló Handshake con Memoria.");
    }

    log_info(logger, "CONEXION MEMORIA: Inicializado cliente de conexion con Memoria, socket: %d.", socketMemoria);

    uint32_t pid;
    void *buffer;
    int tamanio_buffer;
    codigoKernelMemoria response;
    t_pcb_kernel *pcb;
    t_list *tablaRecibida = list_create();
    t_list *tablaGlobalDeSegmentos = list_create();

    while (1)
    {
        pthread_mutex_lock(&MENSAJEAMEMORIA);
        tamanio_buffer = 0;

        switch (mensajeAMemoria)
        {

        case REQ_INICIAR_PROCESO:
            //log_info(logger, "ENTRO REQ_INICIAR_PROCESO");
            pcb = queue_peek(colaNew);
            //log_info(logger, "PID %d", pcb -> pid);
            buffer = serializar_int(mensajeAMemoria, pcb -> pid, &tamanio_buffer); // Se serializa REQ_INICIAR_PROCESO

            //log_info(logger, "TAMANIO BUFFER %d", tamanio_buffer);
            int losBytes = send(socketMemoria, buffer, tamanio_buffer, 0);
            //log_info(logger, "ENVIO A MEMORIA %d", losBytes);
            int codigo;
            recv(socketMemoria, &codigo, sizeof(int), MSG_WAITALL);
            //log_info(logger, "RECIBO DE MEMORIA: %d", codigo);
            tablaRecibida = deserializarTablaSegmentos(socketMemoria);

            pthread_mutex_lock(&PCBNEW);
            pcb = queue_peek(colaNew);
            pcb->tabla_segmentos = tablaRecibida;
            pthread_mutex_unlock(&TABLASEGMENTOS);
            pthread_mutex_unlock(&PCBNEW);
            pthread_mutex_unlock(&PCBREADY);

            break;

        case REQ_CREAR_SEGMENTO:
            pcb = list_get(listaExec, 0);
            pid = pcb->pid;

            log_info(logger, "CONEXION MEMORIA: PID: %u - CREAR SEGMENTO - ID: %d - TAMAÑO: %d.", pcb->pid, parametroInt1, parametroInt2);

            buffer = serializar_3_int(mensajeAMemoria, pcb->pid, parametroInt1, parametroInt2, &tamanio_buffer); // Se serializa REQ_INICIAR_PROCESO, parametroInt1(id del segmento), parametroInt2(tamaño del segmento) y pid
            send(socketMemoria, buffer, tamanio_buffer, 0);

            recv(socketMemoria, &response, sizeof(int), 0);

            switch (response)
            {
            case ERROR_CREAR_SEGMENTO:

                log_error(logger, "CONEXION MEMORIA: Finaliza el proceso %u - MOTIVO: OUT_OF_MEMORY.", pcb->pid);
                log_info(logger, "CONEXION MEMORIA: PID: %u - ESTADO ANTERIOR: S_RUNNING - ESTADO ACTUAL: S_EXIT.", pcb->pid);
                finalizarPcb(pcb);
                showLists();

                break;

            case OK_CREAR_SEGMENTO:

                tablaRecibida = deserializarTablaSegmentos(socketMemoria);
                list_destroy_and_destroy_elements(pcb->tabla_segmentos, free);

                pcb->tabla_segmentos = tablaRecibida;
                pcb->program_counter += 1;

                pthread_mutex_unlock(&MENSAJEACPU);
                break;

            case REQ_PEDIR_COMPACTACION:

                log_info(logger, "CONEXION MEMORIA: COMPACTACION: SE SOLICITÓ COMPACTACIÓN.");

                e_codigo_paquete_fs request = REQ_OP_MEMORIA;
                int tamanio_request;

                buffer = serializar_codigo(request, &tamanio_request);

                log_info(logger, "CONEXION MEMORIA: COMPACTACION: SE CONSULTA A FILESYSTEM SI ESTÁ OPERANDO CON MEMORIA.");
                send(socketFileSystem, buffer, tamanio_request, 0);
                recv(socketFileSystem, &request, sizeof(int), MSG_WAITALL);

                if (request == OK_OP_MEMORIA)
                {
                    log_info(logger, "CONEXION MEMORIA: COMPACTACION: FILESYSTEM NO OPERANDO CON MEMORIA.");
                }

                // PEDIMOS COMPACTACION A MEMORIA

                response = OK_COMPACTAR;
                buffer = serializar_codigo(response, &tamanio_buffer); // Se serializa OK_COMPACTACION

                send(socketMemoria, buffer, tamanio_buffer, 0);
                recv(socketMemoria, &response, sizeof(int), MSG_WAITALL);

                if (response == OK_COMPACTACION)
                {
                    log_info(logger, "CONEXION MEMORIA: Se finalizó el proceso de compactación.");
                }

                int cantidad_tablas;
                recv(socketMemoria, &cantidad_tablas, sizeof(int), MSG_WAITALL);
                log_debug(logger, "CANTIDAD DE TABLAS RECIBIDAS %d", cantidad_tablas);
                for (int i = 0; i < cantidad_tablas; i++)
                {

                    tablaSegmMemoria *tablaSegm = malloc(sizeof(tablaSegmMemoria));
                    recv(socketMemoria, &pid, sizeof(int), MSG_WAITALL);

                    tablaSegm->pid = pid;

                    t_list *tablaRecv;

                    tablaRecv = deserializarTablaSegmentos(socketMemoria);

                    tablaSegm->tabla = tablaRecv;
                    log_debug(logger, "TABLA %d", i);
                    list_add(tablaGlobalDeSegmentos, tablaSegm);
                }
                log_debug(logger, "TERMINO DE DESERIALIZAR LAS TABLAS");
             
                actualizarTablasDeSegmentos(tablaGlobalDeSegmentos);
                log_debug(logger, "ACTUALIZO LAS TASBLAS");
                
                list_clean_and_destroy_elements(tablaGlobalDeSegmentos, free);
                log_info(logger, "CONEXION MEMORIA: COMPACTACION: SE FINALIZÓ LA COMPACTACIÓN.");

                pthread_mutex_unlock(&MENSAJEACPU);
                break;
            }

            break;

        case REQ_ELIMINAR_SEGMENTO:

            pcb = list_get(listaExec, 0);

            log_info(logger, "CONEXION MEMORIA: PID: %u - ELIMINAR SEGMENTO - ID: %d.", pcb->pid, parametroInt1);

            buffer = serializar_int_e_int(mensajeAMemoria, pcb->pid, parametroInt1, &tamanio_buffer);

            send(socketMemoria, buffer, tamanio_buffer, 0);

            int resp;

            recv(socketMemoria, &resp, sizeof(int), MSG_WAITALL);

            tablaRecibida = deserializarTablaSegmentos(socketMemoria);

            list_destroy_and_destroy_elements(pcb->tabla_segmentos, free);

            pcb->tabla_segmentos = tablaRecibida;
            pcb->program_counter += 1;

            pthread_mutex_unlock(&MENSAJEACPU);
            break;

        case REQ_FIN_PROCESO:

            pthread_mutex_lock(&LISTAEXIT);
            //log_info(logger, "LISTA EXIT: %d", list_size(listaExit));
            showLists();
            pcb = list_get(listaExit, 0);
            pid = pcb->pid;

            buffer = serializar_int(mensajeAMemoria, pid, &tamanio_buffer);
            //log_info(logger, "Serializar");

            send(socketMemoria, buffer, tamanio_buffer, 0);
            int confirmacion;
            recv(socketMemoria, &confirmacion, sizeof(int), MSG_WAITALL);

            pthread_mutex_unlock(&SYNC_FIN);
            pthread_mutex_unlock(&LISTAEXIT);

            pthread_mutex_unlock(&FINALIZARPCB);

            break;
        }
        pthread_mutex_unlock(&CONEXIONMEMORIA);
    }
}

/*************************************
 *************************************
 CONEXION FILE SYSTEM
 *************************************
 ************************************/
void conexionFileSystem()
{

    //log_info(logger, "CONEXION FILESYSTEM: Inicializado hilo de conexion con File System.");

    char *ipFileSystem = kernel_config->ip_filesystem;
    char *puertoFileSystem = kernel_config->puerto_filesystem;

    while ((socketFileSystem = inicializar_cliente(ipFileSystem, puertoFileSystem)) == -1)
    {
        log_debug(logger, "CONEXION FILESYSTEM: Error al conectarse con File System.");
        sleep(5);
    }

    if (handshake_cliente(socketFileSystem, P_KERNEL) != 0)
    {
        log_error(logger, "CONEXION FILESYSTEM: Falló Handshake con File System.");
    }

    //log_info(logger, "CONEXION FILESYSTEM: Inicializado cliente de conexion con File System, socket: %d.", socketFileSystem);

    t_pcb_kernel *pcb;
    uint32_t pid;
    t_file_pcb *file_pcb;
    int tamanio_buffer = 0;
    e_codigo_paquete_fs response;
    void *buffer;

    while (1)
    {

        pthread_mutex_lock(&MENSAJEAFILESYSTEM);
        int resSend;
        switch (mensajeFileSystem)
        {
        case REQ_ABRIR_ARCHIVO:

            log_debug(logger, "CONEXION FILESYSTEM: REQ_ABRIR_ARCHIVO %s", parametroChar);
            buffer = serializar_char(mensajeFileSystem, parametroChar, &tamanio_buffer); // mensajeFileSystem es REQ_ABRIR_ARCHIVO y parametroChar es el nombre del archivo
            send(socketFileSystem, buffer, tamanio_buffer, 0);

            recv(socketFileSystem, &response, sizeof(int), MSG_WAITALL);

            switch (response)
            {
            case ERROR_ABRIR_ARCHIVO:

                mensajeFileSystem = REQ_CREAR_ARCHIVO;

                buffer = serializar_char(mensajeFileSystem, parametroChar, &tamanio_buffer); // mensajeFileSystem es REQ_CREAR_ARCHIVO - Falta Funcion
                send(socketFileSystem, buffer, tamanio_buffer, 0);
                recv(socketFileSystem, &response, sizeof(int), MSG_WAITALL);

                pcb = list_get(listaExec, 0);

                log_info(logger, "CONEXION FILESYSTEM: PID: %u - SE CREO ARCHIVO: %s - TAMAÑO: 0.", pcb->pid, parametroChar);

                break;

            case OK_ABRIR_ARCHIVO:

                pcb = list_get(listaExec, 0);

                log_info(logger, "CONEXION FILESYSTEM: PID: %u - EXISTE ARCHIVO: %s.", pcb->pid, parametroChar);

                break;
            }

            pthread_mutex_unlock(&MENSAJEACPU);
            break;

        case REQ_TRUNCAR_ARCHIVO:

            pcb = list_remove(listaExec, 0);
            log_info(logger, "CONEXION FILESYSTEM: PID: %u - TRUNCAR - ARCHIVO: %s - TAMAÑO: %d.", pcb->pid, parametroChar, parametroInt1);
            time(&tiempo2);
            pcb->llegada_ready = tiempo2;
            actualizarProximaRafaga(pcb);
            pid = pcb->pid;
            pcb->estado = S_BLOCKED;
            list_add(listaBlock, pcb);
            log_info(logger, "CONEXION FILESYSTEM: PID: %u - ESTADO ANTERIOR: S_RUNNING - ESTADO ACTUAL: S_BLOCKED.", pcb->pid);
            showLists();

            buffer = serializar_char_e_int(mensajeFileSystem, parametroInt1, parametroChar, &tamanio_buffer); // mensajeFileSystem es REQ_TRUNCAR_ARCHIVO, parametroChar es el nombre del archivo y parametroInt1 es el tamaño
            send(socketFileSystem, buffer, tamanio_buffer, 0);

            pthread_mutex_lock(&PLANICORTOPLAZO);
            pthread_mutex_unlock(&PCBENEJECUCION);

            log_info(logger, "CONEXION FILESYSTEM: PID: %u - TRUNCAR - ESPERANDO RESPUESTA DE FILESYSTEM.", pcb->pid);
            recv(socketFileSystem, &response, sizeof(int), MSG_WAITALL);

            switch(response){
                case OK_TRUNCAR: log_debug(logger, "OK TRUNCAR"); break;
                case ERROR_GENERICO: log_error(logger, "ERROR AL TRUNCAR"); break;
                default: log_debug(logger, "LLEGO BASURA AL TRUNCAR");
            }

            desbloquear_pcb(pid);

            break;

        case REQ_LEER_ARCHIVO:
            pcb = list_remove(listaExec, 0);
            file_pcb = list_find(pcb->archivos_abiertos, buscarFile);

            log_info(logger, "CONEXION FILESYSTEM: PID: %u - LEER - ARCHIVO: %s - PUNTERO: %d - DIRECCION DE MEMORIA: %d - TAMAÑO: %d.", pcb->pid, parametroChar, file_pcb->pointer, parametroInt2, parametroInt1);

            time(&tiempo2);
            pcb->llegada_ready = tiempo2;
            actualizarProximaRafaga(pcb);
            pid = pcb->pid;
            pcb->estado = S_BLOCKED;
            list_add(listaBlock, pcb);
            log_info(logger, "CONEXION FILESYSTEM: PID: %u - ESTADO ANTERIOR: S_RUNNING - ESTADO ACTUAL: S_BLOCKED.", pcb->pid);
            showLists();
            log_info(logger, "CONEXION FILESYSTEM: PID: %u - BLOQUEADO POR: %s.", pcb->pid, parametroChar);

            buffer = serializarDataArchivo(mensajeFileSystem, pcb->pid, parametroInt2, file_pcb->pointer, parametroInt1, parametroChar, &tamanio_buffer); // mensajeFileSystem es REQ_LEER_ARCHIVO, parametroChar es el nombre del archivo, file_pcb->pointer es el puntero, parametroInt1 es el tamaño y parametroInt2 es la direccion de memoria
            resSend = send(socketFileSystem, buffer, tamanio_buffer, 0);

            log_debug(logger, "ENVIADOS %d BYTES", resSend);

            pthread_mutex_lock(&PLANICORTOPLAZO);
            pthread_mutex_unlock(&PCBENEJECUCION);

            log_info(logger, "CONEXION FILESYSTEM: PID: %u - LEER - ESPERANDO RESPUESTA DE FILESYSTEM.", pcb->pid);
            recv(socketFileSystem, &response, sizeof(int), MSG_WAITALL);

            switch(response){
                case OK_LEER_ARCHIVO: log_debug(logger, "OK LEER"); break;
                case ERROR_GENERICO: log_error(logger, "ERROR AL LEER"); break;
                default: log_debug(logger, "LLEGO BASURA AL LEER");
            }

            desbloquear_pcb(pid);

            break;

        case REQ_ESCRIBIR_ARCHIVO:

            pcb = list_remove(listaExec, 0);
            file_pcb = list_find(pcb->archivos_abiertos, buscarFile);

            log_info(logger, "CONEXION FILESYSTEM: PID: %u - ESCRIBIR - ARCHIVO: %s - PUNTERO: %d - DIRECCION DE MEMORIA: %d - TAMAÑO: %d.", pcb->pid, parametroChar, file_pcb->pointer, parametroInt2, parametroInt1);

            time(&tiempo2);
            pcb->llegada_ready = tiempo2;
            actualizarProximaRafaga(pcb);
            pid = pcb->pid;
            pcb->estado = S_BLOCKED;
            list_add(listaBlock, pcb);
            log_info(logger, "CONEXION FILESYSTEM: PID: %u - ESTADO ANTERIOR: S_RUNNING - ESTADO ACTUAL: S_BLOCKED.", pcb->pid);
            showLists();
            log_info(logger, "CONEXION FILESYSTEM: PID: %u - BLOQUEADO POR: %s.", pcb->pid, parametroChar);

            buffer = serializarDataArchivo(mensajeFileSystem, pcb->pid, parametroInt2, file_pcb->pointer, parametroInt1, parametroChar, &tamanio_buffer); // mensajeFileSystem es REQ_ESCRIBIR_ARCHIVO, parametroChar es el nombre del archivo, file_pcb->pointer es el puntero, parametroInt1 es el tamaño y parametroInt2 es la direccion de memoria

            resSend = send(socketFileSystem, buffer, tamanio_buffer, 0);
            log_debug(logger, "ENVIADOS %d BYTES", resSend);

            pthread_mutex_lock(&PLANICORTOPLAZO);
            pthread_mutex_unlock(&PCBENEJECUCION);

            log_info(logger, "CONEXION FILESYSTEM: PID: %u - ESCRIBIR - ESPERANDO RESPUESTA DE FILESYSTEM.", pcb->pid);
            recv(socketFileSystem, &response, sizeof(int), MSG_WAITALL);

            switch(response){
                case OK_ESCRIBIR_ARCHIVO: log_debug(logger, "OK ESCRIBIR"); break;
                case ERROR_GENERICO: log_error(logger, "ERROR AL ESCRIBIR"); break;
                default: log_debug(logger, "LLEGO BASURA AL ESCRIBIR");
            }

            desbloquear_pcb(pid);

            break;
        }
    }
}

/*************************************
 *************************************
 CONEXION CPU
 *************************************
 ************************************/
void conexionCPU()
{

    log_info(logger, "CONEXION CPU: Inicializado hilo de conexion con CPU.");

    char *ipCPU = kernel_config->ip_cpu;
    char *puertoCPU = kernel_config->puerto_cpu;

    while ((socketCPU = inicializar_cliente(ipCPU, puertoCPU)) == -1)
    {
        log_debug(logger, "CONEXION CPU: Error al conectarse con CPU.");
        sleep(5);
    }

    if (handshake_cliente(socketCPU, P_KERNEL) != 0)
    {
        log_error(logger, "CONEXION CPU: Falló Handshake con CPU.");
    }

    log_info(logger, "CONEXION CPU: Inicializado cliente de conexion con CPU, socket: %d.", socketCPU);

    t_pcb_kernel *pcb;
    t_pcb_kernel *newPcb;
    t_pcb_cpu *pcbCpu;
    e_codigo_cpu_kernel codigo;
    t_recurso *recurso = malloc(sizeof(t_recurso));
    t_file_kernel *file_kernel = malloc(sizeof(t_file_kernel));
    t_file_pcb *file_pcb = malloc(sizeof(t_file_pcb));
    ;

    while (1)
    {

        pthread_mutex_lock(&MENSAJEACPU);

        pcb = list_get(listaExec, 0);

        pcbCpu = new_cpu_pcb(pcb);

        time(&tiempo1);
        log_debug(logger, "TIEMPO1: %d", tiempo1);

        int tamanio;

        //////////
        t_list* list = pcbCpu->tabla_segmentos;
        entradaTablaSegmentos* entrada = list_get(list,0);
        //log_info(logger, "SIZE SEGM = %d", entrada->size);
        //////////

        void* sub_buffer = serializarPCB(pcbCpu, &tamanio);
        int codigo = NUEVO_PCB;

        void* buffer = malloc(tamanio + sizeof(int));
        //log_info(logger, "INICIALIZANDO BUFFER");

        memcpy(buffer, &codigo, sizeof(int));
        //log_info(logger, "COPY CODIGO");

        memcpy(buffer + sizeof(int), sub_buffer, tamanio);

        send(socketCPU, buffer, tamanio + sizeof(int), 0);

        recv(socketCPU, &codigo, sizeof(int), MSG_WAITALL);

        switch (codigo)
        {
        case REQ_OPEN_FILE:
            pcbCpu = deserializar_contexto_y_char(socketCPU);
            actualizarContextoEjec(pcbCpu, pcb);

            file_pcb->filename = parametroChar;
            file_pcb->pointer = 0;
            list_add(pcb->archivos_abiertos, file_pcb);

            log_info(logger, "CONEXION CPU: PID: %u - ABRIR ARCHIVO: %s.", pcb->pid, file_pcb->filename);

            if (dictionary_has_key(tablaArchivosAbiertos, parametroChar))
            {
                file_kernel = dictionary_get(tablaArchivosAbiertos, parametroChar);
                if (file_kernel->open)
                {
                    pcb = list_remove(listaExec, 0);
                    log_info(logger, "CONEXION CPU: PID: %u - ESTADO ANTERIOR: S_RUNNING - ESTADO ACTUAL: S_BLOCKED.", pcb->pid);
                    showLists();
                    log_info(logger, "CONEXION CPU: PID: %u - BLOQUEADO POR: %s.", pcb->pid, file_kernel->filename);
                    time(&tiempo2);
                    pcb->llegada_ready = tiempo2;
                    actualizarProximaRafaga(pcb);
                    queue_push(file_kernel->pcbs, pcb);
                    dictionary_put(tablaArchivosAbiertos, parametroChar, file_kernel); //[Warning] - Tener en cuenta que esto no va a liberar la memoria del `data` original.

                    pthread_mutex_lock(&PLANICORTOPLAZO);
                    pthread_mutex_unlock(&PCBENEJECUCION);
                }
                else
                {
                    file_kernel->open = true;
                    dictionary_put(tablaArchivosAbiertos, parametroChar, file_kernel); //[Warning] - Tener en cuenta que esto no va a liberar la memoria del `data` original.
                    pcb->program_counter += 1;

                    pthread_mutex_unlock(&MENSAJEACPU);
                }
            }
            else
            {
                file_kernel->filename = parametroChar;
                file_kernel->open = true;
                file_kernel->pcbs = queue_create();

                dictionary_put(tablaArchivosAbiertos, parametroChar, file_kernel); //[Warning] - Tener en cuenta que esto no va a liberar la memoria del `data` original.

                pcb->program_counter += 1;

                mensajeFileSystem = REQ_ABRIR_ARCHIVO;

                pthread_mutex_unlock(&MENSAJEAFILESYSTEM);
            }
            break;

        case REQ_CLOSE_FILE:
            pcbCpu = deserializar_contexto_y_char(socketCPU);
            actualizarContextoEjec(pcbCpu, pcb);

            list_remove_and_destroy_by_condition(pcb->archivos_abiertos, buscarFile, free);
            pcb->program_counter += 1;

            t_file_kernel *file_kernel = dictionary_get(tablaArchivosAbiertos, parametroChar);

            file_kernel->open = false;

            log_info(logger, "CONEXION CPU: PID: %u - CERRAR ARCHIVO: %s.", pcb->pid, file_kernel->filename);

            if (queue_is_empty(file_kernel->pcbs))
            {
                dictionary_remove_and_destroy(tablaArchivosAbiertos, parametroChar, free);
            }
            else
            {
                newPcb = queue_pop(file_kernel->pcbs);

                list_add(listaReady, newPcb);
                pthread_mutex_lock(&PLANICORTOPLAZO);
                pthread_mutex_unlock(&PCBENEJECUCION);
                
                char* listaPCBs = listaPCBATexto(listaReady);

                log_info(logger, "CONEXION CPU: PID: %u - ESTADO ANTERIOR: S_BLOCKED - ESTADO ACTUAL: S_READY.", newPcb->pid);
                log_info(logger, "Cola Ready %s: %s", kernel_config->planificador, listaPCBs);

                free(listaPCBs);
                
                showLists();

            }
            pthread_mutex_unlock(&MENSAJEACPU);
            break;

        case REQ_SEEK_FILE:
            pcbCpu = deserializar_contexto_char_e_int(socketCPU, parametroChar, &parametroInt1);
            actualizarContextoEjec(pcbCpu, pcb);

            t_file_pcb *file_pcb = list_find(pcb->archivos_abiertos, buscarFile);
            file_pcb->pointer = parametroInt1;

            log_info(logger, "CONEXION CPU: PID: %u - ACTUALIZAR PUNTERO ARCHIVO: %s - PUNTERO: %d.", pcb->pid, parametroChar, parametroInt1);

            pcb->program_counter += 1;

            pthread_mutex_unlock(&MENSAJEACPU);
            break;

        case REQ_READ_FILE:
            pcbCpu = deserializar_contexto_char_y_2_int(socketCPU, parametroChar, &parametroInt2, &parametroInt1);
            actualizarContextoEjec(pcbCpu, pcb);

            mensajeFileSystem = REQ_LEER_ARCHIVO;

            pthread_mutex_unlock(&MENSAJEAFILESYSTEM);
            break;

        case REQ_WRITE_FILE:
            pcbCpu = deserializar_contexto_char_y_2_int(socketCPU, parametroChar, &parametroInt2, &parametroInt1); // int1:dir_fisica_wr_fl, int2:cant_bytes_wr_fl
            actualizarContextoEjec(pcbCpu, pcb);

            mensajeFileSystem = REQ_ESCRIBIR_ARCHIVO;

            pthread_mutex_unlock(&MENSAJEAFILESYSTEM);
            break;

        case REQ_TRUNCATE_FILE:
            pcbCpu = deserializar_contexto_char_e_int(socketCPU, parametroChar, &parametroInt1);
            actualizarContextoEjec(pcbCpu, pcb);

            mensajeFileSystem = REQ_TRUNCAR_ARCHIVO;

            pthread_mutex_unlock(&MENSAJEAFILESYSTEM);
            break;

        case REQ_WAIT:

            log_debug(logger, "ENTRO WAIT");
            pcbCpu = deserializar_contexto_y_char(socketCPU);
            log_debug(logger, "parametroChar (1): %s", parametroChar);

            actualizarContextoEjec(pcbCpu, pcb);
            log_debug(logger, "CONTEXTO ACTUALIZADO");

            if(list_find(listaRecursos, buscarRecurso)!=NULL){
                recurso = list_find(listaRecursos, buscarRecurso);
            }
            else{
                log_error(logger, "CONEXION CPU: Finaliza el proceso %u - MOTIVO: INVALID_RESOURCE.", pcb->pid);
                log_info(logger, "CONEXION CPU: PID: %u - ESTADO ANTERIOR: S_RUNNING - ESTADO ACTUAL: S_EXIT.", pcb->pid);
                finalizarPcb(pcb);
                showLists();
                break;
            }
            
            log_debug(logger, "RECURSO: %s", recurso->recurso);
            pcb->program_counter += 1;
            
            if (recurso->instancias <= 0)
            {

                recurso->instancias -= 1;
                log_info(logger, "CONEXION CPU: PID: %u - WAIT: %s - INSTANCIAS: %d.", pcb->pid, parametroChar, recurso->instancias);
                pcb = list_remove(listaExec, 0);
                time(&tiempo2);
                pcb->llegada_ready = tiempo2;
                actualizarProximaRafaga(pcb);
                queue_push(recurso->colaBloqueados, pcb);
                log_info(logger, "CONEXION CPU: PID: %u - ESTADO ANTERIOR: S_RUNNING - ESTADO ACTUAL: S_BLOCKED.", pcb->pid);
                showLists();
                log_info(logger, "CONEXION CPU: PID: %u - BLOQUEADO POR: %s.", pcb->pid, recurso->recurso);

                pthread_mutex_lock(&PLANICORTOPLAZO);
                pthread_mutex_unlock(&PCBENEJECUCION);
            }
            else
            {

                recurso->instancias -= 1;
                log_info(logger, "CONEXION CPU: PID: %u - WAIT: %s - INSTANCIAS: %d.", pcb->pid, parametroChar, recurso->instancias);
                pthread_mutex_unlock(&MENSAJEACPU);
            }
            break;

        case REQ_SIGNAL:
            pcbCpu = deserializar_contexto_y_char(socketCPU);
            actualizarContextoEjec(pcbCpu, pcb);

            if(list_find(listaRecursos, buscarRecurso)!=NULL){
                recurso = list_find(listaRecursos, buscarRecurso);
            }
            else{
                log_error(logger, "CONEXION CPU: Finaliza el proceso %u - MOTIVO: INVALID_RESOURCE.", pcb->pid);
                log_info(logger, "CONEXION CPU: PID: %u - ESTADO ANTERIOR: S_RUNNING - ESTADO ACTUAL: S_EXIT.", pcb->pid);
                finalizarPcb(pcb);
                showLists();
                break;
            }

            recurso = list_find(listaRecursos, buscarRecurso);

            if (recurso != NULL)
            {
                recurso->instancias += 1;
                log_info(logger, "CONEXION CPU: PID: %u - SIGNAL: %s - INSTANCIAS: %d.", pcb->pid, parametroChar, recurso->instancias);
                pcb->program_counter += 1;

                if (recurso->instancias >= 0)
                {
                    if (!queue_is_empty(recurso->colaBloqueados))
                    {

                        newPcb = queue_pop(recurso->colaBloqueados);

                        list_add(listaReady, newPcb);
                        pthread_mutex_lock(&PLANICORTOPLAZO);
                        pthread_mutex_unlock(&PCBENEJECUCION);
                    }
                    else
                    {
                    }
                }
                else
                {
                    recurso->instancias += 1;
                    log_info(logger, "CONEXION CPU: PID: %u - SIGNAL: %s - INSTANCIAS: %d.", pcb->pid, parametroChar, recurso->instancias);
                }
            }
            pthread_mutex_unlock(&MENSAJEACPU);
            break;

        case REQ_I_O:
            pcbCpu = deserializar_contexto_e_int(socketCPU);
            log_debug(logger, "TIEMPO: %d", parametroInt1);
            actualizarContextoEjec(pcbCpu, pcb);

            pcb = list_remove(listaExec, 0);
            log_debug(logger, "En ejec: %d", list_size(listaExec));
            log_debug(logger, "En ready: %d", list_size(listaReady));
            time(&tiempo2);
            pcb->llegada_ready = tiempo2;
            actualizarProximaRafaga(pcb);

            log_info(logger, "CONEXION CPU: PID: %u - ESTADO ANTERIOR: S_RUNNING - ESTADO ACTUAL: S_BLOCKED.", pcb->pid);
            showLists();

            pthread_mutex_lock(&IOPARAM);
            parametroIO = parametroInt1;

            pthread_t hiloIO;

            pthread_create(&hiloIO, NULL, handlerIO, pcb);

            pthread_detach(hiloIO);

            pthread_mutex_lock(&PLANICORTOPLAZO);
            pthread_mutex_unlock(&PCBENEJECUCION);
            break;

        case REQ_YIELD:
            pcbCpu = deserializarPCB(socketCPU);
            actualizarContextoEjec(pcbCpu, pcb);

            pcb = list_remove(listaExec, 0);
            log_debug(logger, "(%d) SALE DE RUNNING", pcb->pid);
            time(&tiempo2);
            pcb->llegada_ready = tiempo2;
            actualizarProximaRafaga(pcb);

            pcb->estado = S_READY;
            pcb->program_counter ++;
            list_add(listaReady, pcb);
            log_debug(logger, "(%d) ENTRA A READY", pcb->pid);
            showLists();
            pthread_mutex_lock(&PLANICORTOPLAZO);
            pthread_mutex_unlock(&PCBENEJECUCION);

            char* listaPCBs = listaPCBATexto(listaReady);

            log_info(logger, "CONEXION CPU: PID: %u - ESTADO ANTERIOR: S_RUNNING - ESTADO ACTUAL: S_READY.", pcb->pid);
            log_info(logger, "Cola Ready %s: %s", kernel_config->planificador, listaPCBs);

            free(listaPCBs);

            showLists();

            //pthread_mutex_unlock(&MENSAJEAMEMORIA);
            break;

        case REQ_EXIT:
            pcbCpu = deserializarPCB(socketCPU);
            actualizarContextoEjec(pcbCpu, pcb);
            log_info(logger, "CONEXION CPU: Finaliza el proceso %u - MOTIVO: SUCCESS.", pcb->pid);
            log_info(logger, "CONEXION CPU: PID: %u - ESTADO ANTERIOR: S_RUNNING - ESTADO ACTUAL: S_EXIT.", pcb->pid);
            finalizarPcb(pcb);
            showLists();
            break;

        case REQ_CREATE_SEGMENT:
            pcbCpu = deserializar_contexto_int_e_int(socketCPU, &parametroInt1, &parametroInt2); // parametroInt1(id segmento) parametroInt2(tamaño segmento)
            actualizarContextoEjec(pcbCpu, pcb);

            pthread_mutex_lock(&CONEXIONMEMORIA);
            mensajeAMemoria = REQ_CREAR_SEGMENTO;
            pthread_mutex_unlock(&MENSAJEAMEMORIA);
            break;

        case REQ_DELETE_SEGMENT:
            pcbCpu = deserializar_contexto_e_int(socketCPU); // parametroInt1(id segmento)
            actualizarContextoEjec(pcbCpu, pcb);

            pthread_mutex_lock(&CONEXIONMEMORIA);
            mensajeAMemoria = REQ_ELIMINAR_SEGMENTO;
            pthread_mutex_unlock(&MENSAJEAMEMORIA);
            break;

        case ERR_FILE_OPENED:
            pcbCpu = deserializarPCB(socketCPU);
            actualizarContextoEjec(pcbCpu, pcb);

            log_error(logger, "CONEXION CPU: Finaliza el proceso %u - MOTIVO: ERROR_FILE_OPENED.", pcb->pid);
            log_info(logger, "CONEXION CPU: PID: %u - ESTADO ANTERIOR: S_RUNNING - ESTADO ACTUAL: S_EXIT.", pcb->pid);
            finalizarPcb(pcb);
            showLists();
            break;

        case ERR_FILE_NOT_OPEN:
            pcbCpu = deserializarPCB(socketCPU);
            actualizarContextoEjec(pcbCpu, pcb);

            log_error(logger, "CONEXION CPU: Finaliza el proceso %u - MOTIVO: ERROR_FILE_NOT_OPEN.", pcb->pid);
            log_info(logger, "CONEXION CPU: PID: %u - ESTADO ANTERIOR: S_RUNNING - ESTADO ACTUAL: S_EXIT.", pcb->pid);
            finalizarPcb(pcb);
            showLists();
            break;

        case ERR_SEGMENTATION_FAULT:
            pcbCpu = deserializarPCB(socketCPU);
            actualizarContextoEjec(pcbCpu, pcb);

            log_error(logger, "CONEXION CPU: Finaliza el proceso %u - MOTIVO: SEG_FAULT.", pcb->pid);
            log_info(logger, "CONEXION CPU: PID: %u - ESTADO ANTERIOR: S_RUNNING - ESTADO ACTUAL: S_EXIT.", pcb->pid);
            finalizarPcb(pcb);
            break;

        case ERR_INVALID_REG:
            pcbCpu = deserializarPCB(socketCPU);
            actualizarContextoEjec(pcbCpu, pcb);

            log_error(logger, "CONEXION CPU: Finaliza el proceso %u - MOTIVO: ERROR_INVALID_REG.", pcb->pid);
            log_info(logger, "CONEXION CPU: PID: %u - ESTADO ANTERIOR: S_RUNNING - ESTADO ACTUAL: S_EXIT.", pcb->pid);
            finalizarPcb(pcb);
            showLists();
            break;

        case ERR_INVALID_SEGMENT:
            pcbCpu = deserializarPCB(socketCPU);
            actualizarContextoEjec(pcbCpu, pcb);

            log_error(logger, "CONEXION CPU: Finaliza el proceso %u - MOTIVO: ERROR_INVALID_SEGMENT.", pcb->pid);
            log_info(logger, "CONEXION CPU: PID: %u - ESTADO ANTERIOR: S_RUNNING - ESTADO ACTUAL: S_EXIT.", pcb->pid);
            finalizarPcb(pcb);
            showLists();
            break;

        case ERR_READ_MEMORY:
            pcbCpu = deserializarPCB(socketCPU);
            actualizarContextoEjec(pcbCpu, pcb);

            log_error(logger, "CONEXION CPU: Finaliza el proceso %u - MOTIVO: ERROR_READ_MEMORY.", pcb->pid);
            log_info(logger, "CONEXION CPU: PID: %u - ESTADO ANTERIOR: S_RUNNING - ESTADO ACTUAL: S_EXIT.", pcb->pid);
            finalizarPcb(pcb);
            showLists();
            break;

        case ERR_WRITE_MEMORY:
            pcbCpu = deserializarPCB(socketCPU);
            actualizarContextoEjec(pcbCpu, pcb);

            log_error(logger, "CONEXION CPU: Finaliza el proceso %u - MOTIVO: ERROR_WRITE_MEMORY.", pcb->pid);
            log_info(logger, "CONEXION CPU: PID: %u - ESTADO ANTERIOR: S_RUNNING - ESTADO ACTUAL: S_EXIT.", pcb->pid);
            finalizarPcb(pcb);
            showLists();
            break;

        default:
            log_error(logger, "CONEXION CPU: REQUEST DE CPU A KERNEL INVALIDO: %d", codigo);
            log_info(logger, "CONEXION CPU: PID: %u - ESTADO ANTERIOR: S_RUNNING - ESTADO ACTUAL: S_EXIT.", pcb->pid);
            finalizarPcb(pcb);
            showLists();
        };
    }
}

void handlerConexionesEntrantes(int servidor_kernel)
{
    log_info(logger, "SERVIDOR: Creado hilo de conexiones entrantes.");

    while (1)
    {

        struct sockaddr_in direccionCliente;
        socklen_t tamanio_direccion = sizeof(direccionCliente);

        int socketConsola;

        if ((socketConsola = accept(servidor_kernel, (struct sockaddr *)&direccionCliente, &tamanio_direccion)) < 0)
        {
            log_info(logger, "Error al aceptar desde el socket.");
            continue;
        }

        log_info(logger, "SERVIDOR: Consola %d conectada", socketConsola);
        handshake_servidor(socketConsola, P_CONSOLA);
        pthread_t hiloConsola;

        pthread_create(&hiloConsola, NULL, handlerConsola, socketConsola);

        pthread_detach(&hiloConsola);
    }
}

void handlerConsola(int socketConsola)
{

    t_list *instrucciones = deserializarListaInstrucciones(socketConsola);
    leerLista(instrucciones);
    pthread_mutex_lock(&PCBNEW);
    t_pcb_kernel *pcb = crearPcb(instrucciones);
    log_info(logger, "HANDLER CONSOLA: Se creó el proceso %d en S_NEW", pcb->pid);
    pthread_mutex_t semaforo;
    pthread_mutex_init(&semaforo, NULL);
    pthread_mutex_lock(&semaforo);
    //log_info(logger, "HANDLER CONSOLA 1");
    char *key_sem = string_itoa(pcb->pid);
    dictionary_put(diccionarioSemaforos, key_sem, &semaforo);
    queue_push(colaNew, pcb);
    log_info(logger, "PID: %d - Estado Actual: S_NEW", pcb->pid);
    pthread_mutex_unlock(&PCBNEW);
    //log_info(logger, "HANDLER CONSOLA 2");
    pthread_mutex_lock(&MENSAJELARGOPLAZO);
    //log_info(logger, "HANDLER CONSOLA 3");
    mensajePlanificador = READY_PCB;
    pthread_mutex_unlock(&PLANILARGOPLAZO);
    //log_info(logger, "HANDLER CONSOLA 4");

    pthread_mutex_lock(&semaforo);
    //log_info(logger, "PASO SEMAFORO");

    mensajeKernel mensajeAConsola = FINALIZED_PCB;

    void *buffer;
    int tamanio_buffer = sizeof(int);
    buffer = serializar_codigo(mensajeAConsola, &tamanio_buffer);
    send(socketConsola, buffer, tamanio_buffer, 0);

    free(key_sem);
}

void *handlerIO(void *elemento)
{
    t_pcb_kernel *pcb = (t_pcb_kernel *)elemento;

    log_info(logger, "CONEXION CPU: PID: %u - BLOQUEADO POR: I/O.", pcb->pid);
    int tiempo = parametroIO;
    pthread_mutex_unlock(&IOPARAM);

    log_info(logger, "CONEXION CPU: PID: %u - EJECUTA I/O: %d SEGUNDOS.", pcb->pid, tiempo);
    sleep(tiempo);

    pcb->program_counter ++;

    list_add(listaReady, pcb);
    pthread_mutex_lock(&PLANICORTOPLAZO);
    pthread_mutex_unlock(&PCBENEJECUCION);

    char* listaPCBs = listaPCBATexto(listaReady);

    log_info(logger, "CONEXION CPU: PID: %u - ESTADO ANTERIOR: S_BLOCKED - ESTADO ACTUAL: S_READY.", pcb->pid);
    log_info(logger, "Cola Ready %s: %s", kernel_config->planificador, listaPCBs);

    free(listaPCBs);

    showLists();

    return NULL;
}

t_pcb_kernel *crearPcb(t_list *instrucciones)
{

    t_pcb_kernel *pcb = malloc(sizeof(t_pcb_kernel));
    t_registros_cpu *registros = malloc(sizeof(t_registros_cpu));

    pcb->pid = cantidadProcesos++;
    pcb->instrucciones = instrucciones;
    pcb->program_counter = 0;
    pcb->registros = registros;
    pcb->tabla_segmentos = list_create();
    pcb->est_prox_rafaga = (kernel_config->estimacion_inicial)/1000;
    pcb->llegada_ready = 0;
    pcb->archivos_abiertos = list_create();
    pcb->estado = S_NEW;

    return pcb;
}

/*************************************
 *************************************
 PLANIFICADOR DE LARGO PLAZO
 *************************************
 ************************************/

void planificacionLargoPlazo()
{

    log_info(logger, "PLANIFICACION LARGO PLAZO: Creado hilo de Planificación a Largo Plazo.");

    log_info(logger, "PLANIFICACION LARGO PLAZO: El grado de multiprogram es %d.", kernel_config->grado_multiprogramacion);

    int instancias = kernel_config->grado_multiprogramacion;
    sem_init(&grado_programacion, 0, instancias);

    time_t tiempoActual;

    t_pcb_kernel *pcb;

    while (1)
    {
        pthread_mutex_lock(&PLANILARGOPLAZO);
        log_debug(logger, "ENTRO: %d", mensajePlanificador);
        switch (mensajePlanificador)
        {

        case READY_PCB:
            sem_wait(&grado_programacion);
            //log_info(logger, "ENTRO A READY");
            pthread_mutex_lock(&CONEXIONMEMORIA);
            mensajeAMemoria = REQ_INICIAR_PROCESO;

            pthread_mutex_unlock(&MENSAJEAMEMORIA);
            //log_info(logger, "VOLVIO A PLANIFICADOR");
            pthread_mutex_lock(&TABLASEGMENTOS);
            //log_info(logger, "OBTUVO TABLA DE SEGMENTOS");
            pthread_mutex_lock(&PCBNEW);
            //log_info(logger, "NEW");
            pthread_mutex_lock(&PCBREADY);
            pcb = queue_pop(colaNew);
            //log_info(logger, "HIZO EL POP");
            pcb->estado = S_READY;
            time(&tiempoActual);
            //log_info(logger, "PASO EL TIME");
            pcb->llegada_ready = tiempoActual;
            //log_info(logger, "LLEGADA READY: %d", pcb->llegada_ready);
            list_add(listaReady, pcb);
            log_info(logger, "PLANIFICACION LARGO PLAZO: PID: %u - ESTADO ANTERIOR: S_NEW - ESTADO ACTUAL: S_READY.", pcb->pid);
            showLists();
            pthread_mutex_unlock(&PCBNEW);

            pthread_mutex_lock(&PLANICORTOPLAZO);
            pthread_mutex_unlock(&PCBENEJECUCION);

            break;

        case FINALIZED_PCB:
            pthread_mutex_lock(&SYNC_FIN);
            pthread_mutex_lock(&CONEXIONMEMORIA);
            mensajeAMemoria = REQ_FIN_PROCESO;
            pthread_mutex_unlock(&MENSAJEAMEMORIA);
            pthread_mutex_lock(&SYNC_FIN);
            pthread_mutex_lock(&LISTAEXIT);
            log_info(logger, "LISTA EXIT: %d", list_size(listaExit));
            list_remove_and_destroy_element(listaExit, 0, free);
            pthread_mutex_unlock(&LISTAEXIT);
            pthread_mutex_lock(&PLANICORTOPLAZO);
            pthread_mutex_unlock(&PCBENEJECUCION);
            pthread_mutex_unlock(&SYNC_FIN);
            
            break;
        }

        pthread_mutex_unlock(&MENSAJELARGOPLAZO);
    }
}

/*************************************
 *************************************
 PLANIFICADOR FIFO
 *************************************
 ************************************/
void planificacionFIFO()
{

    log_info(logger, "FIFO: Planificador FIFO iniciado.");

    t_pcb_kernel *pcb;

    while (1)
    {

        pthread_mutex_lock(&PCBENEJECUCION);

        if (!list_is_empty(listaReady) && list_size(listaExec)==0)
        {
            pcb = list_remove(listaReady, 0);

            pcb->estado = S_RUNNING;

            list_add(listaExec, pcb);
            log_info(logger, "FIFO: PID: %u - ESTADO ANTERIOR: S_READY - ESTADO ACTUAL: S_RUNNING.", pcb->pid);
            showLists();

            pthread_mutex_unlock(&MENSAJEACPU);
        }
        pthread_mutex_unlock(&PLANICORTOPLAZO);
    };
}

/*************************************
 *************************************
 PLANIFICADOR HRRN
 *************************************
 ************************************/
void planificacionHRRN()
{

    log_info(logger, "HRRN: Planificador HRRN iniciado.");

    t_pcb_kernel *pcb;

    while (1)
    {

        pthread_mutex_lock(&PCBENEJECUCION);
        log_debug(logger, "VOY A PLANIFICAR");

        if (!list_is_empty(listaReady) && list_size(listaExec)==0)
        {
            log_debug(logger, "VA A PASAR UNO A RUNNING (Hay %d)", list_size(listaReady));
            if(list_size(listaReady)==1){
                pcb = list_remove(listaReady, 0);
            }
            else{
                pcb = list_get_minimum(listaReady, (void *)comparar);
                buscarPcbxPid(listaReady, pcb->pid);
            }
            
            log_debug(logger, "EL PID ES (%d)", pcb->pid);

            log_debug(logger, "ELIMINO DE READY");

            pcb->estado = S_RUNNING;

            list_add(listaExec, pcb);
            log_info(logger, "HRRN: PID: %u - ESTADO ANTERIOR: S_READY - ESTADO ACTUAL: S_RUNNING.", pcb->pid);
            showLists();

            pthread_mutex_unlock(&MENSAJEACPU);
        }
        pthread_mutex_unlock(&PLANICORTOPLAZO);
    };
}

/*************************************
 *************************************
 FUNCIONES
 *************************************
 ************************************/

int determinarRecursos()
{
    int cantidadRecursos = 0;
    listaRecursos = list_create();

    char *token = strtok_r(kernel_config->recursos, "[, ]", &(kernel_config->recursos));
    char *tokenInstancia = strtok_r(kernel_config->instancias_recursos, "[, ]", &(kernel_config->instancias_recursos));

    while (token != NULL && tokenInstancia != NULL)
    {

        t_recurso *recurso = malloc(sizeof(t_recurso));
        recurso->recurso = strdup(token);
        recurso->instancias = atoi(tokenInstancia);
        recurso->colaBloqueados = queue_create();

        list_add(listaRecursos, recurso);

        log_info(logger, "MAIN: Inicializado recurso %s con %d instancias.", recurso->recurso, recurso->instancias);

        token = strtok_r(kernel_config->recursos, "[, ]", &(kernel_config->recursos));
        tokenInstancia = strtok_r(kernel_config->instancias_recursos, "[, ]", &(kernel_config->instancias_recursos));

        cantidadRecursos++;
    }

    log_debug(logger, "Cant Recursos: %d", cantidadRecursos);

    return cantidadRecursos;
}

void* comparar(t_pcb_kernel *pcb1, t_pcb_kernel *pcb2)
{
    time_t proxima_rafaga1, proxima_rafaga2;
    double response1, response2;
    proxima_rafaga1 = (time_t)pcb1->est_prox_rafaga;
    proxima_rafaga2 = (time_t)pcb2->est_prox_rafaga;

    log_debug(logger, "PROX RAFAGAS: %d vs %d", proxima_rafaga1, proxima_rafaga2);
    log_debug(logger, "WAIT TIMES: %d vs %d", tiempo2 - pcb1->llegada_ready, tiempo2 - pcb2->llegada_ready);

    response1 = (double)(tiempo2 - pcb1->llegada_ready + proxima_rafaga1) / proxima_rafaga1;
    response2 = (double)(tiempo2 - pcb2->llegada_ready + proxima_rafaga2) / proxima_rafaga2;

    log_debug(logger, "COMPARANDO: (%d) %f >= %f (%d) -> %d", pcb1->pid, response1, response2, pcb2->pid, response1 >= response2);

    if(response1 > response2)
        return pcb1;
    else
        return pcb2;
}

bool buscarFile(void *elemento)
{
    t_file_pcb *file_pcb = (t_file_pcb *)elemento;
    char *file = parametroChar;

    return strcmp(file_pcb->filename, file) == 0;
}

bool buscarRecurso(void *elemento)
{
    t_recurso *recurso = (t_recurso *)elemento;
    
    char *nombreRecurso = parametroChar;
    log_debug(logger, "(Buscar) %s (%d) = %s (%d)", recurso->recurso, strlen(recurso->recurso), nombreRecurso, strlen(nombreRecurso));
    int res = (strcmp(recurso->recurso, nombreRecurso) == 0);
    log_debug(logger, "Resultado: %d", res);
    return res;
}

t_pcb_kernel *buscarPcbxPid(t_list *lista, uint32_t pidBuscado)
{
    log_debug(logger, "Busco al PCB %d", pidBuscado);
    for (int i = 0; i < list_size(lista); i++)
    {
        t_pcb_kernel *pcb = list_get(lista, i);
        log_debug(logger, "Busco al PCB %d = %d ?", pidBuscado, pcb->pid);
        if (pcb->pid == pidBuscado)
        {
            list_remove(lista, i);
            return pcb;
        }
    }
    return NULL;
}

void finalizarPcb(t_pcb_kernel *pcb)
{
    //log_info(logger, "1");
    pcb = list_remove(listaExec, 0);
    time(&tiempo2);
    pcb->llegada_ready = tiempo2;
    actualizarProximaRafaga(pcb);
    pcb->estado = S_EXIT;
    //log_info(logger, "2");
    list_add(listaExit, pcb);
    //log_info(logger, "3");
    pthread_mutex_lock(&FINALIZARPCB);
    //log_info(logger, "4");

    char *key_sem = string_itoa(pcb->pid);
    //log_info(logger, "5");
    log_debug(logger, "Finalizando PID %s", key_sem);
    pthread_mutex_t* semaforo = dictionary_get(diccionarioSemaforos, key_sem);
    log_debug(logger, "Semaforo encontrado");
    int resultado_mutex = pthread_mutex_unlock(semaforo);

    log_debug(logger, "Mutex: %d", resultado_mutex);
    dictionary_remove(diccionarioSemaforos, key_sem);
    //log_info(logger, "6");

    pthread_mutex_lock(&MENSAJELARGOPLAZO);
    mensajePlanificador = FINALIZED_PCB;
    pthread_mutex_unlock(&PLANILARGOPLAZO);

    sem_post(&grado_programacion);

    free(key_sem);
}

void actualizarTablasDeSegmentos(t_list *tablaGlobal)
{
    actualizarListaPcb(tablaGlobal, listaReady);
    actualizarListaPcb(tablaGlobal, listaExec);
    actualizarListaPcb(tablaGlobal, listaBlock);
}

void actualizarListaPcb(t_list *tablaGlobal, t_list *listaPcb)
{
    tablaSegmMemoria *tablaSegmentos;
    t_pcb_kernel *pcb;
    for (int i = 0; i < list_size(listaPcb); i++)
    {
        for (int j = 0; j < list_size(tablaGlobal); j++)
        {
            tablaSegmentos = list_get(tablaGlobal, j);
            pcb = list_get(listaPcb, i);
            if (tablaSegmentos->pid == pcb->pid)
            {
                list_clean_and_destroy_elements(pcb->tabla_segmentos, free);
                list_add_all(pcb->tabla_segmentos, tablaSegmentos->tabla);
                break;
            }
        }
    }
}

void actualizarProximaRafaga(t_pcb_kernel *pcb)
{
    log_debug(logger, "Rafaga para PID (%d)", pcb->pid);
    int rafagaActual = (int)(tiempo2 - tiempo1);
    log_debug(logger, "Rafaga actual: %d", rafagaActual);
    int rafagaAnterior = pcb->est_prox_rafaga;
    log_debug(logger, "Rafaga anterior: %d", rafagaAnterior);
    pcb->est_prox_rafaga = rafagaActual*kernel_config->hrrn_alfa + rafagaAnterior * (1-kernel_config->hrrn_alfa);
    log_debug(logger, "Est siguiente: %f", pcb->est_prox_rafaga);
}

void desbloquear_pcb(int pid)
{
    t_pcb_kernel *pcb;
    pcb = buscarPcbxPid(listaBlock, pid);
    pcb->estado = S_READY;
    pcb->program_counter += 1;

    list_add(listaReady, pcb);
    pthread_mutex_lock(&PLANICORTOPLAZO);
    pthread_mutex_unlock(&PCBENEJECUCION);

    char* listaPCBs = listaPCBATexto(listaReady);

    log_info(logger, "CONEXION FILESYSTEM: PID: %u - ESTADO ANTERIOR: S_BLOCKED - ESTADO ACTUAL: S_READY.", pcb->pid);
    log_info(logger, "Cola Ready %s: %s", kernel_config->planificador, listaPCBs);

    free(listaPCBs);
    showLists();
}

/*************************************
 *************************************
 DESEREALIZACIONES MENSAJES DE CPU
 *************************************
 ************************************/

t_pcb_cpu *deserializar_contexto_y_char(int socket)
{

    t_pcb_cpu *contexto = deserializarPCB(socket);

    int size_string;

    recv(socket, &size_string, sizeof(int), MSG_WAITALL);

    char *string_received = malloc(size_string);

    recv(socket, string_received, size_string, MSG_WAITALL);

    strcpy(parametroChar, string_received);

    return contexto;





    /*
    int unSize;
    log_debug(logger, "PCB");
    recv(socket, &unSize, sizeof(int), MSG_WAITALL);
    log_debug(logger, "RECV: %d", unSize);

    char* unString = string_new();
    log_debug(logger, "LENGTH ES: %d (%s)", strlen(unString), unString);
    log_debug(logger, "MALLOC");

    int testRecv = recv(socket, unString, unSize, MSG_WAITALL);

    log_debug(logger, "TEST1: %d", testRecv);
    unString[unSize] = '\0';
    strcpy(parametroChar, unString);
    log_debug(logger, "TEST2: %s (%d) = %s (%d)", parametroChar, string_length(parametroChar), unString, string_length(unString));

    return contexto;
    */
}

t_pcb_cpu *deserializar_contexto_e_int(int socket)
{

    t_pcb_cpu *contexto = deserializarPCB(socket);

    int aux;

    recv(socket, &aux, sizeof(int), MSG_WAITALL);
    
    log_debug(logger, "RECIBIDO: %d", aux);


    parametroInt1 = aux;

    log_debug(logger, "PARAM: %d", parametroInt1);

    return contexto;
}

t_pcb_cpu *deserializar_contexto_char_e_int(int socket, char *string, int *entero)
{

    t_pcb_cpu *contexto = deserializarPCB(socket);

    recv(socket, entero, sizeof(int), MSG_WAITALL);

    int size_string;

    recv(socket, &size_string, sizeof(int), MSG_WAITALL);

    char *string_received = malloc(size_string);

    recv(socket, string_received, size_string, MSG_WAITALL);

    strcpy(string, string_received);

    return contexto;
}

t_pcb_cpu *deserializar_contexto_char_y_2_int(int socket, char *string, int *entero_a, int *entero_b)
{

    t_pcb_cpu *contexto = deserializarPCB(socket);

    recv(socket, entero_a, sizeof(int), MSG_WAITALL);
    recv(socket, entero_b, sizeof(int), MSG_WAITALL);

    int size_string;

    recv(socket, &size_string, sizeof(int), MSG_WAITALL);

    char *string_received = malloc(size_string);

    recv(socket, string_received, size_string, MSG_WAITALL);

    strcpy(string, string_received);

    return contexto;
}

t_pcb_cpu *deserializar_contexto_int_e_int(int socket, int *entero_a, int *entero_b)
{

    t_pcb_cpu *contexto = deserializarPCB(socket);

    recv(socket, entero_a, sizeof(int), MSG_WAITALL);
    recv(socket, entero_b, sizeof(int), MSG_WAITALL);

    return contexto;
}

void showLists(){
    log_debug(logger, "RE: (%d) EX: (%d) BL: (%d)", list_size(listaReady), list_size(listaExec), list_size(listaBlock));

    /*for(int i=0; i<list_size(listaReady); i++){
        t_pcb_kernel* unPCB = list_get(listaReady, i);
        log_debug(logger, "(%d) en READY", unPCB->pid);
    }

    for(int i=0; i<list_size(listaBlock); i++){
        t_pcb_kernel* unPCB = list_get(listaBlock, i);
        log_debug(logger, "(%d) en BLOCK", unPCB->pid);
    }

    for(int i=0; i<list_size(listaExec); i++){
        t_pcb_kernel* unPCB = list_get(listaExec, i);
        log_debug(logger, "(%d) en EXEC", unPCB->pid);
    }*/
}

/*************************************
 *************************************
 DESEREALIZACIONES MENSAJES DE MEMORIA
 *************************************
 ************************************/