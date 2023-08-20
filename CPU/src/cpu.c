#include "cpu.h"

t_log* logger;
t_dictionary* dict_instrucciones;
t_dictionary* dict_registros;
t_cpu_config* config;
t_pcb_cpu* pcb_actual;
int socket_memoria;
int socket_kernel;

int continuar_ejecucion;

int main(int argc, char ** argv){
    logger = log_create("./cfg/cpu.log", "CPU", true, LOG_LEVEL_INFO);
    dict_instrucciones = inicializar_diccionario();
    dict_registros = init_dict_registros();
    config = inicializar_config(argv[1]);
    
    int socket_servidor = inicializar_servidor(config -> puerto_escucha);
    pthread_t hiloKernel;
    pthread_create(&hiloKernel, NULL, conexionKernel, socket_servidor);
    //log_info(logger, "CREO EL HILO DE KERNEL");
    pthread_t hiloMemoria;
    pthread_create(&hiloMemoria, NULL, conexionMemoria, P_FILESYSTEM);
    pthread_join(hiloMemoria, NULL);
    pthread_join(hiloKernel, NULL);   
    /*
    continuar_ejecucion = 1;
    test_serializacion_kernel();
    ciclo_ejec();
    */
    return 0;
}

void reinicializarPCB(){
    pcb_actual = malloc(sizeof(t_pcb_cpu));
    pcb_actual->archivos_abiertos = list_create();
    pcb_actual->instrucciones = list_create();
    pcb_actual->pid = 0;
    pcb_actual->program_counter = 0;
    pcb_actual->registros = malloc(sizeof(t_registros_cpu));
    pcb_actual->tabla_segmentos = list_create;
}

void conexionKernel(int socket_servidor){
    //log_info(logger, "ESPERANDO AL KERNEL");
    socket_kernel = accept(socket_servidor, NULL, NULL);
    handshake_servidor(socket_kernel, P_KERNEL);
    //log_info(logger, "CONEXION EXITOSA CON KERNEL");
    while(1){
        reinicializarPCB();
        continuar_ejecucion = 1;
        int codigo;
        pcb_actual = malloc(sizeof(t_pcb_cpu));
        //log_debug(logger, "ESPERO UN PCB");
        recv(socket_kernel, &codigo, sizeof(int), MSG_WAITALL);
        //log_info(logger, "RECIBIO EL CODIGO");
        pcb_actual = deserializarPCB(socket_kernel);
        //log_info(logger, "DESERIALIZO PCB PID: %d", pcb_actual -> pid);
        ciclo_ejec(pcb_actual);
        free(pcb_actual);
    }
}


void conexionMemoria(){
    //log_info(logger, "CONEXION MEMORIA: Inicializado hilo de conexion con Memoria.");

    char *ipMemoria = config->ip_memoria;
    char *puertoMemoria = config->puerto_memoria;

    socket_memoria = inicializar_cliente(ipMemoria, puertoMemoria);

    while(socket_memoria == -1){
        //log_info(logger, "INTENTANDO CONECTAR A MEMORIA");
        sleep(5);
        socket_memoria = inicializar_cliente(ipMemoria, puertoMemoria);
    }

    int response = handshake_cliente(socket_memoria, P_CPU);


    if(response != 0)
        log_error(logger, "NO ES POSIBLE CONECTARSE A MEMORIA");

    //log_info(logger, "CONEXION MEMORIA: Inicializado cliente de conexion con Memoria, socket: %d.", socket_memoria);
}

/*------------------------------------------
--------------------------------------------
-------------- CONFIG & KERNEL -------------
--------------------------------------------
-------------------------------------------*/

t_cpu_config* inicializar_config(char* path){
    t_config* raw;
    raw = config_create(path);
    config = malloc(sizeof(t_cpu_config));
    config->ip_memoria = buscarEnConfig(raw, "IP_MEMORIA");
    config->puerto_escucha = buscarEnConfig(raw, "PUERTO_ESCUCHA");
    config->puerto_memoria = buscarEnConfig(raw, "PUERTO_MEMORIA");
    config->tam_max_segmento = textoAuint32(buscarEnConfig(raw, "TAM_MAX_SEGMENTO"));
    config->retardo_instruccion = textoAuint32(buscarEnConfig(raw, "RETARDO_INSTRUCCION"));

    return config;
}


/*------------------------------------------
--------------------------------------------
--------------- INSTRUCCIONES --------------
--------------------------------------------
-------------------------------------------*/


