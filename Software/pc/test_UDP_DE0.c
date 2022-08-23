/*==============================================================================
                                  LIBRERIAS
==============================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <string.h>
// interprocess comm
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <time.h>
// network stuff
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>  /* IP address conversion stuff */
#include <netdb.h>

int main() {

    /*==========================================================================
                               Configuracion de UDP
    ==========================================================================*/
    int sock, sock_rcv, n, flags;
    unsigned int length;

    // Estructuras que contienen la direccion del servidor, cliente y ...
    struct sockaddr_in client, client_rcv, from;
    // struct sockaddr_in from;

    // Puntero a la estructura hostent
    // The hostent structure is used by functions to store information about a given host, such as host name, IPv4 address, and so forth.
    struct hostent *hp;

    // Buffer para mandar / recibir
    char buffer[1024];

    // Comando de Linux para configurar la direccion IP
    system("ifconfig eth0 10.10.10.34 netmask 255.255.255.0"); // Set the DE0 IP address
    // system("ifconfig eth0 192.168.1.12 netmask 255.255.255.0"); // Set the DE0 IP address
    printf("===== UDP test HPS =====\r\n");

    // open socket and associate with remote IP address
    // socket(): creates an endpoint for communication and returns a file descriptor that refers to that endpoint.
    // AF_INET: IPv4, SOCK_DGRAM: UDP
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("socket\n\r");
    }

    // Indica a la estructura client que se usa IPv4
    client.sin_family = AF_INET;

    // associate remote IP address
    printf("10.10.10.36\n");
    // printf("192.168.1.13");

    // gethostbyname retorna un puntero a la estructura hostent que copia la direccion IPv4 al campo h_name
    hp = gethostbyname("10.10.10.36"); // replace with actual
    // hp = gethostbyname("192.168.1.13"); // replace with actual
    if (hp == 0) {
        printf("Unknown host\n\r");
    }

    // Copia contenido (o direccion del puntero?) de h_addr a sin_addr
    bcopy((char *)hp -> h_addr, (char *)&client.sin_addr, hp -> h_length);

    // set IP port number
    client.sin_port = htons(9090);

    // associate remote IP address with socket
    sock_rcv = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_rcv < 0) {
        printf("socket\n\r");
    }

    //set timer for recv_socket
    struct timeval tv;
    fd_set fds;

    // Set up the struct timeval for the timeout.
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    // Wait until timeout or data received.
    int rv = select (sock_rcv, &fds, NULL, NULL, &tv);
    if ( rv == 0) {
        printf("Timeout..\n");
    } else if( rv == -1 ) {
        printf("Error..\n");
    }

    flags = fcntl(sock_rcv, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(sock_rcv, F_SETFL, flags);

    length = sizeof(client_rcv);

    client_rcv.sin_family = AF_INET;
    client_rcv.sin_addr.s_addr=INADDR_ANY;
    client_rcv.sin_port = htons(9090);

    if (bind(sock_rcv, (struct sockaddr *)&client_rcv, length) < 0){
        printf("binding...");
    }

    length = sizeof(struct sockaddr_in);

    int i, j;
    unsigned char data[1024 * 32];

    i = 0;

    while (i < 32) {

        n = recvfrom(sock_rcv, buffer, 1024, 0, (struct sockaddr *)&from, &length);
        if(n > 0) {
            //printf("Message received:%s\n\r",buffer);
            for (j = 0; j < 1024; j++) {
                data[j + i * 1024] = buffer[j];
            }

            i++;
            printf("contador: %i\n", i);

            memset(buffer, 0, 1024);
        }
    }

    printf("\nData:\n");


    int k;
    for (k = 0; k < 1024 * 32; k++) {
        printf("%x", data[k]);
        if ((k + 1) % 1024 == 0) {
            printf("\n\n");
        }
    }

}
