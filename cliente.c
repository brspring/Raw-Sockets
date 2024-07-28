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


void init_frame(frame_t *frame, unsigned int sequencia, unsigned int tipo) {
        frame->marcador_inicio = BIT_INICIO;
        frame->sequencia = sequencia;
        frame->tipo = tipo;
        frame->tamanho = strlen(frame->data);
        frame->crc = 0;
}

void MenuCliente(){
    printf("-------------------\n");
    printf("Cliente Iniciado\n");
    printf("Digite seu comando: \n");
}

void lista(int socket){
    frame_t frameSend;
    frame_t frameRecv;
    init_frame(&frameSend, 0, TIPO_LISTA);
    //strcpy(frameSend.data, "oii");
    if (send(socket, &frameSend, sizeof(frameSend), 0) == -1)
        {
            perror("Erro ao enviar mensagem! \n");
        }

    recv(socket, &frameRecv, sizeof(frameRecv), 0);
    switch(frameRecv.tipo) {
        case TIPO_ACK:
            printf("ACK\n");
            memset(&frameRecv, 0, sizeof(frameRecv));
            while (1) {
                switch(frameRecv.tipo){
                    case TIPO_LISTA:
                        printf("Cliente recebeu:\n %s", frameRecv.data);
                        if (recv(socket, &frameRecv, sizeof(frameRecv), 0) == -1) {
                            perror("Erro ao receber mensagem! \n");
                            break;
                        }
                    case  TIPO_FIM_TX:
                        break;
                    case  TIPO_ERRO:
                        goto tipo_erro;
                        break;
                    case TIPO_NACK:
                        goto tipo_nack;
                        break;
                }
                }
        case TIPO_ERRO:
            tipo_erro:
            printf("Erro ao receber a lista de arquivos\n");
            printf("Envie novamente a mensagem\n");
            break;
        case TIPO_NACK:
            tipo_nack:
            printf("NACK\n");
            printf("Envie novamente a mensagem\n");
            break;
}



}

int main(int argc, char **argv) {
    //frame_t frame;
    //char *tipo = argv[1];

    int soquete = cria_raw_socket("eno1"); //lo é loopback para envviar a msg dentro do mesmo PC, acho q eth0 é entre dois PC
    if (soquete == -1) {
        perror("Erro ao criar socket");
        exit(-1);
    }
    char arg[100];

    while(1){
        MenuCliente();
        //pega oq foi digitado da entrada
        fgets(arg, 100, stdin);
        strtok(arg, "\n");

        if(strcmp(arg, "01010") == 0){
            lista(soquete);
        }
    }
    /*
    ssize_t num_bytes = send(soquete, &frame, sizeof(frame), 0); //send() envia a mensagem para o servidor

    if (num_bytes == -1) {
        perror("Erro ao enviar dados");
        exit(-1);
    }

    printf("Cliente enviou: \n");
    printf("  Marcador de Início: 0x%02X\n", frame.marcador_inicio);
    printf("  Tamanho: %u\n", frame.tamanho);
    printf("  Sequência: %u\n", frame.sequencia);
    printf("  Tipo: 0x%02X\n", frame.tipo);
    printf("  Dados: %s\n", frame.data);
    printf("  CRC: 0x%02X\n", frame.crc);
 */
    close(soquete); //diz que a operacao terminou ver na imagem
    return 0;
}