int escribir_registro(char* registro, char* valor){
    t_registros_cpu* registros = pcb_actual->registros;
    //Obtengo el id de registro segun el diccionario
    idRegistro key_registro = (idRegistro) dictionary_get(dict_registros, registro);
    ////log_debug(logger, "ENUM REGISTRO: %d", key_registro);
    switch(key_registro){
        case AX: 
            memcpy(registros->ax, valor, 4);
            break;
        case BX:
            memcpy(registros->bx, valor, 4);
            break;
        case CX:
            memcpy(registros->cx, valor, 4);
            break;
        case DX:
            memcpy(registros->dx, valor, 4);
            break;
        case EAX:
            memcpy(registros->eax, valor, 8);
            break;
        case EBX:
            memcpy(registros->ebx, valor, 8);
            break;
        case ECX:
            memcpy(registros->ecx, valor, 8);
            break;
        case EDX:
            memcpy(registros->edx, valor, 8);
            break;
        case RAX:
            memcpy(registros->rax, valor, 16);
            break;
        case RBX:
            memcpy(registros->rbx, valor, 16);
            break;
        case RCX:
            memcpy(registros->rcx, valor, 16);
            break;
        case RDX:
            memcpy(registros->rdx, valor, 16);
            break;
        default:
            log_error(logger, "ERROR REGISTRO INVALIDO");
            int tamanio;
            void* pcb = serializar_contexto(ERR_INVALID_REG, &tamanio);
            enviar_pcb(pcb, tamanio);
            return -1;
    }
    return 0;
}


int leer_registro(char* registro, char* valor){
    t_registros_cpu* registros = pcb_actual -> registros;
    idRegistro key_registro = (idRegistro) dictionary_get(dict_registros, registro);
    switch(key_registro){
        case AX: 
            memcpy(valor, registros->ax, 4);
            break;
        case BX:
            memcpy(valor, registros->bx, 4);
            break;
        case CX:
            memcpy(valor, registros->cx, 4);
            break;
        case DX:
            memcpy(valor, registros->dx, 4);
            break;
        case EAX:
            memcpy(valor, registros->eax, 8);
            break;
        case EBX:
            memcpy(valor, registros->ebx, 8);
            break;
        case ECX:
            memcpy(valor, registros->ecx, 8);
            break;
        case EDX:
            memcpy(valor, registros->edx, 8);
            break;
        case RAX:
            memcpy(valor, registros->rax, 16);
            break;
        case RBX:
            memcpy(valor, registros->rbx, 16);
            break;
        case RCX:
            memcpy(valor, registros->rcx, 16);
            break;
        case RDX:
            memcpy(valor, registros->rdx, 16);
            break;
        default:
            log_error(logger, "ERROR REGISTRO INVALIDO");
            int tamanio;
            void* pcb = serializar_contexto(ERR_INVALID_REG, &tamanio);
            enviar_pcb(pcb, tamanio);
            return -1;
    }
    return 0;
}

int exe_SET(INSTRUCCIONES* instr){
    milisleep(config->retardo_instruccion);
    char* registro = instr->parametro1;
    char* valor = instr->parametro2;
    int escribir_bool = escribir_registro(registro, valor);
    //log_info(logger, "Registro %s escrito", registro);
    if(escribir_bool == -1){
        return -1; //La PCB la devuelve escribir registro
    }
    pcb_actual->program_counter ++;
    return 0;
}


int size_of_registro(char* registro){
    idRegistro key_registro = (idRegistro) dictionary_get(dict_registros, registro);
    if(key_registro < 4)
        return 4;
    else if(key_registro < 8)
        return 8;
    else if(key_registro < 12)
        return 16;
    log_error(logger, "ERROR REGISTRO INVALIDO");
    int tamanio;
    void* pcb = serializar_contexto(ERR_INVALID_REG, &tamanio);
    enviar_pcb(pcb, tamanio);
    return -1;
}

int obtener_dir_fisica(char* dir_log, entradaTablaSegmentos* tabla){
    int dir_logica_int = atoi(dir_log);
    //log_info(logger, "DIR LOGICA: %d", dir_logica_int);
    int dir_fisica;
    dir_fisica = mmu(dir_logica_int, tabla);
    //log_info(logger, "Datos segm: %d %d %d", tabla->idSegmento, tabla->dirBase, tabla->size);
    if(tabla -> idSegmento == -1){
        log_error(logger, "ERROR SEGMENTO INEXISTENTE");
        int tamanio;
        void* pcb = serializar_contexto(ERR_INVALID_SEGMENT, &tamanio);
        enviar_pcb(pcb, tamanio);
        return -1; 
    }
    //log_info(logger, "DIR FISICA: %d", dir_fisica);
    return dir_fisica;
}

int obtener_dir_fisica_con_registro(char* dir_log, char* registro, entradaTablaSegmentos* tabla){
    int dir_fisica = obtener_dir_fisica(dir_log, tabla); //Pasa tabla por referencia
    if(dir_fisica == -1)
        return -1;
    int desplazamiento = dir_fisica - tabla->dirBase;
    int tamanio_registro = size_of_registro(registro);
    if(tamanio_registro == -1)
        return -1; // la PCB la devuelve size_of_registro
    if(desplazamiento + tamanio_registro > tabla -> size){
        log_error_segmentation_fault(tabla->idSegmento, desplazamiento, tabla->size);
        int tamanio;
        void* pcb = serializar_contexto(ERR_SEGMENTATION_FAULT, &tamanio);
        enviar_pcb(pcb, tamanio);
        continuar_ejecucion = 0;
        return -1; 
    }  
    return dir_fisica;
}


// DUDA!!! --> Hay que notificar al kernel la actualizacion de la PCB??

