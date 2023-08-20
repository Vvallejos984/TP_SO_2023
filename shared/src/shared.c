#include "shared.h"

/*PID: identificador del proceso (deberá ser un número entero, único en todo el sistema).
Instrucciones: lista de instrucciones a ejecutar.
Program_counter: número de la próxima instrucción a ejecutar.
Registros de la CPU: Estructura que contendrá los valores de los registros de uso general de la CPU.
Tabla de Segmentos: Contendrá ids, direcciones base y tamaños de los segmentos de datos del proceso.
Estimado de próxima ráfaga: estimación utilizada para planificar los procesos en el algoritmo HRRN, la misma tendrá un valor inicial definido por archivo de configuración y será recalculada bajo la fórmula de promedio ponderado vista en clases.
Tiempo de llegada a ready: Timestamp en que el proceso llegó a ready por última vez (utilizado para el cálculo de tiempo de espera del algoritmo HRRN).
Tabla de archivos abiertos: Contendrá la lista de archivos abiertos del proceso con la posición del puntero de cada uno de ellos.
*/

int testShared()
{
    printf("Funcionan las shared");
    return 0;
}

char *buscarEnConfig(t_config *config, char *index)
{
    char *valorObtenido;
    valorObtenido = config_get_string_value(config, index);
    return valorObtenido;
}

uint32_t textoAuint32(char *texto)
{
    uint32_t result;
    sscanf(texto, "%d", &result);
    return result;
}

double textoAdouble(char *texto)
{
    double result;

    result = (double)atof(texto);

    return result;
}

void showList(t_list *list)
{
    //printf("\n[");
    for (int i = 0; i < list_size(list); i++)
    {
        //printf("%s", list_get(list, i));
        //if (list_size(list) - i - 1)
            //printf(",");
    }
    //printf("]\n");
}

t_list *listarTexto(char *texto)
{
    char *rest = texto;
    t_list *lista = list_create();
    char *token = strtok_r(rest, "[, ]", &rest);

    while (token != NULL)
    {
        list_add(lista, token);
        token = strtok_r(rest, "[, ]", &rest);
    }
    return lista;
}

char* listaPCBATexto(t_list* lista){
    char* texto = string_new();
    string_n_append(&texto, "[", 1);

    for(int i=0; i<list_size(lista); i++){
        t_pcb_kernel* pcb = list_get(lista, i);
        char* pid = string_itoa(pcb->pid);
        string_n_append(&texto, pid, strlen(pid));
        if(i!=list_size(lista)-1){
            string_n_append(&texto, ", ", 2);
        }
    }

    string_n_append(&texto, "]", 1);

    return texto;
}


void listaTextoANum(t_list *original)
{
    list_map(original, (void *)atoi);
}

int crear_hilo_conexion(dataHiloConexion *data)
{
    while (1)
    {
        pthread_t hilo;
        int socket_cliente = accept(data->socket, NULL, NULL);
        pthread_create(&hilo,
                       NULL,
                       data->funcion,
                       socket_cliente);
        pthread_detach(hilo);
    }
}

void leerInstr(INSTRUCCIONES *instr)
{
    /*printf("%s %s %s %s\n",
           instr->comando,
           instr->parametro1 ? instr->parametro1 : "",
           instr->parametro2 ? instr->parametro2 : "",
           instr->parametro3 ? instr->parametro3 : "");*/
}

/*Imprime una lista de instrucciones*/
void leerLista(t_list *lista)
{
    for (int i = 0; i < list_size(lista); i++)
    {
        INSTRUCCIONES *instr = list_get(lista, i);
        //printf("(%d) ", i);
        leerInstr(instr);
    }
}

void milisleep(uint32_t retardo){
    //Retardo de memoria
    struct timespec ts;
    ts.tv_sec = retardo / 1000;
    ts.tv_nsec = (retardo % 1000) * 1000000;
    nanosleep(&ts, &ts);
}

//---------------------------------------------------------------
//-----------------------Serialización---------------------------
//---------------------------------------------------------------

void *serializar_codigo(int codigo, int *fullsize){

    *(fullsize) = sizeof(int);

    void* buffer = malloc(sizeof(int));

    memcpy(buffer, &codigo, sizeof(int));

    return buffer;
    
}

