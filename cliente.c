#include <stdio.h>          
#include <stdlib.h>         
#include <string.h>         
#include <unistd.h>         
#include <arpa/inet.h>      
#include <net/ethernet.h>   
#include <net/if.h>         
#include <linux/if_packet.h>

#include "API-raw-socket.h"
#include "frame.h"

#define BUFFER_SIZE 1024

int main() {
    int soquete = cria_raw_socket("lo"); //lo é loopback para envviar a msg dentro do mesmo PC, acho q eth0 é entre dois PC
    Frame frame;
    /*frame.marcador_inicio = 0;
    frame.tamanho = 6;
    frame.sequencia = 1; 
    frame.tipo = 0; 
    frame.crc = 0; */

    // ver esse jeito estranho de passar msg
    //const char *mensagem = "Oieeer";
    strcpy(frame.data, "Oieeer");
    printf("Cliente enviou: %s\n", frame.data);
    ssize_t num_bytes = send(soquete, &frame, sizeof(frame), 0); //send() envia a mensagem para o servidor

    if (num_bytes == -1) {
        perror("Erro ao enviar dados");
        exit(-1);
    }

    printf("Cliente enviou: %s\n", frame.data);
    close(soquete); //diz que a operacao terminou ver na imagem
    return 0;
}