#include "consola.h"

t_log* logger;

int main(int argc, char ** argv){
    logger = log_create("./cfg/consola.log", "CONSOLA", true, LOG_LEVEL_INFO); 
    
    t_consola_config* config;
    int a = 0;
    t_list* instrucciones;
    config = inicializar_config(argv[2]);
    instrucciones = leer_archivo_pseudocodigo(argv[1]);

    int size = 0;
    void* buffer = serializarListaInstrucciones(instrucciones, &size);

    int socket_cliente = inicializar_cliente(config->ip_kernel, config->puerto_kernel);
    handshake_cliente(socket_cliente, P_CONSOLA);

    int validSend = send(socket_cliente, buffer, size, 0);

    esperar_fin(socket_cliente);


    return 0;
}

t_consola_config* inicializar_config(char* path){

    t_config* raw;
    raw = config_create(path);
    log_debug(logger, "CREO CONFIG");

    t_consola_config* config = malloc(sizeof(t_consola_config));

    config->ip_kernel = buscarEnConfig(raw, "IP_KERNEL");
    config->puerto_kernel = buscarEnConfig(raw, "PUERTO_KERNEL");

    return config;
}

t_list* leer_archivo_pseudocodigo(char *path){
    FILE *archivo_de_instrucciones = fopen(path, "r");

    log_debug(logger, "OK FOPEN");
    if(archivo_de_instrucciones == NULL){
        log_error(logger, "No se pudo abrir el archivo de insrucciones");
        return NULL;
    }
    char* linea = string_new();
    size_t len;
    ssize_t read;
    log_debug(logger, "OK INIT");

    t_list *instrucciones = list_create();

    while((read = getline(&linea, &len, archivo_de_instrucciones)) != -1){
        log_debug(logger, "NUEVA LINEA");

        if(string_contains(linea, "\n")){
            linea = string_substring_until(linea, string_length(linea)-1);
            //log_debug(logger, "LINEA TENIA /n");

        }

        INSTRUCCIONES* instruc = new_null_instr();
        log_debug(logger, "NUEVA INSTR");
   
        char* token = malloc(50);
        token = strtok(linea, " ");
        strcpy(instruc->comando,token);
        //log_debug(logger, "%s -> %d", token, strlen(token));
        
        for(int i=0; token!=NULL; i++){
            if(i==1){                    //==>  2da posicion  si no es null se mete el dato en la variable
                //log_debug(logger, "%s -> %d", token, strlen(token));
                strcpy(instruc->parametro1,token);
            }
            if(i==2){                    //==>  3ra posicion  si no es null se mete el dato en la variable
                //log_debug(logger, "%s -> %d", token, strlen(token));
                strcpy(instruc->parametro2,token);
            }
            if(i==3){                    //==>  4ta posicion  si no es null se mete el dato en la variable
                //log_debug(logger, "%s -> %d", token, strlen(token));
                strcpy(instruc->parametro3,token);
            }
            token = strtok(NULL, " ");
            instruc->cantParam++;
        }
        list_add(instrucciones, instruc);
        //linea = string_new();
    }
    fclose(archivo_de_instrucciones);
    if(linea)
        free(linea);
    return instrucciones;
}

void esperar_fin(int socket_cliente){
    int code = -1;
    recv(socket_cliente, &code, sizeof(int), MSG_WAITALL);
    log_info(logger, "Finalizando con '%d'\n", code);
    close(socket_cliente);
}
