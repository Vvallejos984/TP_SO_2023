#ifndef SERVIDOR_H
#define SERVIDOR_H

#include <string.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include "shared.h"
#include <stdbool.h>
//server
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>

int inicializar_servidor(char* puerto);
void handshake_servidor(int socket, idProtocolo id_conectado);
int esperar_cliente(int socket_servidor, t_log *logger);
void cerrar_servidor(int servidor);


#endif