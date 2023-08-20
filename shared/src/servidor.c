#include "servidor.h"

int inicializar_servidor(char* puerto){
    struct addrinfo *servinfo;
    struct addrinfo hints;
    struct addrinfo *p;
    int socket_servidor;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if(getaddrinfo(NULL, puerto, &hints, &servinfo)!=0){ //crea el socket del server
        perror("Error al obtener la información del address");
        exit(EXIT_FAILURE);
    } 
    for (p = servinfo; p != NULL; p = p->ai_next){
        socket_servidor = socket(servinfo->ai_family,
                                 servinfo->ai_socktype,
                                 servinfo->ai_protocol);

        if(socket_servidor == -1){
            perror("Error creando el socket");
            continue;
        }

        int reuse = 1;
        if (setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
            perror("Error setting SO_REUSEADDR option");
            exit(EXIT_FAILURE);
        }

        if(bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen) == -1){
            perror("Error vinculando el socket");
            continue;
        }
        break;
    }
    

    freeaddrinfo(servinfo);

    if (p == NULL) {
        fprintf(stderr, "Fallo al vincular socket\n");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(socket_servidor, SOMAXCONN) == -1) {
        perror("Error al esperar conexiones");
        exit(EXIT_FAILURE);
    }
    return socket_servidor;
}

void handshake_servidor(int socket, idProtocolo id_conectado){
    uint32_t handshake;
    uint32_t resultOk = 0;
    uint32_t resultError = -1;

    if(recv(socket, &handshake, sizeof(uint32_t), MSG_WAITALL) == -1)
        perror("No funciono el handshake");

    printf("\nRecibi un: %d\n", handshake);

    if(handshake == id_conectado){
        send(socket, &resultOk, sizeof(uint32_t), 0);
        printf("\nHandshake OK. Fue: %d\n", resultOk);  //Borrar, solo para corroborar
    }
    else{
        send(socket, &resultError, sizeof(uint32_t), 0);
        printf("\nHandshake incorrecto. Fue: %d\n", resultError); //Borrar, solo para corroborar
    }
}

idProtocolo handshake_servidor_multiple(int socket, idProtocolo idProt1, idProtocolo idProt2, idProtocolo idProt3){
    uint32_t handshake;
    uint32_t resultOk = 0;
    uint32_t resultError = -1;

    if(recv(socket, &handshake, sizeof(uint32_t), MSG_WAITALL) == -1)
        perror("No funciono el handshake");

    printf("\nRecibi un: %d\n", handshake);

    if(handshake == idProt1 || handshake == idProt2 || handshake == idProt3){
        send(socket, &resultOk, sizeof(uint32_t), 0);
        printf("\nHandshake OK. Fue: %d\n", resultOk);  //Borrar, solo para corroborar
    }
    else{
        send(socket, &resultError, sizeof(uint32_t), 0);
        printf("\nHandshake incorrecto. Fue: %d\n", resultError); //Borrar, solo para corroborar
    }
    return handshake;
}

void cerrar_servidor(int servidor){
    close(servidor);
}
