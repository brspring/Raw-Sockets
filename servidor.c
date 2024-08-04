#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <dirent.h>

#include "API-raw-socket.h"
#include "frame.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

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

void set_frame(frame_t *frame, unsigned int sequencia, unsigned int tipo) {
    frame->marcador_inicio = BIT_INICIO;
    frame->sequencia = sequencia;
    frame->tipo = tipo;
    frame->tamanho = strlen(frame->data);
    // frame->crc = gencrc((const uint8_t *)frame, sizeof(frame_t) - sizeof(frame->crc));
}

// essa funcao so lista os arquivos, n sei como passa eles pelo frame kk
void lista_arquivos(const char *diretorio, int soquete) {
    DIR *dir;
    struct dirent *entry;
    frame_t frameSend, frameRecv;
    int sequencia = 0;

    dir = opendir(diretorio);
    if (dir == NULL) {
        perror("Erro ao abrir o diretorio");
        return;
    }

    printf("Filmes disponíveis em '%s':\n", diretorio);
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            int nome_len = strlen(entry->d_name);

            strncpy(frameSend.data, entry->d_name, nome_len);
            frameSend.data[nome_len] = '\n';
            frameSend.data[nome_len + 1] = '\0';
            frameSend.tamanho = nome_len + 1;
            set_frame(&frameSend, sequencia, TIPO_MOSTRA_NA_TELA);

            if (send(soquete, &frameSend, sizeof(frameSend), 0) == -1) {
                perror("Erro ao enviar o frame");
                closedir(dir);
                return;
            }

            if (recv(soquete, &frameRecv, sizeof(frameRecv), 0) == -1) {
                perror("Erro ao receber o ACK");
                closedir(dir);
                return;
            }

            uint8_t received_crc = frameRecv.crc;
            frameRecv.crc = 0;
            uint8_t calculated_crc = gencrc((const uint8_t *)&frameRecv, sizeof(frameRecv) - sizeof(frameRecv.crc));

            if (frameRecv.tipo == TIPO_ACK && received_crc == calculated_crc) {
                sequencia++;
                sequencia %= 32; //se passa de 32 ele volta a sequencia
            } else {
                if (frameRecv.tipo == TIPO_NACK || frameRecv.tipo == TIPO_ERRO) {
                    printf("Recebido NACK ou ERRO, reenviando frame...\n");
                    continue;
                }
            }
        }
    }

    closedir(dir);
}

void set_descritor_arquivo(const char *diretorio, char *nome_arquivo, frame_t *frame) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char file_path[256];
    char file_info[256];

    dir = opendir(diretorio);
    if (dir == NULL) {
        perror("Erro ao abrir o diretório");
        return;
    }

    int found = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, nome_arquivo) == 0) {
            snprintf(file_path, sizeof(file_path), "%s/%s", diretorio, nome_arquivo);
            if (stat(file_path, &file_stat) == -1) {
                perror("Erro ao obter informações do arquivo");
                closedir(dir);
                return;
            }

            struct tm *time_info = localtime(&file_stat.st_mtime);
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);

            snprintf(file_info, sizeof(file_info), "%s\n%s\n", nome_arquivo, time_str);
            strncpy(frame->data, file_info, MAX_DATA_SIZE);
            frame->data[MAX_DATA_SIZE - 1] = '\0'; // Garante que o data esteja null-terminated

            found = 1;
            break;
        }
    }

    if (!found) {
        snprintf(frame->data, MAX_DATA_SIZE, "Arquivo não encontrado: %s\n", nome_arquivo);
    }

    closedir(dir);
}

void enviar_arquivo(const char *diretorio, char *nome_arquivo, int soquete) {
    char file_path[256];
    frame_t frameSend, frameRecv;
    int file;
    ssize_t bytes_read;
    unsigned int sequencia = 0;

    snprintf(file_path, sizeof(file_path), "%s/%s", diretorio, nome_arquivo);
    file = open(file_path, O_RDONLY);
    if (file == -1) {
        perror("Erro ao abrir o arquivo");
        return;
    }

    while ((bytes_read = read(file, frameSend.data, MAX_DATA_SIZE)) > 0) {
        set_frame(&frameSend, sequencia, TIPO_DADOS);
        frameSend.tamanho = bytes_read;

        if (send(soquete, &frameSend, sizeof(frameSend), 0) == -1) {
            perror("Erro ao enviar o frame");
            close(file);
            return;
        }

        if (recv(soquete, &frameRecv, sizeof(frameRecv), 0) == -1) {
            perror("Erro ao receber o ACK");
            close(file);
            return;
        }

        if (frameRecv.tipo == TIPO_ACK) {
            sequencia++;
            sequencia %= 32; //se passa de 32 ele volta a sequencia
        } else {
            if (frameRecv.tipo == TIPO_NACK || frameRecv.tipo == TIPO_ERRO) {
                printf("Recebido NACK ou ERRO, reenviando frame...\n");
                continue;
            }
        }
    }

    //frame fim tx
    set_frame(&frameSend, sequencia, TIPO_FIM_TX);
    if (send(soquete, &frameSend, sizeof(frameSend), 0) == -1) {
        perror("Erro ao enviar o frame de finalização");
    }

    close(file);
}

