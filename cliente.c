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


void init_frame(Frame *frame, uint8_t sequencia, uint8_t tipo) {
        frame->marcador_inicio = 0x7E; 
        frame->sequencia = sequencia;
        frame->tipo = tipo;
}

uint8_t convert_bits_to_uint8(const char *bit_string) {
    return (uint8_t) strtol(bit_string, NULL, 2);
}

int main(int argc, char **argv) {
    Frame frame;

    int soquete = cria_raw_socket("lo"); //lo é loopback para envviar a msg dentro do mesmo PC, acho q eth0 é entre dois PC

    if (soquete == -1) {
        perror("Erro ao criar socket");
        exit(-1);
    }
   /* possiveis mensagens =
    00000 ack
    00001 nack
    01010 lista
    01011 baixar
    10000 mostra na tela
    10001 descritor arquivo
    10010 dados
    11110 fim tx
    11111 erro
    VER COMO EU FACO PRA RECEBER O TIPO DA MENSAGEM
    if(strcmp(tipo, "00000") == 0)]
    }else if(strcmp(tipo, "00001") == 0){
        init_frame(&frame, 0, TIPO_NACK);
    }else if(strcmp(tipo, "01010") == 0){
        init_frame(&frame, 0, TIPO_LISTA);
    }else if(strcmp(tipo, "01011") == 0){
        init_frame(&frame, 0, TIPO_BAIXAR);
    }else if(strcmp(tipo, "10000") == 0){
        init_frame(&frame, 0, TIPO_MOSTRA_NA_TELA);
    }else if(strcmp(tipo, "10001") == 0){
        init_frame(&frame, 0, TIPO_DESCRITOR_ARQUIVO);
    }*/
    char *tipo = argv[1];
    if(strcmp(tipo, "00000") == 0){
        init_frame(&frame, 0, TIPO_ACK);
    }

    strcpy(frame.data, "Oieeer");
    frame.tamanho = strlen(frame.data);
    frame.crc = 0;

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
    close(soquete); //diz que a operacao terminou ver na imagem
    return 0;
}