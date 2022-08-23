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
#include <errno.h>
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
// Custom libraries
#include "hwlib.h"
#include "soc_cv_av/socal/socal.h"
#include "soc_cv_av/socal/hps.h"
#include "soc_cv_av/socal/alt_gpio.h"
#include "hps_0.h"

/*==============================================================================
                              MACROS Y PUNTEROS
==============================================================================*/

#define WAIT {}

// =====================================
// Bus HPS-to-FPGA
// =====================================

#define H2F_AXI_MASTER_BASE   0xC0000000

// Direcciones de SRAM
#define FPGA_ONCHIP_BASE      0xC8000000
#define FPGA_ONCHIP_SPAN      0x00010000
// Puntero de SRAM
void *sram_virtual_base;
volatile unsigned int * sram_ptr = NULL ;

// =====================================
// Bus LW HPS-to-FPGA
// =====================================

#define HW_REGS_BASE        0xff200000
#define HW_REGS_SPAN        0x00005000

// Direcciones del DMA
#define DMA					0xff200000
#define DMA_STATUS_OFFSET	0x00
#define DMA_READ_ADD_OFFSET	0x04 // DATASHEET says 1!
#define DMA_WRT_ADD_OFFSET	0x08
#define DMA_LENGTH_OFFSET	0x012
#define DMA_CNTL_OFFSET		0x024

// Direcciones del habilitador de FPGA
#define EN_FPGA_OFFSET      0x20

// Direcciones de bandera RAM
#define RAM_FLAG_OFFSET     0x30

// Puntero del bus LW HPS-to-FPGA
void *h2p_lw_virtual_base;

// Punteros del DMA
volatile unsigned int * DMA_status_ptr = NULL ;
volatile unsigned int * DMA_read_ptr = NULL ;
volatile unsigned int * DMA_write_ptr = NULL ;
volatile unsigned int * DMA_length_ptr = NULL ;
volatile unsigned int * DMA_cntl_ptr = NULL ;

// Puntero del habilitador de FPGA
volatile unsigned int * en_fpga_ptr = NULL;

// Puntero de la bandera RAM
volatile unsigned int * sram_flag_ptr = NULL;

// ======================================
// HPS On-Chip Memory
// ======================================

// Direcciones del OCRAM
#define HPS_ONCHIP_BASE		0xffff0000
#define HPS_ONCHIP_SPAN		0x00010000

// Punteros del OCRAM
void *hps_onchip_virtual_base;
volatile unsigned int * hps_onchip_ptr = NULL ;

/*==============================================================================
                              VARIABLES GLOBALES
==============================================================================*/

// /dev/mem file id
int fd;

// Numero de bytes por paquete
int num_bytes = 1024;
// Numero de words por paquete
int num_words = 256;
// Numero de secciones de la memoria
int num_secciones = 64;

/*==============================================================================
                                    MAIN
==============================================================================*/