int busca_arquivo_diretorio(const char *diretorio, char *nome_arquivo) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(diretorio);
    if (dir == NULL) {
        perror("Erro ao abrir o diretório");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, nome_arquivo) == 0) {
            closedir(dir);
            return 0;
        }
    }

    closedir(dir);
    return -1;
}

void enviar_descritor(const char *diretorio, char *nome_arquivo, int soquete) {
    frame_t frameS, frameR;
    memset(&frameS, 0, sizeof(frameS));
    set_frame(&frameS, 0, TIPO_DESCRITOR_ARQUIVO);
    set_descritor_arquivo(diretorio, nome_arquivo, &frameS);
    printf("servidor enviou:\n%s", frameS.data);
    send(soquete, &frameS, sizeof(frameS), 0);

    if (recv(soquete, &frameR, sizeof(frameR), 0) == -1) {
        perror("Erro ao receber o ACK do descritor");
    }

    if (frameR.tipo == TIPO_ACK) {
        printf("ACK descritor recebido, enviar dados\n");
        enviar_arquivo(diretorio, nome_arquivo, soquete);
    } else if(frameR.tipo == TIPO_NACK){
        printf("ACK descritor recebido, enviar descritor novamente\n");
        enviar_descritor(diretorio, nome_arquivo, soquete);
    }
}

int main() {
    const char *diretorio = "./filmes";
    char* nome_arquivo;
    int soquete = cria_raw_socket("enp2s0"); //note: enp2s0 pc: eno1
    frame_t frameS;
    frame_t frameR;

    while(1){
        if(recv(soquete, &frameR, sizeof(frameR), 0) == -1)
        {
                perror("Erro ao receber dados");
                exit(-1);
        }

        //deve ter algum jeito melhor de identificar o tipo do frame, fiz assim por enquanto
        switch(frameR.tipo){
            case TIPO_LISTA:
                // recebeu tipo lista e vai confirmar isso pro cliente
                uint8_t crc_recebido = frameR.crc;
                frameR.crc = 0;
                uint8_t crc_calculado = gencrc((const uint8_t *)&frameR, sizeof(frameR) - sizeof(frameR.crc));

                printf("CRC calculado pelo servidor: %d\n", crc_calculado);
                printf("CRC recebido pelo servidor: %d\n", crc_recebido);

                if (crc_recebido != crc_calculado) {
                    printf("CRC inválido\n");
                    break;
                }

                set_frame(&frameS, 0, TIPO_ACK);
                send(soquete, &frameS, sizeof(frameS), 0);
                printf("servidor mandou ACK\n");
                //seta o frame para enviar a lista
                /*memset(&frameS, 0, sizeof(frameS));
                set_frame(&frameS, 0, TIPO_MOSTRA_NA_TELA);*/
                lista_arquivos(diretorio, soquete);

                //tirar isso dps
                /*printf("servidor enviou: %s\n", frameS.data);
                if(send(soquete, &frameS, sizeof(frameS), 0) == -1)
                {
                        perror("Erro ao enviar a lista de arquivos");
                        exit(-1);
                }*/
                break;
            case TIPO_ACK:
                memset(&frameR, 0, sizeof(frameR));
                set_frame(&frameR, 0, TIPO_ACK);
                break;
            case TIPO_BAIXAR:
                    nome_arquivo = frameR.data;
                    printf("nome do arquivo que o servidor recebeu: %s\n", nome_arquivo);

                    //se o arquivo passado existir, manda ACK
                    if(busca_arquivo_diretorio(diretorio, nome_arquivo) == 0){
                        memset(&frameS, 0, sizeof(frameS));
                        set_frame(&frameS, 0, TIPO_ACK);
                        send(soquete, &frameS, sizeof(frameS), 0);
                        printf("servidor mandou ACK\n");
                    }else{
                        memset(&frameS, 0, sizeof(frameS));
                        set_frame(&frameS, 0, TIPO_ERRO);
                        frameS.data[0] = NAO_ENCONTRADO;
                        frameS.data[1] = '\0';
                        send(soquete, &frameS, sizeof(frameS), 0);
                        printf("servidor mandou ERRO\n");
                        break;
                    }
                    //envia o descritor
                    enviar_descritor(diretorio, nome_arquivo, soquete);
                    break;
        }
        /*fim:
            memset(&frameS, 0, sizeof(frameS));
            set_frame(&frameS, 0, TIPO_FIM_TX);
            send(soquete, &frameS, sizeof(frameS), 0);*/
    }
    close(soquete);  // diz que a operacao terminou ver na imagem
    return 0;
}
