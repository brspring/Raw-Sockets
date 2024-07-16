#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>

#define MAX_DATA_SIZE 50  // 63 bytes

typedef struct {
    uint8_t marcador_inicio;        
    uint8_t tamanho;         
    uint8_t sequencia;     
    uint8_t tipo;          
    char data[MAX_DATA_SIZE]; 
    uint8_t crc;           
} Frame;

#endif // FRAME_H
