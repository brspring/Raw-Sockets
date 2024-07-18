#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>

#define MAX_DATA_SIZE 50  // 63 bytes

#define TIPO_ACK 0x00
#define TIPO_NACK 0x01
#define TIPO_LISTA 0x0A
#define TIPO_BAIXAR 0x0B
#define TIPO_MOSTRA_NA_TELA 0x10
#define TIPO_DESCRITOR_ARQUIVO 0x11
#define TIPO_DADOS 0x12
#define TIPO_FIM_TX 0x1E
#define TIPO_ERRO 0x1F

typedef struct {
    uint8_t marcador_inicio;        
    uint8_t tamanho;         
    uint8_t sequencia;     
    uint8_t tipo;          
    char data[MAX_DATA_SIZE]; 
    uint8_t crc;           
} Frame;

#endif // FRAME_H