void *serializarInstruccion(INSTRUCCIONES *instr, int *fullsize)
{

    uint32_t size = sizeof(instr->cantParam);                  // Espacio para identificador de cantidad
    size += sizeof(int);                                       // Espacio para el identificador de tamaño del nombre
    size += strlen(instr->comando);                            // Espacio para nombre de la instr
    size += instr->cantParam * sizeof(int);                    // Espacio para los n identificadores de tamaño de param
    size += instr->parametro1 ? strlen(instr->parametro1) : 0; //{
    size += instr->parametro2 ? strlen(instr->parametro2) : 0; //{ Espacios para cada parámetro existente
    size += instr->parametro3 ? strlen(instr->parametro3) : 0; //{

    void *buffer = malloc(size); // Creo el buffer con el size calculado

    int despl = 0;
    int len = 0;

    memcpy(buffer + despl, &(instr->cantParam), sizeof(instr->cantParam));
    despl += sizeof(instr->cantParam);

    len = strlen(instr->comando);

    memcpy(buffer + despl, &len, sizeof(len));
    despl += sizeof(len);

    memcpy(buffer + despl, instr->comando, len);
    despl += len;

    for (int i = 0; i < instr->cantParam; i++)
    {
        char *param = string_new();
        switch (i)
        {
        case 0:
            strcpy(param, instr->parametro1);
            break;
        case 1:
            strcpy(param, instr->parametro2);
            break;
        case 2:
            strcpy(param, instr->parametro3);
            break;
        }

        len = strlen(param);

        memcpy(buffer + despl, &len, sizeof(len));
        despl += sizeof(len);

        memcpy(buffer + despl, param, len);
        despl += len;
    }
    (*fullsize) = despl;
    return buffer;
}

void *serializarListaInstrucciones(t_list *instrucciones, int *fullsize)
{
    void *buffer;
    int despl = 0;
    t_list *auxListaDeSubBuffers = list_create();
    t_list *auxSubSizes = list_create();
    int cantInstr = list_size(instrucciones);
    int acumulador = sizeof(cantInstr);

    for (int i = 0; i < cantInstr; i++)
    {
        int subSize;
        INSTRUCCIONES *instr = list_get(instrucciones, i);
        void *subBuffer = serializarInstruccion(instr, &subSize);
        acumulador += subSize;
        list_add(auxListaDeSubBuffers, subBuffer);
        list_add(auxSubSizes, subSize);
    }

    buffer = malloc(acumulador);
    memcpy(buffer + despl, &cantInstr, sizeof(cantInstr));
    despl += sizeof(cantInstr);

    for (int i = 0; i < cantInstr; i++)
    {
        void *subBuffer = list_get(auxListaDeSubBuffers, i);
        int subSize = list_get(auxSubSizes, i);
        memcpy(buffer + despl, subBuffer, subSize);
        despl += subSize;
    }

    (*fullsize) = despl;
    //printf("Size del buffer: %d = %d\n", despl, acumulador);
    return buffer;
}

void *serializarEntradaTablaSegmentos(entradaTablaSegmentos *entrada)
{
    int size = 3 * sizeof(int); // Tamaño predeterminado de una entrada de tabla de segmentos
    void *buffer = malloc(size);
    int despl = 0;

    memcpy(buffer + despl, &(entrada->idSegmento), sizeof(entrada->idSegmento));
    despl += sizeof(entrada->idSegmento);

    memcpy(buffer + despl, &(entrada->dirBase), sizeof(entrada->dirBase));
    despl += sizeof(entrada->dirBase);

    memcpy(buffer + despl, &(entrada->size), sizeof(entrada->size));
    despl += sizeof(entrada->size);

    return buffer;
}