int main() {

	/*==========================================================================
	                            Mapeo de memorias
	==========================================================================*/

	// Obtener direcciones virtuales del FPGA
    // Abrir /dev/mem
	if((fd = open("/dev/mem", ( O_RDWR | O_SYNC))) == -1) 	{
		printf("ERROR: could not open \"/dev/mem\"...\n");
		return(1);
	}

    //  Mapeo de las direcciones de memoria del bus LW HPS-to-FPGA
	sram_virtual_base = mmap(NULL, FPGA_ONCHIP_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, FPGA_ONCHIP_BASE);

	if(sram_virtual_base == MAP_FAILED) {
		printf("ERROR3: mmap3() failed...\n");
		close(fd);
		return(1);
	}

    // Puntero que apunta a la direccion de memoria de la SRAM
	sram_ptr =(unsigned int *)(sram_virtual_base);

    // Mapeo de las direcciones de memoria del bus LW HPS-to-FPGA
    h2p_lw_virtual_base = mmap(NULL, HW_REGS_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, HW_REGS_BASE);

    if (h2p_lw_virtual_base == MAP_FAILED) {
        printf("ERROR1: mmap3() failed...\n");
		close(fd);
        return 1;
    }

    // Punteros del DMA
	DMA_status_ptr = (unsigned int *)(h2p_lw_virtual_base);
	DMA_read_ptr = (unsigned int *)(h2p_lw_virtual_base + DMA_READ_ADD_OFFSET);
	DMA_write_ptr = (unsigned int *)(h2p_lw_virtual_base + DMA_WRT_ADD_OFFSET);
	DMA_length_ptr = (unsigned int *)(h2p_lw_virtual_base + DMA_LENGTH_OFFSET);
	DMA_cntl_ptr = (unsigned int *)(h2p_lw_virtual_base + DMA_CNTL_OFFSET);

    // Puntero del habilitador
	en_fpga_ptr = (unsigned int *)(h2p_lw_virtual_base + EN_FPGA_OFFSET);

    // Puntero de la bandera
	sram_flag_ptr = (unsigned int *)(h2p_lw_virtual_base + RAM_FLAG_OFFSET);

    // Mapeo de las direcciones de memoria del On-Chip RAM
    hps_onchip_virtual_base = mmap(NULL, HPS_ONCHIP_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, HPS_ONCHIP_BASE);

	if( hps_onchip_virtual_base == MAP_FAILED ) {
		printf( "ERROR: mmap3() failed...\n" );
		close( fd );
		return(1);
	}

    // Puntero de la OCRAM
	hps_onchip_ptr = (unsigned int *)(hps_onchip_virtual_base);

    /*==========================================================================
	                           Configuracion de UDP
	==========================================================================*/

	int sock, sock_rcv, n, flags;
	unsigned int length;

    // Estructuras que contienen direcciones IP
    struct sockaddr_in talker_addr, listener_addr;

    // Puntero a la estructura hostent
    // "The hostent structure is used by functions to store information about a given host, such as host name, IPv4 address, and so forth."
	struct hostent *hp;

    // Comando de Linux para configurar la direccion IP
	system("ifconfig eth0 10.10.10.36 netmask 255.255.255.0"); // IP del DE0-Nano-SoC (ROJ)

	printf("===== UDP test HPS =====\r\n");

    // Abre socket y lo asocia con una dirección IP remota
    // socket(): "creates an endpoint for communication and returns a file descriptor that refers to that endpoint"
    // AF_INET: IPv4, SOCK_DGRAM: UDP
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
        printf("ERROR: sock = socket() failed...\n\r");
        return(2);
    }
    printf("Successful creation of socket 'soc'\n");

    // Indica a la estructura talker_addr que se usa IPv4
    talker_addr.sin_family = AF_INET;

    // gethostbyname retorna un puntero a la estructura hostent que copia la direccion IPv4 al campo h_name
	hp = gethostbyname("10.10.10.34"); // Direccion IP de la PC (ROJ)
	if (hp == 0) {
        printf("ERROR: Unknown host\n\r");
        return(2);
    }

    // Copia contenido (o direccion del puntero?) de h_addr a sin_addr
    bcopy((char *)hp -> h_addr, (char *)&talker_addr.sin_addr, hp -> h_length);

    // Puerto asociado al envío de paquetes
	talker_addr.sin_port = htons(9090);

	// Abre socket y lo asocia con una dirección IP local

	sock_rcv = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock_rcv < 0) {
        printf("ERROR: sock_rcv = socket() failed...\n\r");
        return (2);
    }
    printf("Successful creation of socket 'soc_rcv'\n");

    // Establece timer para el sock_rcv
    struct timeval tv;
	fd_set fds;

    // Establece estructura timeval para el timeout.
	tv.tv_sec = 2;
	tv.tv_usec = 0;

    // Espera hasta timeout o data recibida
    int rv = select(sock_rcv, &fds, NULL, NULL, &tv);
    if (rv == 0) {
        printf("TIMEOUT: rv = select()...\n");
        // return(2);
    } else if( rv == -1 ) {
        printf("ERROR: rv = select() failed...\n");
        return(2);
    }

    // Banderas para sock_rcv
    printf("Setting flags...\n");
    flags = fcntl(sock_rcv, F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(sock_rcv, F_SETFL, flags);
    printf("Flags set successfully\n");

    length = sizeof(listener_addr);

    // Configuracion del puerto que escucha
    listener_addr.sin_family = AF_INET;
	listener_addr.sin_addr.s_addr = INADDR_ANY;
	listener_addr.sin_port = htons(8080);

    printf("Listener structure configured\n");

    printf("Binding...\n");
    if (bind(sock_rcv, (struct sockaddr *)&listener_addr, length) < 0) {
        printf("ERROR: bind() failed...");
        return(2);
    }
    printf("Bind successful\n");

	length = sizeof(talker_addr);

    /*==========================================================================
	                             Variables para debug
	==========================================================================*/

    int i;

	int sram_flag_vec[100];
	int rl_vec[100];

    /*==========================================================================
	                              Sincronizacion
	==========================================================================*/

    int sram_flag;
	unsigned int start_dir;
	int offset;

    // Habilita el FPGA
	*(en_fpga_ptr) = 1;

    // En t = 1, FSM escribe en SRAM[0:1023]
    sram_flag = *(sram_flag_ptr);

    while (*(sram_flag_ptr) == sram_flag) WAIT

    // En t = 2, FSM escribe en SRAM[1024:2047] y DMA copia SRAM[0:1023] a
    // OCRAM[0:1023]
    sram_flag = *(sram_flag_ptr);

    // === DMA transfer HPS->FPGA
	// set up DMA
	*(DMA_status_ptr) = 0;

	// read bus-master gets data from FPGA SRAM addr = 0x08000000
	*(DMA_status_ptr + 1) = 0x08000000;

	// write bus_master for OCRAM is mapped to 0xffff0000
	*(DMA_status_ptr + 2) = 0xffff0000;

	// copy 1024 bytes for 256 words
	*(DMA_status_ptr + 3) = num_bytes;

	// set bit 2 for WORD transfer
	// set bit 3 to start DMA
	// set bit 7 to stop on byte-count
	// start the timer because DMA will start
	*(DMA_status_ptr + 6) = 0b10001100;

	while ((*(DMA_status_ptr) & 0x010) == 0) WAIT
	while (*(sram_flag_ptr) == sram_flag) WAIT

    /*==========================================================================
	                              Lazo principal
	==========================================================================*/

    for (i = 0; i < 100; i++) {

        // Adquiere el valor de la bandera para saber en que direccion se esta escribiendo
        sram_flag = *(sram_flag_ptr);

        // === DMA transfer HPS->FPGA
    	// set up DMA
    	*(DMA_status_ptr) = 0;

    	// read bus-master gets data from FPGA SRAM addr = 0x08000000
		// write bus_master for OCRAM is mapped to 0xffff0000
		if (sram_flag == 0) {
			offset = (num_secciones - 1) * num_bytes;
		} else {
			offset = (sram_flag - 1) * num_bytes;
		}

		*(DMA_status_ptr + 1) = 0x08000000 + offset; // tercera seccion de memoria
		*(DMA_status_ptr + 2) = 0xffff0000 + offset; // tercera seccion de memoria

    	// copy 1024 bytes for 256 words
    	*(DMA_status_ptr + 3) = num_bytes;

    	// set bit 2 for WORD transfer
    	// set bit 3 to start DMA
    	// set bit 7 to stop on byte-count
    	// start the timer because DMA will start
    	*(DMA_status_ptr + 6) = 0b10001100;

		// Establece que direcciones de memoria se van a mandar por UDP
		if (sram_flag == 0) {
			start_dir = (num_secciones - 2) * num_words; // segunda seccion de memoria
		} else if (sram_flag == 1) {
            start_dir = (num_secciones - 1) * num_words; // tercera seccion de memoria
        } else {
			start_dir = (sram_flag - 2) * num_words; // posicion de memoria n-2
		}

		n = sendto(sock, (const void *)(hps_onchip_ptr + start_dir), num_bytes, 0, (const struct sockaddr *)&talker_addr, length);

        while ((*(DMA_status_ptr) & 0x010) == 0) WAIT
		while (*(sram_flag_ptr) == sram_flag) WAIT

    }

    // Inhabilita FPGA
    *(en_fpga_ptr) = 0;
	printf("\nFPGA deshabilitado\n");

	// Imprime el contenido del primer paquete que envía
	unsigned int mem_cont[256];

	memcpy((void*)mem_cont, (const void*)(hps_onchip_ptr), 1024);

	printf("\nContenido de la OCRAM\n");
	for (i = 0; i < 256; i++) {
		printf("%x\n", mem_cont[i]);
	}

    // Fin del programa
    return(0);

}
