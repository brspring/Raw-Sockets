#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include "API-raw-socket.h"
#include "frame.h"
#include "utils.h"

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
            memset(&frameSend, 0, sizeof(frameSend));
            int nome_len = strlen(entry->d_name);

            strncpy(frameSend.data, entry->d_name, nome_len);
            frameSend.data[nome_len] = '\n';
            frameSend.data[nome_len + 1] = '\0';
            frameSend.tamanho = nome_len + 1;

            init_frame(&frameSend, sequencia, TIPO_MOSTRA_NA_TELA);
            size_t tamanho_cliente = sizeof(frameSend) - sizeof(frameSend.crc);
            frameSend.crc = gencrc((uint8_t *)&frameSend, tamanho_cliente);

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

            if (frameRecv.tipo == TIPO_ACK) {
                sequencia++;
                if(sequencia > 31) {
                    sequencia = 0;
                }
            } else {
                if (frameRecv.tipo == TIPO_NACK || frameRecv.tipo == TIPO_ERRO) {
                    printf("Recebido NACK ou ERRO, reenviando frame...\n");
                    continue;
                }
            }
        }
    }

    // fim do envio dos nomes
    memset(&frameSend, 0, sizeof(frameSend));
    memset(&frameRecv, 0, sizeof(frameRecv));
    init_frame(&frameSend, sequencia, TIPO_FIM_TX);

    if (send(soquete, &frameSend, sizeof(frameSend), 0) == -1) {
        perror("Erro ao enviar frame TIPO_FIM_TX");
        closedir(dir);
        return;
    }
    if (recv(soquete, &frameRecv, sizeof(frameRecv), 0) == -1) {
        perror("Erro ao receber o ACK");
        closedir(dir);
        return;
    }

    // confirmacao de recebimento do fim_tx
    if(frameRecv.tipo == TIPO_ACK) {
        printf("Lista de arquivos enviada com sucesso!\n");
    } else {
        printf("Erro ao enviar a lista de arquivos\n");
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

            snprintf(file_info, sizeof(file_info), "%ld\n%s\n", file_stat.st_size, time_str);
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

void preparar_dados(frame_t *frame) {
    size_t new_len = 0;
    char temp_data[MAX_DATA_SIZE * 2]; // Aloca espaço suficiente para o novo tamanho
    for (size_t i = 0; i < frame->tamanho; i++) {
        temp_data[new_len++] = frame->data[i];
        if (frame->data[i] == 0x88 && frame->data[i + 1] == 0x81) {
            temp_data[new_len++] = 0xFF;
        }
    }
    memcpy(frame->data, temp_data, new_len);
    frame->tamanho = new_len;
}

void enviar_arquivo(const char *diretorio, char *nome_arquivo, int soquete) {
    char caminho_arquivo[256];
    frame_t frameSend, frameRecv;
    int file;
    ssize_t bytes_lidos;
    unsigned int sequencia = 0;
    int frame_enviado = 0;

    snprintf(caminho_arquivo, sizeof(caminho_arquivo), "%s/%s", diretorio, nome_arquivo);
    file = open(caminho_arquivo, O_RDONLY);
    if (file == -1) {
        perror("Erro ao abrir o arquivo");
        return;
    }

    while ((bytes_lidos = read(file, frameSend.data, MAX_DATA_SIZE)) > 0) {
        // verificao para saber se pode envair um frame novo
        if (!frame_enviado) {
            memset(&frameRecv, 0, sizeof(frameRecv));
            init_frame(&frameSend, sequencia, TIPO_DADOS);
            frameSend.tamanho = bytes_lidos;

            // para placa de rede nao comer os bytes
            preparar_dados(&frameSend);

            size_t tamanho_servidor = sizeof(frameSend) - sizeof(frameSend.crc);
            frameSend.crc = gencrc((uint8_t *)&frameSend, tamanho_servidor);

            frame_enviado = 1;
        }

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
            if(sequencia > 31) {
                sequencia = 0;
            }
            memset(&frameSend, 0, sizeof(frameSend));
            frame_enviado = 0;
        } else {
            if (frameRecv.tipo == TIPO_NACK) {
                printf("Recebido NACK, reenviando frame...\n");
            }
        }
    }

    //frame fim tx
    init_frame(&frameSend, sequencia, TIPO_FIM_TX);
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
    init_frame(&frameS, 0, TIPO_DESCRITOR_ARQUIVO);
    // seta o frame com o descritor do arquivo
    set_descritor_arquivo(diretorio, nome_arquivo, &frameS);
    // seta crc do descritor
    size_t tamanho = sizeof(frameS) - sizeof(frameS.crc);
    frameS.crc = gencrc((uint8_t *)&frameS, tamanho);

    // envia descritor
    if (send(soquete, &frameS, sizeof(frameS), 0) == -1) {
        perror("Erro ao enviar o descritor do arquivo");
    }

    // recebe reposta
    if (recv(soquete, &frameR, sizeof(frameR), 0) == -1) {
        perror("Erro ao receber o ACK do descritor");
    }

    if (frameR.tipo == TIPO_ACK) {
        printf("Começando envio de dados...\n");
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

                if (crc_recebido != crc_calculado) {
                    envia_nack(soquete, &frameS);
                    break;
                }else{
                    envia_ack(soquete, &frameS);
                    lista_arquivos(diretorio, soquete);
                }
                break;
            case TIPO_ACK:
                memset(&frameR, 0, sizeof(frameR));
                init_frame(&frameR, 0, TIPO_ACK);
                break;
            case TIPO_BAIXAR:
                    uint8_t crc_recebido_baixar = frameR.crc;
                    frameR.crc = 0;
                    size_t tamanho_servidor_baixar = sizeof(frameR) - sizeof(frameR.crc);
                    uint8_t crc_calculado_baixar = gencrc((const uint8_t *)&frameR, tamanho_servidor_baixar);

                    nome_arquivo = frameR.data;
                    //se o arquivo passado existir, manda ACK
                    if((busca_arquivo_diretorio(diretorio, nome_arquivo) == 0) && (crc_recebido_baixar == crc_calculado_baixar)){
                        envia_ack(soquete, &frameS);
                        enviar_descritor(diretorio, nome_arquivo, soquete);
                        break;
                    }else{
                        memset(&frameS, 0, sizeof(frameS));
                        init_frame(&frameS, 0, TIPO_ERRO);
                        send(soquete, &frameS, sizeof(frameS), 0);
                        break;
                    }
            case TIPO_NACK:
                memset(&frameR, 0, sizeof(frameR));
                break;
        }
    }
    close(soquete);  // diz que a operacao terminou ver na imagem
    return 0;
}