void *serializarTablaSegmentos(t_list *tabla, int *fullsize)
{
    void *buffer;
    int cant = list_size(tabla);
    int despl = 0;
    int size = (1 + 3 * cant) * sizeof(int); // 1 int para la cantidad y 3 int por cada entrada

    buffer = malloc(size);

    memcpy(buffer + despl, &cant, sizeof(cant));
    despl += sizeof(cant);

    for (int i = 0; i < cant; i++)
    {
        entradaTablaSegmentos *entrada = list_get(tabla, i);

        //printf("\nSize de segmento %d = %d\n", entrada->idSegmento, entrada->size);

        void *subBuffer = serializarEntradaTablaSegmentos(entrada);

        memcpy(buffer + despl, subBuffer, SIZE_ENTRADA_TABLA_SEGM);
        despl += SIZE_ENTRADA_TABLA_SEGM;
    }

    (*fullsize)=despl;
    return buffer;
}


void* serializarArchivo(void* arch){
    void* buffer = malloc(sizeof(int)*2);

    memcpy(buffer, arch, sizeof(int));

    return buffer;

}

/*[DEFINIRLA]: de momento devuelve un buffer con un 0 indicando 0 archivos abiertos*/
void* serializarArchivosAbiertos(t_list* tabla, int* fullsize){
    int cant_arch = list_size(tabla);
    int despl = 0;
    int size = sizeof(int);
    void* buffer = malloc(size);

    memcpy(buffer, &cant_arch, sizeof(int));
    despl+=sizeof(int);

    for(int i=0; i<cant_arch; i++){
        t_file_pcb* archivo = list_get(tabla, i);
        char* nombre = archivo->filename;
        int sizeNombre = strlen(nombre);
        int pointer = archivo->pointer;

        size+= 2*sizeof(int) + sizeNombre;

        realloc(buffer, size);

        memcpy(buffer+despl, &sizeNombre, sizeof(int));
        despl+=sizeof(int);

        memcpy(buffer+despl, nombre, sizeNombre);
        despl+=sizeNombre;

        memcpy(buffer+despl, &pointer, sizeof(int));
        despl+=sizeof(int);
    }
    
    (*fullsize)=despl;

    return buffer;
}

void *serializarRegistros(t_registros_cpu *reg)
{
    int size = 4 * (4 + 8 + 16); // 4 registros de cada tamaño (4, 8 y 16) (112 bytes)
    int despl = 0;


    void* buffer = malloc(size);

    memcpy(buffer+despl, reg->ax, 4); despl+=4;
    memcpy(buffer+despl, reg->bx, 4); despl+=4;
    memcpy(buffer+despl, reg->bx, 4); despl+=4;
    memcpy(buffer+despl, reg->bx, 4); despl+=4;

    memcpy(buffer+despl, reg->eax, 8); despl+=8;
    memcpy(buffer+despl, reg->ebx, 8); despl+=8;
    memcpy(buffer+despl, reg->ecx, 8); despl+=8;
    memcpy(buffer+despl, reg->edx, 8); despl+=8;

    memcpy(buffer+despl, reg->rax, 16); despl+=16;
    memcpy(buffer+despl, reg->rbx, 16); despl+=16;
    memcpy(buffer+despl, reg->rcx, 16); despl+=16;
    memcpy(buffer+despl, reg->rdx, 16); despl+=16;

    return buffer;
}
    
/*Serializa el contexto de ejecución del PCB.
(Tanto para envios Kernel->CPU o CPU->Kernel)*/
void *serializarPCB(t_pcb_cpu *pcb, int *fullsize)
{

    uint32_t pid = pcb->pid; // sizeof(int)

    int instr_size;
    void *instr = serializarListaInstrucciones(pcb->instrucciones, &instr_size);

    uint32_t pc = pcb->program_counter; // sizeof(int)

    int reg_size = 112; // Tamaño predeterminado del conjunto de registros
    void *reg = serializarRegistros(pcb->registros);

    int segm_size;
    void *segm = serializarTablaSegmentos(pcb->tabla_segmentos, &segm_size);

    int arch_size;
    void *arch = serializarArchivosAbiertos(pcb->archivos_abiertos, &arch_size);

    int size = sizeof(pid) + instr_size + sizeof(pc) + reg_size + segm_size + arch_size;

    void *buffer = malloc(size);

    int despl = 0;

    memcpy(buffer + despl, &pid, sizeof(pid));
    despl += sizeof(pid);

    memcpy(buffer + despl, instr, instr_size);
    despl += instr_size;

    memcpy(buffer + despl, &pc, sizeof(pc));
    despl += sizeof(pc);

    memcpy(buffer + despl, reg, reg_size);
    despl += reg_size;

    memcpy(buffer + despl, segm, segm_size);
    despl += segm_size;

    memcpy(buffer + despl, arch, arch_size);
    despl += arch_size;

    (*fullsize) = despl;
    return buffer;
}