int exe_MOV_IN(INSTRUCCIONES* instr){
    //log_debug(logger, "ENTRA A MOV_IN");
    char* registro = instr->parametro1;
    char* dir_log = instr->parametro2;
    entradaTablaSegmentos* tabla;
    int dir_fisica = obtener_dir_fisica_con_registro(dir_log, registro, tabla); //Pasa tabla por referencia
    if(dir_fisica == -1) // Error en la obtencion de la dir fisica
        return -1; //La PCB la devuelve obtener_dir_fisica_con_registro
    
    int tamanio_buffer;
                    
    void* buffer = serializar_3_int(REQ_LECTURA, pcb_actual -> pid,dir_fisica, size_of_registro(registro), &tamanio_buffer); 

    int response = enviar_y_recibir_codigo(socket_memoria, buffer, tamanio_buffer);
    //log_debug(logger, "RESPONSE DE MOV IN: %d", response);
    if(response == ERR_LECTURA){
        log_error(logger, "ERROR LECTURA MEMORIA");
        int tamanio;
        void* pcb = serializar_contexto(ERR_READ_MEMORY, &tamanio);
        enviar_pcb(pcb, tamanio);
        return -1; 
    }
    //log_debug(logger, "Respuesta de memoria: %d", response);
    //Deserializar
    int tamanio_valor_a_escribir;
    recv(socket_memoria, &tamanio_valor_a_escribir, sizeof(int), MSG_WAITALL);
    //log_debug(logger, "Size: %d", tamanio_valor_a_escribir);
    
    char* valor = malloc(tamanio_valor_a_escribir);
    recv(socket_memoria, valor, tamanio_valor_a_escribir, MSG_WAITALL);
    valor[tamanio_valor_a_escribir] = '\0';
    //log_debug(logger, "Contenido: %s", valor);
    


    int escribir_bool = escribir_registro(registro, valor);
    if(escribir_bool == -1)
        return -1;// La PCB la devuelve escribir_registro
    log_acceso_memoria("LEER", tabla->idSegmento, dir_fisica, valor);
    free(valor);
    pcb_actual->program_counter ++;
    
    return 0;
}

int exe_MOV_OUT(INSTRUCCIONES* instr){
    char* dir_log = instr->parametro1;
    char* registro = instr->parametro2;
    entradaTablaSegmentos* tabla;
    ////log_debug(logger, "PRE OBTENER DIR FISICA");
    int dir_fisica = obtener_dir_fisica_con_registro(dir_log, registro, tabla);
    //log_debug(logger, "DIR FISICA %d", dir_fisica);
    if(dir_fisica == -1) // Error en la obtencion de la dir fisica
        return -1;
    
    int tamanio_reg = size_of_registro(registro); //No validamos porque lo hace obtener_dir_fisica_con_registro
    ////log_debug(logger, "TAMANIO REGISTRO %d", tamanio_reg);
    char* valor = malloc(tamanio_reg);
    int leer_bool = leer_registro(registro, valor);
    valor[tamanio_reg] = '\0';
    //log_debug(logger, "LEER REGISTRO %s", valor);
    if(leer_bool == -1)
        return -1;//La PCB la devuelve leer_registro

    int tamanio_buffer;

    void* buffer = serializar_char_y_2_int(REQ_ESCRITURA , pcb_actual -> pid ,dir_fisica, valor, tamanio_reg, &tamanio_buffer);
    //log_debug(logger, "LOG %d, %d, %s, %d TAMANIO ENVIADO %d", pcb_actual -> pid, dir_fisica, valor, tamanio_reg, tamanio_buffer);

    int response = enviar_y_recibir_codigo(socket_memoria, buffer, tamanio_buffer);
    //log_debug(logger, "RESPONSE %d", response);



    if(response == ERR_ESCRITURA){
        log_error(logger, "ERROR ESCRITURA MEMORIA");
        int tamanio;    
        void* pcb = serializar_contexto(ERR_WRITE_MEMORY, &tamanio);
        enviar_pcb(pcb, tamanio);
        return -1; 
    }
    pcb_actual->program_counter ++;

    return 0;
}

int exe_IO(INSTRUCCIONES* instr){
    char* tiempo = instr->parametro1;
    int tamanio;
    void* pcb = serializar_contexto_e_int(tiempo, REQ_I_O, &tamanio);
    int resultado = enviar_pcb(pcb, tamanio);
    continuar_ejecucion = 0;
    return resultado;
}

int archivo_esta_abierto(char* nombre){

    for(int i=0; i<list_size(pcb_actual->archivos_abiertos); i++){
        t_file_pcb* archivo = list_get(pcb_actual->archivos_abiertos, i);

        //log_info(logger, "Comparando nombres (%s)=(%s)?", nombre, archivo->filename);
        if(strcmp(nombre, archivo->filename) == 0){
            //log_info(logger, "ENCONTRADO (%s)=(%s) en (%d)", nombre, archivo->filename, i);
            return 1;
        }       
    }
    //log_info(logger, "ARCHIVO '%s' NO ESTA ABIERTO", nombre);
    return 0;
}

