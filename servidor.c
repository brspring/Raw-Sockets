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

int main() {
    int soquete = cria_raw_socket("lo"); //lo é loopback para envviar a msg dentro do mesmo PC, acho q eth0 é entre dois PC
    Frame frame;

    memset(&frame, 0, sizeof(frame));
    
    if( listen(soquete, 1) == -1){
        perror("Erro ao escutar");
        exit(-1);
    }

    ssize_t num_bytes = recv(soquete, &frame, sizeof(frame), 0); //recv() recebe a mensagem do cliente

    if (num_bytes == -1) {
        perror("Erro ao receber dados");
        exit(-1);
    }

    printf("Servidor recebeu: \n");
    printf("  Marcador de Início: 0x%02X\n", frame.marcador_inicio);
    printf("  Tamanho: %u\n", frame.tamanho);
    printf("  Sequência: %u\n", frame.sequencia);
    printf("  Tipo: 0x%02X\n", frame.tipo);
    printf("  Dados: %s\n", frame.data); 
    printf("  CRC: 0x%02X\n", frame.crc);

    close(soquete);  // diz que a operacao terminou ver na imagem
    return 0;
}