void* serializarDataArchivo(int codigo, int pid, int dir_fisica, int puntero, int tamanio_lectura_escritura, char* nombre_archivo, int* fullsize){
    int tamanio_nombre_archivo = strlen(nombre_archivo);
    void* buffer = malloc(6*sizeof(int) + tamanio_nombre_archivo);

    int offset = 0;

    memcpy(buffer + offset, &codigo, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &pid, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &dir_fisica, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &puntero, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &tamanio_lectura_escritura, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &tamanio_nombre_archivo, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, nombre_archivo, tamanio_nombre_archivo);
    offset += tamanio_nombre_archivo;

    (*fullsize) = offset;

    return buffer;
}

void* serializar_3_int(int codigo, int entero_a, int entero_b, int entero_c,  int* fullsize){
    void* buffer = malloc(4*sizeof(int));

    int offset = 0;

    memcpy(buffer + offset, &codigo, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &entero_a, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &entero_b, sizeof(int)); 
    offset += sizeof(int);
    memcpy(buffer + offset, &entero_c, sizeof(int)); 
    offset += sizeof(int);

    (*fullsize) = offset;

    return buffer;
}

void* serializar_int_e_int(int codigo, int entero_a, int entero_b, int* fullsize){
    void* buffer = malloc(3*sizeof(int));

    int offset = 0;

    memcpy(buffer + offset, &codigo, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &entero_a, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &entero_b, sizeof(int)); 
    offset += sizeof(int);

    (*fullsize) = offset;

    return buffer;
}

void *serializar_int(int codigo, int entero_a, int *fullsize)
{
    void *buffer = malloc(2 * sizeof(int));

    int offset = 0;

    memcpy(buffer + offset, &codigo, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &entero_a, sizeof(int));
    offset +=sizeof(int);

    (*fullsize) = offset;

    return buffer;
}

void* serializar_char_y_2_int(int codigo, int entero_a, int entero_b, char* string, int tamanio_string ,int* fullsize){
    void* buffer = malloc(4*sizeof(int) + tamanio_string);

    int offset = 0;

    memcpy(buffer + offset, &codigo, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &entero_a, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &entero_b, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &tamanio_string, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, string, tamanio_string);
    offset += tamanio_string;

    (*fullsize) = offset;
    
    return buffer;

}



void* serializar_char_e_int(int codigo, int entero, char* string, int* fullsize){
    int tam_string = strlen(string);
    void* buffer = malloc(3*sizeof(int) + tam_string);

    int offset = 0;

    memcpy(buffer + offset, &codigo, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &entero, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &tam_string, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, string, tam_string);
    offset += tam_string;

    (*fullsize) = offset;
    
    return buffer;

}

void* serializar_char(int codigo, char* string, int* fullsize){
    int tam_string = string_length(string);
    void* buffer = malloc(2*sizeof(int) + tam_string);

    int offset = 0;

    memcpy(buffer + offset, &codigo, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &tam_string, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, string, tam_string);
    offset += tam_string;

    (*fullsize) = offset;
    return buffer;
}

//---------------------------------------------------------------
//----------------------Deserialización--------------------------
//---------------------------------------------------------------

