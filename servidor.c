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

void set_frame(frame_t *frame, unsigned int sequencia, unsigned int tipo) {
        frame->marcador_inicio = BIT_INICIO;
        frame->sequencia = sequencia;
        frame->tipo = tipo;
        frame->tamanho = strlen(frame->data);
        frame->crc = 0;
}

// essa funcao so lista os arquivos, n sei como passa eles pelo frame kk
void lista_arquivos(const char *diretorio, frame_t *frame) {
    DIR *dir;
    struct dirent *entry;
    char *leitura_pos;
    unsigned long int bytes_lidos = 1; // inicio com 1 pq coloco o \0 no fim
    char *escrita_pos;
    escrita_pos = frame->data;
    dir = opendir(diretorio);
    if (dir == NULL) {
        perror("Erro em abrir o diretorio");
        return;
    }

    printf("Filmes disponiveis '%s':\n", diretorio);
    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0){
            leitura_pos = entry->d_name;
            unsigned int tam_nome = strlen(entry->d_name);

            int i = 0;
            while(i < tam_nome){ // enqt não enviou o nome do filme que está
                if(bytes_lidos + tam_nome + 1 <= MAX_DATA_SIZE){ // se cabe tudo
                    strcpy(escrita_pos, leitura_pos);
                    escrita_pos += tam_nome; // ajusta onde vai continuar a escrever no campo de dados
                    *escrita_pos = '\n'; // bota quebra de linha dps do nome de um filme pra fazer uma lista bonita
                    escrita_pos++;
                    bytes_lidos += tam_nome + 1;
                    i = tam_nome;
                    frame->tamanho = bytes_lidos;
                }
                else { // se não cabe o nome todo
                    strncpy(escrita_pos, leitura_pos, MAX_DATA_SIZE - bytes_lidos - 1); // adiciona o que cabe e não põe \0 no fim pq ainda vai ter q por mais caracteres pro nome desse filme
                    leitura_pos += MAX_DATA_SIZE - bytes_lidos - 1;
                    tam_nome -= MAX_DATA_SIZE - bytes_lidos - 1; // atualiza qnts caracteres ainda faltam pra por todo o nome do filme
                    frame->tamanho = MAX_DATA_SIZE;
                    // enviar mensagem pq não cabe mais nada no campo de dados -- tem que fazer ainda
                    printf("%s", frame->data); // vamos fingir que o print é enviar a msg
                    escrita_pos = frame->data; // começa a escrever no começo do campo de dados dnv pq enviou a mensagem
                    bytes_lidos = 1;
                }
            }

        }
    }
    *escrita_pos = '\0';
    escrita_pos++;
    // enviar mensagem aqui também pra quando não envia a mensagem por encher o campo de dados
    printf("%s", frame->data); // vamos fingir que o print é enviar a msg
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
        frameSend.data[bytes_read] = '\0'; // Garante que a string esteja terminada em '\0'

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
        } else {
            if (frameRecv.tipo == TIPO_NACK || frameRecv.tipo == TIPO_ERRO) {
                printf("Recebido NACK ou ERRO, reenviando frame...\n");
                continue;
            }
        }
    }

    // Envia o frame de finalização
    set_frame(&frameSend, sequencia, TIPO_FIM_TX);
    if (send(soquete, &frameSend, sizeof(frameSend), 0) == -1) {
        perror("Erro ao enviar o frame de finalização");
    }

    close(file);
}

int main() {
    const char *diretorio = "./filmes";
    char* nome_arquivo;
    int soquete = cria_raw_socket("eno1"); //lo é loopback para envviar a msg dentro do mesmo PC
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
                set_frame(&frameS, 0, TIPO_ACK);
                send(soquete, &frameS, sizeof(frameS), 0);
                printf("servidor mandou ACK\n");
                //seta o frame para enviar a lista
                memset(&frameS, 0, sizeof(frameS));
                set_frame(&frameS, 0, TIPO_LISTA);
                lista_arquivos(diretorio, &frameS);
                printf("servidor enviou: %s\n", frameS.data);
                send(soquete, &frameS, sizeof(frameS), 0);
                    while(1){
                        if(recv(soquete, &frameR, sizeof(frameR), 0) == -1)
                        {
                            perror("Erro ao receber dados");
                            exit(-1);
                        }
                        if(frameR.tipo == TIPO_ACK){
                            memset(&frameS, 0, sizeof(frameS));
                            printf("teste fim tx");
                            set_frame(&frameS, 0, TIPO_FIM_TX);
                            send(soquete, &frameS, sizeof(frameS), 0);
                            break;
                        }
                    }
                break;
                case TIPO_BAIXAR:
                    nome_arquivo = frameR.data;
                    set_frame(&frameS, 0, TIPO_ACK);
                    send(soquete, &frameS, sizeof(frameS), 0);
                    printf("servidor mandou ACK\n");

                    // Seta o frame para enviar o descritor
                    memset(&frameS, 0, sizeof(frameS));
                    set_frame(&frameS, 0, TIPO_DESCRITOR_ARQUIVO);
                    set_descritor_arquivo(diretorio, nome_arquivo, &frameS);
                    printf("servidor enviou: %s\n", frameS.data);
                    send(soquete, &frameS, sizeof(frameS), 0);

                    while (1) {
                        if (recv(soquete, &frameR, sizeof(frameR), 0) == -1) {
                            perror("Erro ao receber dados");
                            exit(-1);
                        }
                        if (frameR.tipo == TIPO_ACK) {
                            enviar_arquivo(diretorio, nome_arquivo, soquete);
                            break;
                        }
                    }
                    break;
            /*case TIPO_ACK:
                printf("ACK\n");
                break;
            case TIPO_NACK:
                printf("NACK\n");
                break;
            case TIPO_DADOS:
                printf("DADOS\n");
                break;
            case TIPO_FIM_TX:
                printf("FIM TX\n");
                break;
            case TIPO_ERRO:
                printf("ERRO\n");
                break;*/
        }
        /*
        printf("Servidor recebeu: \n");
        printf("  Marcador de Início: 0x%02X\n", frame.marcador_inicio);
        printf("  Tamanho: %u\n", frame.tamanho);
        printf("  Sequência: %u\n", frame.sequencia);
        printf("  Tipo: 0x%02X\n", frame.tipo);
        printf("  Dados: %s\n", frame.data);
        printf("  CRC: 0x%02X\n", frame.crc);
        */
    }
    close(soquete);  // diz que a operacao terminou ver na imagem
    return 0;
}
