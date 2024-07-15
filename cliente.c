#include <stdio.h>          
#include <stdlib.h>         
#include <string.h>         
#include <unistd.h>         
#include <arpa/inet.h>      
#include <net/ethernet.h>   
#include <net/if.h>         
#include <linux/if_packet.h>

#include "API-raw-socket.h"

#define BUFFER_SIZE 1024

int main() {
    int soquete = cria_raw_socket("lo"); //lo é loopback para envviar a msg dentro do mesmo PC, acho q eth0 é entre dois PC
    const char *mensagem = "Oieeer";
    ssize_t num_bytes = send(soquete, mensagem, strlen(mensagem), 0); //send() envia a mensagem para o servidor

    if (num_bytes == -1) {
        perror("Erro ao enviar dados");
        exit(-1);
    }

    printf("Cliente enviou: %s\n", mensagem);
    close(soquete); //diz que a operacao terminou ver na imagem
    return 0;
}