INSTRUCCIONES* deserializarInstruccion(int socket)
{
    INSTRUCCIONES* instr = new_null_instr();
    int cant = 0;
    int len = 0;
    int recibidos = 0;

    recv(socket, &cant, sizeof(int), MSG_WAITALL);
    instr->cantParam = cant;

    recv(socket, &len, sizeof(int), MSG_WAITALL);
    instr->comando = malloc(len + 1);
    recibidos = recv(socket, instr->comando, len, MSG_WAITALL);
    (instr->comando)[recibidos] = '\0';

    for (int i = 0; i < cant; i++)
    {
        recv(socket, &len, sizeof(int), MSG_WAITALL);

        switch (i)
        {
        case 0:
            instr->parametro1 = malloc(len + 1);
            recibidos = recv(socket, instr->parametro1, len, MSG_WAITALL);
            (instr->parametro1)[recibidos] = '\0';
            break;
        case 1:
            instr->parametro2 = malloc(len + 1);
            recibidos = recv(socket, instr->parametro2, len, MSG_WAITALL);
            (instr->parametro2)[recibidos] = '\0';
            break;
        case 2:
            instr->parametro3 = malloc(len + 1);
            recibidos = recv(socket, instr->parametro3, len, MSG_WAITALL);
            (instr->parametro3)[recibidos] = '\0';
            break;
        }
    }
    return instr;
}

t_list *deserializarListaInstrucciones(int socket)
{
    t_list *listaInstrucciones = list_create();
    int cantInstr;
    int acum = 0;

    int validRecv = recv(socket, &cantInstr, sizeof(int), MSG_WAITALL);
    //printf("Cant: %d\n", cantInstr);
    for (int i = 0; i < cantInstr; i++)
    {
        //printf("(%d)", i);
        INSTRUCCIONES *instr = deserializarInstruccion(socket);
        list_add(listaInstrucciones, instr);
    }
    return listaInstrucciones;
}

entradaTablaSegmentos *deserializarEntradaTablaSegmentos(int socket)
{
    int size = sizeof(int); // Tamaño predeterminado, los 3 campos del struct son de 4 bytes (uint32_t)

    entradaTablaSegmentos *entrada = malloc(sizeof(entradaTablaSegmentos));

    recv(socket, &(entrada->idSegmento), size, MSG_WAITALL);
    recv(socket, &(entrada->dirBase), size, MSG_WAITALL);
    recv(socket, &(entrada->size), size, MSG_WAITALL);

    return entrada;
}

t_list *deserializarTablaSegmentos(int socket)
{
    t_list *tablaSegmentos = list_create();

    int cant;

    recv(socket, &cant, sizeof(int), MSG_WAITALL);

    for (int i = 0; i < cant; i++)
    {
        entradaTablaSegmentos *entrada = deserializarEntradaTablaSegmentos(socket);
        list_add(tablaSegmentos, entrada);
    }

    return tablaSegmentos;
}

t_list* deserializarArchivosAbiertos(int socket){
    t_list* tabla = list_create();

    int cant;

    recv(socket, &cant, sizeof(int), MSG_WAITALL);

    for(int i=0; i<cant; i++){
        t_file_pcb* archivo = malloc(sizeof(t_file_pcb));

        int sizeNombre;

        recv(socket, &sizeNombre, sizeof(int), MSG_WAITALL);

        archivo->filename = malloc(sizeNombre+1);

        recv(socket, archivo->filename, sizeNombre, MSG_WAITALL);

        recv(socket, &(archivo->pointer), sizeof(int), MSG_WAITALL);

        list_add(tabla, archivo);
    }

    return tabla;
}

t_registros_cpu *deserializarRegistros(int socket)
{

    t_registros_cpu *registros = malloc(sizeof(t_registros_cpu));

    recv(socket, registros->ax, 4, MSG_WAITALL);
    recv(socket, registros->bx, 4, MSG_WAITALL);
    recv(socket, registros->cx, 4, MSG_WAITALL);
    recv(socket, registros->dx, 4, MSG_WAITALL);

    recv(socket, registros->eax, 8, MSG_WAITALL);
    recv(socket, registros->ebx, 8, MSG_WAITALL);
    recv(socket, registros->ecx, 8, MSG_WAITALL);
    recv(socket, registros->edx, 8, MSG_WAITALL);

    recv(socket, registros->rax, 16, MSG_WAITALL);
    recv(socket, registros->rbx, 16, MSG_WAITALL);
    recv(socket, registros->rcx, 16, MSG_WAITALL);
    recv(socket, registros->rdx, 16, MSG_WAITALL);

    return registros;
}