bool raise_error_file_not_open(char* nombre_archivo){
    if(!archivo_esta_abierto(nombre_archivo)){
        log_error(logger, "EL ARCHIVO %s NO ESTABA ABIERTO", nombre_archivo);
        int tamanio;
        void* pcb = serializar_contexto(ERR_FILE_NOT_OPEN, &tamanio);
        enviar_pcb(pcb, tamanio);
        return true;
    }
    return false;
}

int exe_F_OPEN(INSTRUCCIONES* instr){
    char* nombre_archivo = instr->parametro1;
    //Si el archivo ya fue abierto
    /*if(archivo_esta_abierto(nombre_archivo)){
        //Vuelvo a insertarlo
        log_error(logger, "EL ARCHIVO %s YA ESTABA ABIERTO", nombre_archivo);
        int tamanio;
        void* pcb = serializar_contexto(ERR_FILE_OPENED, &tamanio);
        enviar_pcb(pcb, tamanio);
        continuar_ejecucion = 0;
        return -1;
    }*/

    //Serializo la PCB, el nombre del archivo y envio el Request
    int tamanio_pcb_y_arch;
    void* pcb_y_arch = serializar_contexto_y_char(nombre_archivo, REQ_OPEN_FILE, &tamanio_pcb_y_arch);
    int resultado = enviar_pcb(pcb_y_arch, tamanio_pcb_y_arch);
    continuar_ejecucion = 0;
    return resultado;
}

int exe_F_CLOSE(INSTRUCCIONES* instr){
    char* nombre_archivo = instr->parametro1;
    t_list* archivos_abiertos = pcb_actual -> archivos_abiertos;
    //Si el archivo no estaba abierto
    continuar_ejecucion = 0;
    if(raise_error_file_not_open(nombre_archivo))
        return -1;
    //Si el archivo fue abierto, envio REQ para que lo cierre
    int tamanio;
    void* pcb = serializar_contexto_y_char(nombre_archivo ,REQ_CLOSE_FILE, &tamanio);
    int resultado = enviar_pcb(pcb, tamanio);
    return resultado;
}

int exe_F_SEEK(INSTRUCCIONES* instr){
    char* nombre_archivo = instr->parametro1;
    char* posicion = instr->parametro2;
    //Si el archivo no estaba abierto
    if(raise_error_file_not_open(nombre_archivo))
        return -1;
    //Si el archivo esta abierto, envio al Kernel la solicitud
    int tamanio; 
    void* pcb = serializar_contexto_char_e_int(nombre_archivo, posicion, REQ_SEEK_FILE, &tamanio);
    int resultado = enviar_pcb(pcb, tamanio);
    continuar_ejecucion = 0;
    return resultado;
}

int exe_F_READ(INSTRUCCIONES* instr){
    char* nombre_archivo = instr->parametro1;
    char* dir_log = instr->parametro2;
    char* cant_bytes = instr->parametro3;
    //Si el archivo no estaba abierto
    if(raise_error_file_not_open(nombre_archivo))
        return -1;
    entradaTablaSegmentos* tabla; //No utilizada por file_system
    int dir_fisica = obtener_dir_fisica(dir_log, tabla);
    if(dir_fisica == -1)
        return -1;
    //Si el archivo esta abierto, envio al Kernel la solicitud
    int tamanio; 
    void* pcb = serializar_contexto_char_y_2_int(nombre_archivo, dir_fisica, cant_bytes, REQ_READ_FILE, &tamanio);
    int resultado = enviar_pcb(pcb, tamanio);
    continuar_ejecucion = 0;
    return resultado;
}

int exe_F_WRITE(INSTRUCCIONES* instr){
    char* nombre_archivo = instr->parametro1;
    char* dir_log = instr->parametro2;
    char* cant_bytes = instr->parametro3;
    //Si el archivo no estaba abierto
    if(raise_error_file_not_open(nombre_archivo))
        return -1;
    log_debug(logger, "TEST 0");
    entradaTablaSegmentos* tabla; //No utilizada por file_system
    int dir_fisica = obtener_dir_fisica(dir_log, tabla);
    log_debug(logger, "TEST 1");
    if(dir_fisica == -1)
        return -1;  
     //Si el archivo esta abierto, envio al Kernel la solicitud
    int tamanio; 
    void* pcb = serializar_contexto_char_y_2_int(nombre_archivo, dir_fisica, cant_bytes, REQ_WRITE_FILE, &tamanio);
    log_debug(logger, "TEST 2: %d", tamanio);
    int resultado = enviar_pcb(pcb, tamanio);
    continuar_ejecucion = 0;
    return resultado;
}

int exe_F_TRUNCATE(INSTRUCCIONES* instr){
    char* nombre_archivo = instr->parametro1;
    char* size = instr->parametro2;
    //Si el archivo no estaba abierto
    if(raise_error_file_not_open(nombre_archivo))
        return -1;
    //Si el archivo esta abierto, envio al Kernel la solicitud
    int tamanio; 
    void* pcb = serializar_contexto_char_e_int(nombre_archivo, size, REQ_TRUNCATE_FILE, &tamanio);
    int resultado = enviar_pcb(pcb, tamanio);
    continuar_ejecucion = 0;
    return resultado;
}

