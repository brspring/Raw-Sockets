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

void init_frame(frame_t *frame, unsigned int sequencia, unsigned int tipo) {
    frame->marcador_inicio = BIT_INICIO;
    frame->sequencia = sequencia;
    frame->tipo = tipo;
    frame->tamanho = strlen(frame->data);
    //frame->crc = gencrc((uint8_t *)frame, sizeof(frame_t) - sizeof(frame->crc));
}

void MenuCliente(){
    printf("-------------------\n");
    printf("Cliente Iniciado\n");
    printf(" 1 - Listar arquivos\n");
    printf(" 2 - Baixar Arquivo\n");
    printf(" 3 - Sair\n");
}

void lista(int soquete){
    frame_t frameSend;
    frame_t frameRecv;
    int sequencia_esperada = 0;

    nack_msg:

    init_frame(&frameSend, 0, TIPO_LISTA);

    size_t tamanho_cliente = sizeof(frameSend) - sizeof(frameSend.crc);
    frameSend.crc = gencrc((uint8_t *)&frameSend, tamanho_cliente);

    printf("Filmes disponíveis:\n");
    //envia o tipo lista
    if (send(soquete, &frameSend, sizeof(frameSend), 0) == -1)
    {
        perror("Erro ao enviar mensagem! \n");
    }

    // esperando resposta do servidor
    while(1){
        if (recv(soquete, &frameRecv, sizeof(frameRecv), 0) == -1) {
            perror("Erro ao receber mensagem!");
            return;
        }

        switch(frameRecv.tipo) {
            case TIPO_ACK:
                memset(&frameRecv, 0, sizeof(frameRecv));
                break;
            case TIPO_NACK:
                printf("NACK\n");
                memset(&frameSend, 0, sizeof(frameSend));
                goto nack_msg;
                break;
            case TIPO_MOSTRA_NA_TELA:
                uint8_t crc_recebido = frameRecv.crc;
                frameRecv.crc = 0;
                uint8_t crc_calculado = gencrc((const uint8_t *)&frameRecv, sizeof(frameRecv) - sizeof(frameRecv.crc));

                // compara o CRC e a sequencia da msg, para enviar ack ou nack
                if (frameRecv.sequencia == (sequencia_esperada % 32) && crc_recebido == crc_calculado) {
                    printf("- %s",frameRecv.data);
                    //printf("Recebendo o frame de sequencia: %u e tamanho %u\n", frameRecv.sequencia, frameRecv.tamanho);

                    memset(&frameSend, 0, sizeof(frameSend));
                    init_frame(&frameSend, 0, TIPO_ACK);
                    if (send(soquete, &frameSend, sizeof(frameSend), 0) == -1) {
                        perror("Erro ao enviar mensagem\n");
                        break;
                    }
                    sequencia_esperada++;
                } else {
                    //se recebe um frame fora de ordem ou com crc errado, envia NACK
                    printf("Frame fora de ordem. Esperado: %u, Recebido: %u\n", sequencia_esperada, frameRecv.sequencia);
                    memset(&frameSend, 0, sizeof(frameSend));
                    init_frame(&frameSend, 0, TIPO_NACK);
                    if (send(soquete, &frameSend, sizeof(frameSend), 0) == -1) {
                        perror("Erro ao enviar NACK\n");
                        break;
                    }
                }
                break;
            case TIPO_FIM_TX:
                memset(&frameSend, 0, sizeof(frameSend));
                init_frame(&frameSend, 0, TIPO_ACK);
                if (send(soquete, &frameSend, sizeof(frameSend), 0) == -1) {
                    perror("Erro ao enviar mensagem\n");
                    break;
                }
                return;
        }
    }
}

