#ifndef UTILS_H
#define UTILS_H

#include "frame.h"

// Declaração das funções comuns
int recv_para_espera(int socket, frame_t *frame, int timeout_sec);
void init_frame(frame_t *frame, unsigned int sequencia, unsigned int tipo);
void envia_nack(int soquete, frame_t *frameSend);
void envia_ack(int soquete, frame_t *frameSend);
unsigned int gencrc(const uint8_t *data, size_t size);

#endif // UTILS_H
