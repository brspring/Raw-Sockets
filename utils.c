#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <pwd.h>
#include <string.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <sys/time.h>
#include <sys/types.h>
#include <linux/if_packet.h>

#include "utils.h"

void init_frame(frame_t *frame, unsigned int sequencia, unsigned int tipo) {
    frame->marcador_inicio = BIT_INICIO;
    frame->sequencia = sequencia;
    frame->tipo = tipo;
    frame->tamanho = strlen(frame->data);
}

void envia_nack(int soquete, frame_t *frameSend)
{
    memset(frameSend, 0, sizeof(*frameSend));
    init_frame(frameSend, 0, TIPO_NACK);
    if (send(soquete, frameSend, sizeof(*frameSend), 0) == -1)
    {
        perror("Erro ao enviar NACK\n");
    }
}

void envia_ack(int soquete, frame_t *frameSend)
{
    memset(frameSend, 0, sizeof(*frameSend));
    init_frame(frameSend, 0, TIPO_ACK);
    if (send(soquete, frameSend, sizeof(*frameSend), 0) == -1)
    {
        perror("Erro ao enviar mensagem\n");
    }
}

int recv_para_espera(int socket, frame_t *frame, int timeout_sec) {
    fd_set read_fds;
    struct timeval timeout;
    int result;

    FD_ZERO(&read_fds);
    FD_SET(socket, &read_fds);

    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;

    result = select(socket + 1, &read_fds, NULL, NULL, &timeout);

    if (result == -1) {
        perror("Erro no select");
        return -1;
    } else if (result == 0) {
        printf("Timeout ao esperar por dados\n");
        return -1;
    } else {
        return recv(socket, frame, sizeof(*frame), 0);
    }

    //(recv_with_timeout(soquete, &frameRecv, 5) == -1)
}

unsigned int gencrc(const uint8_t *data, size_t size) {
    unsigned int crc = 0xff; // Inicializa o CRC com 0xff
    size_t i, j;

    for (i = 0; i < size; i++) {
        crc ^= data[i]; // XOR o CRC com o byte de dados atual

        for (j = 0; j < 8; j++) {
            if (crc & 0x80) // se o bit mais significativo for 1
                crc = (crc << 1) ^ 0x07; // desloca à esquerda e XOR com 0x07
            else
                crc <<= 1; //desloca à esquerda
        }
    }
    return crc;
}