void baixar(int soquete, char* nome_arquivo){
    frame_t frameSend, frameRecv;
    char *separador;
    char buffer_nome_arquivo[256];
    char data[256];
    int sequencia_esperada = 0;

    FILE *arquivo = NULL;

    init_frame(&frameSend, 0, TIPO_BAIXAR);
    strcpy(frameSend.data, nome_arquivo); //manda o nome do arquivo em 1 frame, pq ele n pode ter mais de 63 bytes

    size_t tamanho_cliente_baixar = sizeof(frameSend) - sizeof(frameSend.crc);
    printf("Tamanho dos dados para CRC no servidor: %zu\n", tamanho_cliente_baixar);
    frameSend.crc = gencrc((uint8_t *)&frameSend, tamanho_cliente_baixar);
    printf("CRC : %d\n", frameSend.crc);

    if (send(soquete, &frameSend, sizeof(frameSend), 0) == -1)
    {
        perror("Erro ao enviar mensagem! \n");
    }

    while(1){
        recv(soquete, &frameRecv, sizeof(frameRecv), 0);

        switch(frameRecv.tipo) {
            case TIPO_ACK:
                printf("ACK\n");
                memset(&frameRecv, 0, sizeof(frameRecv));
                break;
            case TIPO_DESCRITOR_ARQUIVO:
                separador = strtok(frameRecv.data, "\n"); //o nome e a data vem um em cada linha, aqui separa
                if (separador != NULL) {
                    strncpy(buffer_nome_arquivo, separador, sizeof(buffer_nome_arquivo) - 1);
                    buffer_nome_arquivo[sizeof(buffer_nome_arquivo) - 1] = '\0';

                    separador = strtok(NULL, "\n");
                    if (separador != NULL) {
                        strncpy(data, separador, sizeof(data) - 1);
                        data[sizeof(data) - 1] = '\0';
                    }
                }
                printf("Nome do arquivo: %s\n", buffer_nome_arquivo);
                printf("Data: %s\n", data);

                //manda ack se recebeu o descritor
                memset(&frameSend, 0, sizeof(frameSend));
                init_frame(&frameSend, 0, TIPO_ACK);
                if (send(soquete, &frameSend, sizeof(frameSend), 0) == -1)
                {
                    perror("Erro ao enviar mensagem! \n");
                }

                arquivo = fopen(buffer_nome_arquivo, "wb");
                if (arquivo == NULL) {
                    perror("Erro ao abrir arquivo para escrita");
                    exit(-1);
                }
                break;
            case TIPO_ERRO:
                printf("Erro ao encontrar o arquivo: %s\n", frameRecv.data);
                break;
            case TIPO_DADOS:
                //compara o que recebeu com o mod de 32 para nao passar de 5 bits
                if (frameRecv.sequencia == (sequencia_esperada % 32)) {
                        fwrite(frameRecv.data, 1, frameRecv.tamanho, arquivo);
                        printf("Recebendo o frame de sequencia: %u e tamanho %u\n", frameRecv.sequencia, frameRecv.tamanho);

                        memset(&frameSend, 0, sizeof(frameSend));
                        init_frame(&frameSend, 0, TIPO_ACK);
                        if (send(soquete, &frameSend, sizeof(frameSend), 0) == -1) {
                            perror("Erro ao enviar mensagem\n");
                            break;
                        }
                        sequencia_esperada++;
                    } else {
                        //se recebe u   m frame fora de ordem, envia NACK
                        printf("Frame fora de ordem. Esperado: %u, Recebido: %u\n", sequencia_esperada, frameRecv.sequencia);
                        memset(&frameSend, 0, sizeof(frameSend));
                        init_frame(&frameSend, 0, TIPO_NACK);
                        if (send(soquete, &frameSend, sizeof(frameSend), 0) == -1) {
                            perror("Erro ao enviar NACK\n");
                            break;
                        }
                    }
                break;
                case TIPO_FIM_TX:
                    printf("Recebimento de dados concluído.\n");
                    if (arquivo != NULL){
                        fclose(arquivo);
                    }
                    break;
                break;
        }
    }
}


int main(int argc, char **argv) {
    int soquete = cria_raw_socket("eno1"); //note: enp2s0 pc: eno1
    if (soquete == -1) {
        perror("Erro ao criar socket");
        exit(-1);
    }

    char arg[100];
    char comando[10];
    char nome_arquivo[504];

    while(1) {
        MenuCliente();
        if (fgets(arg, sizeof(arg), stdin) != NULL) {
            strtok(arg, "\n"); //remove linha

            if (sscanf(arg, "%s %89[^\n]", comando, nome_arquivo) == 2) { //separa o comando do nome do arquivo
                if (strcmp(comando, "2") == 0) {
                    printf("Baixando arquivo %s...\n", nome_arquivo);
                    baixar(soquete, nome_arquivo);
                } else {
                    printf("Mande o nome do filme após o número da operação.\n");
                }
            } else if (strcmp(arg, "1") == 0) {
                lista(soquete);
            } else if (strcmp(arg, "3") == 0) {
                printf("Saindo...\n");
                break;
            } else {
                printf("Opção inválida, tente novamente.\n");
            }
        } else {
            printf("Erro ao ler entrada.\n");
            break;
        }
    }

    close(soquete); //diz que a operacao terminou ver na imagem
    return 0;
}