int exe_WAIT(INSTRUCCIONES* instr){
    ////log_debug(logger, "ENTRO a WAIT");
    char* recurso = string_new();
    strcpy(recurso, instr->parametro1);
    int tamanio;
    void* pcb = serializar_contexto_y_char(recurso ,REQ_WAIT, &tamanio);
    int resultado = enviar_pcb(pcb, tamanio);
    continuar_ejecucion = 0;
    ////log_debug(logger, "FIN WAIT");
    return resultado;
}

int exe_SIGNAL(INSTRUCCIONES* instr){
    char* recurso = string_new();
    strcpy(recurso, instr->parametro1);
    int tamanio;
    void* pcb = serializar_contexto_y_char(recurso ,REQ_SIGNAL, &tamanio);
    int resultado = enviar_pcb(pcb, tamanio);
    continuar_ejecucion = 0;
    return resultado;
}

int exe_CREATE_SEGMENT(INSTRUCCIONES* instr){
    char* id = instr->parametro1;
    char* size = instr->parametro2;
    int size_int = atoi(instr->parametro2);
    int tamanio;
    int resultado;
    if(size_int>config->tam_max_segmento){
        log_error(logger, "SIZE DEL SEGMENTO MAYOR AL MAXIMO (%d > %d)", size_int, config->tam_max_segmento);
        void* pcb = serializar_contexto(ERR_INVALID_SEGMENT, &tamanio);
        resultado = enviar_pcb(pcb, tamanio);
    }
    else{
        void* pcb = serializar_contexto_int_e_int(id, size, REQ_CREATE_SEGMENT, &tamanio);
        resultado = enviar_pcb(pcb, tamanio);
    }
    continuar_ejecucion = 0;
    return resultado;
}

int exe_DELETE_SEGMENT(INSTRUCCIONES* instr){
    char* id = instr->parametro1;
    int tamanio;
    void* pcb = serializar_contexto_e_int(id, REQ_DELETE_SEGMENT, &tamanio);
    int resultado = enviar_pcb(pcb, tamanio);
    continuar_ejecucion = 0;
    return resultado;
}

int exe_YIELD(INSTRUCCIONES* instr){
    int tamanio;
    ////log_debug(logger, "EJECUTANDO YIELD");
    void* pcb = serializar_contexto(REQ_YIELD, &tamanio);
    ////log_debug(logger, "CONTEXTO SERIALIZADO");
    int resultado = enviar_pcb(pcb, tamanio);
    ////log_debug(logger, "ENVIANDO: %d", resultado);
    continuar_ejecucion = 0;
    return resultado;
}

int exe_EXIT(INSTRUCCIONES* instr){
    int tamanio;
    void* pcb = serializar_contexto(REQ_EXIT, &tamanio);
    int resultado = enviar_pcb(pcb, tamanio);
    continuar_ejecucion = 0;
    return resultado;
}

/*------------------------------------------
--------------------------------------------
--------------- DICCIONARIOS ---------------
--------------------------------------------
-------------------------------------------*/

t_dictionary* inicializar_diccionario(){
    t_dictionary* dict = dictionary_create();

    dictionary_put(dict, "SET", *exe_SET);
    dictionary_put(dict, "MOV_IN", *exe_MOV_IN);
    dictionary_put(dict, "MOV_OUT", *exe_MOV_OUT);
    dictionary_put(dict, "I/O", *exe_IO);
    dictionary_put(dict, "F_OPEN", *exe_F_OPEN);
    dictionary_put(dict, "F_CLOSE", *exe_F_CLOSE);
    dictionary_put(dict, "F_SEEK", *exe_F_SEEK);
    dictionary_put(dict, "F_READ", *exe_F_READ);
    dictionary_put(dict, "F_WRITE", *exe_F_WRITE);
    dictionary_put(dict, "F_TRUNCATE", *exe_F_TRUNCATE);
    dictionary_put(dict, "WAIT", *exe_WAIT);
    dictionary_put(dict, "SIGNAL", *exe_SIGNAL);
    dictionary_put(dict, "CREATE_SEGMENT", *exe_CREATE_SEGMENT);
    dictionary_put(dict, "DELETE_SEGMENT", *exe_DELETE_SEGMENT);
    dictionary_put(dict, "YIELD", *exe_YIELD);
    dictionary_put(dict, "EXIT", *exe_EXIT);

    return dict;
}

t_dictionary* init_dict_registros(){
    t_dictionary* dict = dictionary_create();

    dictionary_put(dict, "AX", (int*)AX);
    dictionary_put(dict, "BX", (int*)BX);
    dictionary_put(dict, "CX", (int*)CX);
    dictionary_put(dict, "DX", (int*)DX);
    dictionary_put(dict, "EAX", (int*)EAX);
    dictionary_put(dict, "EBX", (int*)EBX);
    dictionary_put(dict, "ECX", (int*)ECX);
    dictionary_put(dict, "EDX", (int*)EDX);
    dictionary_put(dict, "RAX", (int*)RAX);
    dictionary_put(dict, "RBX", (int*)RBX);
    dictionary_put(dict, "RCX", (int*)RCX);
    dictionary_put(dict, "RDX", (int*)RDX);

    return dict;
}
void ejecutar(void* (func)(INSTRUCCIONES*), INSTRUCCIONES* instr){
    //log_debug(logger, "EJECUTANDO");
    (func)(instr);
}


