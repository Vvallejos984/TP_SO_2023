#include "cliente.h"

int inicializar_cliente(char *ip,char *puerto){

    struct addrinfo hints, *servinfo, *p;
    int socket_cliente;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if(getaddrinfo(ip, puerto, &hints, &servinfo)!=0){ //agarra el puerto y la ip del server
        perror("Error al obtener la información del address");
        return -1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {

        socket_cliente = socket(servinfo->ai_family,
                                    servinfo->ai_socktype,
                                    servinfo->ai_protocol);

        if(socket_cliente == -1){
            perror("Error creando el socket");
            continue;
        }
        if(connect(socket_cliente, p->ai_addr, p->ai_addrlen) == -1){
            //perror("Error al conectar al servidor");
            close(socket_cliente);
            continue;
        }
        break;
    }

    if (p == NULL) {
        //fprintf(stderr, "Fallo al conectar al servidor\n");
        return -1;
    }

    freeaddrinfo(servinfo);
    
    printf("Conectado al servidor con socket %d\n", socket_cliente);

    return socket_cliente;
}

int handshake_cliente(int socket, idProtocolo id_a_conectar){
    uint32_t handshake = id_a_conectar;
    uint32_t result = -1;
    printf("Envio el valor %d al socket %d\n", handshake, socket);
    send(socket, &handshake, sizeof(uint32_t), 0);
    if(recv(socket, &result, sizeof(uint32_t), MSG_WAITALL) == -1){
        perror("No recibo handshake");
        close(socket);
        exit(EXIT_FAILURE);
    }
        

    printf("\nHICE EL HANDSHAKE, FUE: %d\n", result);
    return result;
}
