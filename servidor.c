#include <stdio.h>          
#include <stdlib.h>         
#include <string.h>         
#include <unistd.h>         
#include <arpa/inet.h>      
#include <net/ethernet.h>   
#include <net/if.h>         
#include <linux/if_packet.h>


#include "API-raw-socket.h"


int main() {
    int soquete = cria_raw_socket("lo"); //lo é loopback para envviar a msg dentro do mesmo PC, acho q eth0 é entre dois PC
    char buffer[1024];
    
    memset(buffer, 0, sizeof(buffer));
    ssize_t num_bytes = recv(soquete, buffer, sizeof(buffer), 0); //recv() recebe a mensagem do cliente

    if (num_bytes == -1) {
        perror("Erro ao receber dados");
        exit(-1);
    }

    printf("Servidor recebeu: %s\n", buffer);
    close(soquete);  // diz que a operacao terminou ver na imagem
    return 0;
}