/*------------------------------------------
--------------------------------------------
--------------- LOGS  ----------------------
--------------------------------------------
-------------------------------------------*/

void log_error_segmentation_fault(int segmento, int desplazamiento, int tamanio){
    log_error(logger, 
    "PID: %d - Error SEG_FAULT- Segmento: %d - Offset: %d - Tamaño: %d",
    pcb_actual->pid, segmento, desplazamiento, tamanio);
}

void log_acceso_memoria(char* accion, int segmento, int dir_fisica, char* valor){
    /*log_info(logger, 
    "PID: %d - Acción: %s - Segmento: %d - Dirección Física: %d - Valor: %s", 
    pcb_actual->pid, accion, segmento, dir_fisica, valor);*/
}


int ciclo_ejec(){
    while(continuar_ejecucion){
        INSTRUCCIONES* instr = list_get(pcb_actual->instrucciones, pcb_actual->program_counter);

        int size = list_size(pcb_actual->instrucciones);

        //log_debug(logger, "TAMANIO STRING INSTRUCCION: %d %s ", string_length(instr->comando), instr->comando);
        log_info(logger, "PID: %d - Ejecutando: %s - %s %s %s", pcb_actual->pid ,instr->comando,
                instr->parametro1 ? instr->parametro1 : "",
                instr->parametro2 ? instr->parametro2 : "",
                instr->parametro3 ? instr->parametro3 : ""
        );
        
        dict_instrucciones = inicializar_diccionario();
        //decode:
        //log_debug(logger, "Instr: %s (%d)", instr->comando, strlen(instr->comando));
        int test = dictionary_get(dict_instrucciones, instr->comando) != NULL;
        //log_debug(logger, "KEY ES CONOCIDA? -> %d", test);
        log_debug(logger, "PC: %d", pcb_actual->program_counter);
        ejecutar(dictionary_get(dict_instrucciones, instr->comando), instr);
        log_debug(logger, "ENCONTRADO EN DICCIONARIO");
        if(pcb_actual->program_counter == size){
            ////log_debug(logger, "TERMINO LA ULTIMA INSTRUCCION");
            continuar_ejecucion = 0;
        }
            
    }
    
    return 0;
}

/*------------------------------------------
--------------------------------------------
--------------- SERIALIZACION --------------
--------------------------------------------
-------------------------------------------*/


void* serializar_contexto(e_codigo_cpu_kernel codigo, int* tam_total){
    int tam_contexto;
    void* pcb_serializada = serializarPCB(pcb_actual, &tam_contexto);
    void* buffer = malloc(tam_contexto + sizeof(int));
    int offset = 0;
    memcpy(buffer + offset, &codigo, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, pcb_serializada, tam_contexto);
    offset += tam_contexto;
    (*tam_total) = offset;
    return buffer;
}

void* serializar_contexto_y_char(char* string, e_codigo_cpu_kernel codigo, int* tam_total){
    int tam_contexto;
    int tam_string = strlen(string);
    //printf("--SIZE STRING = %d--", tam_string);
    void* contexto = serializar_contexto(codigo, &tam_contexto);
    void* buffer = malloc(tam_contexto + sizeof(int) + tam_string);

    int offset = 0;

    memcpy(buffer + offset, contexto, tam_contexto);
    offset += tam_contexto;
    memcpy(buffer + offset, &tam_string, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, string, tam_string);

    (*tam_total) = tam_contexto + sizeof(int) + tam_string;
    return buffer;

}
void* serializar_contexto_e_int(char* entero, e_codigo_cpu_kernel codigo, int* tam_total){
    int tam_contexto;
    int entero_int = atoi(entero);
    void* contexto = serializar_contexto(codigo, &tam_contexto);
    void* buffer = malloc(tam_contexto + sizeof(int));

    memcpy(buffer, contexto, tam_contexto);
    memcpy(buffer + tam_contexto, &entero_int, sizeof(int));
    (*tam_total) = tam_contexto + sizeof(int);

    return buffer;
}

void* serializar_contexto_char_e_int(char* string, char* entero, e_codigo_cpu_kernel codigo, int* tam_total){
    int tam_contexto;
    int tam_string = string_length(string);
    int entero_int = atoi(entero);
    void* contexto = serializar_contexto(codigo, &tam_contexto);
    void* buffer = malloc(tam_contexto + 2*sizeof(int) + tam_string);

    int offset = 0;

    memcpy(buffer + offset, contexto, tam_contexto);
    offset += tam_contexto;
    memcpy(buffer + offset, &entero_int, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &tam_string, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, string, tam_string);

    (*tam_total) = tam_contexto + 2*sizeof(int) + tam_string;
    return buffer;
}

