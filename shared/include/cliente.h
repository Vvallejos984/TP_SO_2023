#ifndef CLIENTE_H
#define CLIENTE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <stdbool.h>
#include "shared.h"
//server
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <ctype.h>



int inicializar_cliente(char *ip,char *puerto);
int handshake_cliente(int socket, idProtocolo id_a_conectar);



#endif