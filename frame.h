#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>

#define TIPO_ACK 0x00
#define TIPO_NACK 0x01
#define TIPO_LISTA 0x0A
#define TIPO_BAIXAR 0x0B
#define TIPO_MOSTRA_NA_TELA 0x10
#define TIPO_DESCRITOR_ARQUIVO 0x11
#define TIPO_DADOS 0x12
#define TIPO_FIM_TX 0x1E
#define TIPO_ERRO 0x19

#define MAX_DATA_SIZE 63  // 63 bytes
#define BIT_INICIO 0x7E

enum tipo_error {
    ACESSO_NEGADO = 1,
    NAO_ENCONTRADO = 2,
    DISCO_CHEIO = 3,
};

#pragma pack(push, 1)
struct frame {
    uint8_t marcador_inicio;
    unsigned int tamanho:6;
    unsigned int sequencia:5;
    unsigned int tipo:5;
    char data[MAX_DATA_SIZE];
    uint8_t crc;
};
#pragma pack(pop)
typedef struct frame frame_t;


#endif // FRAME_H