void* serializar_contexto_char_y_2_int(char* string, int entero_a, char* entero_b, e_codigo_cpu_kernel codigo, int* tam_total){
    int tam_contexto;
    int tam_string = string_length(string);
    int entero_int = atoi(entero_b);
    void* contexto = serializar_contexto(codigo, &tam_contexto);
    void* buffer = malloc(tam_contexto + 3*sizeof(int) + tam_string);

    int offset = 0;

    memcpy(buffer + offset, contexto, tam_contexto);
    offset += tam_contexto;
    memcpy(buffer + offset, &entero_a, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &entero_int, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &tam_string, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, string, tam_string);

    (*tam_total) = tam_contexto + 3*sizeof(int) + tam_string;
    return buffer;

}


void* serializar_contexto_int_e_int(char* entero_a, char* entero_b, e_codigo_cpu_kernel codigo, int* tam_total){
    int tam_contexto;
    int entero_int_a = atoi(entero_a);
    int entero_int_b = atoi(entero_b);
    void* contexto = serializar_contexto(codigo, &tam_contexto);
    void* buffer = malloc(tam_contexto + 2*sizeof(int));

    int offset = 0;

    memcpy(buffer + offset, contexto, tam_contexto);
    offset += tam_contexto;
    memcpy(buffer + offset, &entero_int_a, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &entero_int_b, sizeof(int));

    (*tam_total) = tam_contexto + 2*sizeof(int) ;
    return buffer;
}


int enviar_pcb(void* buffer, int tamanio){
    if(send(socket_kernel, buffer, tamanio, NULL) == -1){
        log_error(logger, "ERROR AL ENVIAR EL CONTEXTO DE EJECUCION");
        return -1;
    }
    return 0;
}

/*------------------------------------------
--------------------------------------------
------------------- MMU --------------------
--------------------------------------------
-------------------------------------------*/

entradaTablaSegmentos* buscarSegmentoPorID(t_list* tabla, int id){
    entradaTablaSegmentos* entrada = malloc(sizeof(entradaTablaSegmentos));
    for(int i=0; i<list_size(tabla); i++){
        entrada = list_get(tabla, i);
        if(entrada->idSegmento == id)
            return entrada;
    }

    log_error(logger, "Segmento con ID %d no encontrado", id);
    entrada->idSegmento = -1;
    return entrada;

}


int mmu(int dir_logica, entradaTablaSegmentos* entrada){
    //log_info(logger, "dir_logica: %d", dir_logica);
    int nro_segmento = floor(dir_logica / config->tam_max_segmento);
    int despl = dir_logica % config->tam_max_segmento;
    //log_info(logger, "( %d | %d )", nro_segmento, despl);
    
    entradaTablaSegmentos* unaEntrada = buscarSegmentoPorID(pcb_actual->tabla_segmentos, nro_segmento); //Completa la tabla recibida por referencia

    entrada->dirBase = unaEntrada->dirBase;
    entrada->idSegmento = unaEntrada->idSegmento;
    entrada->size = unaEntrada->size;

    //Validar cuando entrada es -1
    //log_info(logger, "SIZE SEGMENTO %d es %d", entrada->idSegmento, entrada->size);

    if(despl > entrada->size){
        log_error(logger, "SEGMENTATION FAULT (Intenta acceder fuera del limite del segmento)");
        return -1;
    }

    int dir_fisica = entrada->dirBase + despl;
    return dir_fisica;
}


t_pcb_cpu* deserializar_contexto_y_char(int socket, char* string){

    t_pcb_cpu* contexto = deserializarPCB(socket);

    int size_string;
    
    recv(socket, &size_string, sizeof(int), MSG_WAITALL);

    char* string_received = malloc(size_string);

    recv(socket, string_received, size_string, MSG_WAITALL);

    (*string) = string_received; 

    return contexto;
}

t_pcb_cpu* deserializar_contexto_e_int(int socket, int* entero){

    t_pcb_cpu* contexto = deserializarPCB(socket);
    
    recv(socket, entero, sizeof(int), MSG_WAITALL);

    return contexto;
}

t_pcb_cpu* deserializar_contexto_char_e_int(int socket, char* string, int* entero){

    t_pcb_cpu* contexto = deserializarPCB(socket);

    recv(socket, entero, sizeof(int), MSG_WAITALL);

    int size_string;
    
    recv(socket, &size_string, sizeof(int), MSG_WAITALL);

    char* string_received = malloc(size_string);

    recv(socket, string_received, size_string, MSG_WAITALL);

    (*string) = string_received; 

    return contexto;
}

t_pcb_cpu* deserializar_contexto_char_y_2_int(int socket, char* string, int* entero_a, int* entero_b){

    t_pcb_cpu* contexto = deserializarPCB(socket);

    recv(socket, entero_a, sizeof(int), MSG_WAITALL);
    recv(socket, entero_b, sizeof(int), MSG_WAITALL);

    int size_string;
    
    recv(socket, &size_string, sizeof(int), MSG_WAITALL);

    char* string_received = malloc(size_string);

    recv(socket, string_received, size_string, MSG_WAITALL);

    (*string) = string_received; 

    return contexto;
}