t_pcb_cpu *deserializarPCB(int socket)
{

    t_pcb_cpu *pcb = malloc(sizeof(t_pcb_cpu));

    recv(socket, &(pcb->pid), sizeof(int), MSG_WAITALL);

    pcb->instrucciones = deserializarListaInstrucciones(socket);

    recv(socket, &(pcb->program_counter), sizeof(int), MSG_WAITALL);

    pcb->registros = deserializarRegistros(socket);

    pcb->tabla_segmentos = deserializarTablaSegmentos(socket);

    pcb->archivos_abiertos = deserializarArchivosAbiertos(socket);



    return pcb;
}



int deserializar_int_e_int(int socket, int* entero_a, int* entero_b){
    recv(socket, entero_a, sizeof(int), MSG_WAITALL);
    recv(socket, entero_b, sizeof(int), MSG_WAITALL);

    return 0;
}

int deserializar_char_e_int(int socket, int* entero_a, char* string){

    recv(socket, entero_a, sizeof(int), MSG_WAITALL);

    int size_string;
    
    recv(socket, &size_string, sizeof(int), MSG_WAITALL);

    char* string_received = malloc(size_string);

    recv(socket, string_received, size_string, MSG_WAITALL);

    strcpy(string, string_received);

    string[size_string] = '\0';

    return 0;
}

int deserializar_char(int socket, char* string){

    int size_string;
    
    recv(socket, &size_string, sizeof(int), MSG_WAITALL);
    //printf("SIZE RECIBIDO PARA LEER EN MEMORIA: %d", size_string);
    char* string_received = malloc(size_string);

    recv(socket, string_received, size_string, MSG_WAITALL);

    strcpy(string, string_received);

    string[size_string] = '\0';

    return 0;
}

//---------------------------------------------------------------
//----------------------Inicialización---------------------------
//---------------------------------------------------------------
INSTRUCCIONES *new_null_instr()
{
    INSTRUCCIONES *instr = malloc(sizeof(INSTRUCCIONES));
    instr->cantParam = -1;
    instr->comando = string_new();
    instr->parametro1 = string_new();
    instr->parametro2 = string_new();
    instr->parametro3 = string_new();
    return instr;
}

t_pcb_cpu *new_cpu_pcb(t_pcb_kernel *base)
{
    t_pcb_cpu *pcb = malloc(sizeof(t_pcb_cpu));

    pcb->pid = base->pid;
    pcb->instrucciones = base->instrucciones;
    pcb->program_counter = base->program_counter;
    pcb->registros = base->registros;
    pcb->tabla_segmentos = base->tabla_segmentos;
    pcb->archivos_abiertos = base->archivos_abiertos;

    return pcb;
}
entradaTablaSegmentos* new_entrada_ts(uint32_t idSegmento, uint32_t dirBase, uint32_t size){
    entradaTablaSegmentos* entrada = malloc(sizeof(entradaTablaSegmentos));
    entrada->idSegmento = idSegmento;
    entrada->dirBase = dirBase;
    entrada->size = size;
    return entrada;
}

dataHiloConexion *new_dataHiloConexion(void *funcion(void *), int socket)
{
    dataHiloConexion *data = malloc(sizeof(dataHiloConexion));
    data->funcion = funcion;
    data->socket = socket;
    return data;
}

//---------------------------------------------------------------
//-----------------------Actualización---------------------------
//---------------------------------------------------------------

void actualizarContextoEjec(t_pcb_cpu *base, t_pcb_kernel *pcb)
{

    pcb->pid = base->pid;
    pcb->instrucciones = base->instrucciones;
    pcb->program_counter = base->program_counter;
    pcb->registros = base->registros;
    pcb->tabla_segmentos = base->tabla_segmentos;
    pcb->archivos_abiertos = base->archivos_abiertos;
}


//--------------------------------------------------------
//-----------------------ENVIOS---------------------------
//--------------------------------------------------------
int enviar_y_recibir_codigo(int socket, void* buffer, int tamanio){
    int result = send(socket, buffer, tamanio, NULL);
    //printf("TAM ENVIADO: %d", result);
    int response;
    recv(socket, &response, sizeof(int), MSG_WAITALL);

    free(buffer);
    return response;
}