t_pcb_cpu* deserializar_contexto_int_e_int(int socket, int* entero_a, int* entero_b){

    t_pcb_cpu* contexto = deserializarPCB(socket);

    recv(socket, entero_a, sizeof(int), MSG_WAITALL);
    recv(socket, entero_b, sizeof(int), MSG_WAITALL);

    return contexto;
}




// Funciones de para deserializar en Memoria


int deserializar_request_cpu_mem(int socket){
    e_cod_cpu_mem codigo;

    recv(socket, &codigo, sizeof(int), MSG_WAITALL);

    if(codigo == REQ_LECTURA){
        int* dir_fisica;
        int* size;
        deserializar_int_e_int(socket, dir_fisica, size);
        //foo_leer_en_memoria();
        //foo_devolver_resultado_lectura();
    }
    else if (codigo == REQ_ESCRITURA){
        int* dir_fisica;
        char* registro;
        deserializar_char_e_int(socket, dir_fisica, registro);
        //foo_escribir_en_memoria();
        //foo_devolver_resultado();
    }
    else{
        log_error(logger, "REQUEST DE CPU A MEMORIA INVALIDO");
        return -1;
    }
    return 0;
}

int test_serializacion_mem(){
    int tam_ints;
    int tam_char_int;
    int resul = serializar_int_e_int(REQ_LECTURA,10,14, &tam_ints);
    int resul2 = serializar_char_e_int(REQ_ESCRITURA, 10,"AAAA", &tam_char_int);
    //printf("Result: %d y Result2: %d\n", resul, resul2);
    return 0;
}

void test_SET(){
    t_registros_cpu* reg = pcb_actual -> registros;
    char ax[4];
    char bx[4];
    char cx[4];

    leer_registro("AX", ax);
    leer_registro("BX", bx);
    leer_registro("CX", cx);

    ////log_debug(logger, "AX: %s", ax);
    ////log_debug(logger, "BX: %s", bx);
    ////log_debug(logger, "CX: %s", cx);
    
}

/*void test_SET(){
    t_registros_cpu* reg = pcb_actual -> registros;
    ////log_debug(logger, "REGISTRO AX: %s", reg -> ax);
    ////log_debug(logger, "REGISTRO BX: %s", reg -> bx);
    ////log_debug(logger, "REGISTRO CX: %s", reg -> cx);
}*/


int test_serializacion_kernel(){
    //Instrucciones
    INSTRUCCIONES* ins1 = new_null_instr();
    INSTRUCCIONES* ins2 = new_null_instr();
    ins1 -> cantParam = 3;
    ins1 -> comando = "MOV_OUT";
    ins1 -> parametro1 = "12";
    ins1 -> parametro2 = "AX";

    ins2 -> cantParam = 2;
    ins2 -> comando = "SET";
    ins2 -> parametro1 = "AX";
    ins2 -> parametro2 = "ASDS";

    t_list* lista_instruciones = list_create();

    list_add(lista_instruciones,ins1);
    list_add(lista_instruciones,ins2);

    //Registros
    t_registros_cpu* registros = malloc(sizeof(t_registros_cpu));
    strcpy(registros-> ax  ,"AAAA");
    strcpy(registros-> bx  ,"AAAA");
    strcpy(registros-> cx  ,"AAAA");
    strcpy(registros-> dx  ,"AAAA");
    strcpy(registros-> eax ,"AAAABBBB");
    strcpy(registros-> ebx ,"AAAABBBB");
    strcpy(registros-> ecx ,"AAAABBBB");
    strcpy(registros-> edx ,"AAAABBBB");
    strcpy(registros-> rax ,"AAAAAAAAAAAAAAAA");
    strcpy(registros-> rbx ,"AAAAAAAAAAAAAAAA");
    strcpy(registros-> rcx ,"AAAAAAAAAAAAAAAA");
    strcpy(registros-> rdx ,"AAAAAAAAAAAAAAAA");


  
    //Tabla Segmentos
    t_list* tabla_segmentos = list_create();
    
    entradaTablaSegmentos* ent1 = malloc(sizeof(entradaTablaSegmentos));
    entradaTablaSegmentos* ent2 = malloc(sizeof(entradaTablaSegmentos));

    ent1 -> idSegmento = 0;
    ent1 -> dirBase = 12;
    ent1 -> size = 64; 
    ent2 -> idSegmento = 12;
    ent2 -> dirBase = 64;
    ent2 -> size = 10; 

    list_add(tabla_segmentos, ent1);
    list_add(tabla_segmentos, ent2);
    
    t_list* archivos_abiertos = list_create();

    //Iniciailizo PCB
    pcb_actual = malloc(sizeof(t_pcb_cpu));
    pcb_actual -> pid = 123;
    pcb_actual -> instrucciones = lista_instruciones;
    pcb_actual -> program_counter = 0;
    pcb_actual -> registros = registros;
    pcb_actual -> tabla_segmentos = tabla_segmentos;
    pcb_actual -> archivos_abiertos = archivos_abiertos;

    return 0;
}
