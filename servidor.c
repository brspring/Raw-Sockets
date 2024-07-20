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


// essa funcao so lista os arquivos, n sei como passa eles pelo frame kk
void lista_arquivos(const char *diretorio) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(diretorio);
    if (dir == NULL) {
        perror("Erro em abrir o diretorio");
        return;
    }

    printf("Filmes disponiveis '%s':\n", diretorio);
    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0){
            printf("%s\n", entry->d_name);
        }
    }

    closedir(dir);
}

int main() {
    const char *diretorio = "./filmes";

    int soquete = cria_raw_socket("lo"); //lo é loopback para envviar a msg dentro do mesmo PC, acho q eth0 é entre dois PC
    frame_t frame;

    memset(&frame, 0, sizeof(frame));

    ssize_t num_bytes = recv(soquete, &frame, sizeof(frame), 0); //recv() recebe a mensagem do cliente

    if (num_bytes == -1) {
        perror("Erro ao receber dados");
        exit(-1);
    }

    //deve ter algum jeito melhor de identificar o tipo do frame, fiz assim por enquanto
    switch(frame.tipo){
        case TIPO_ACK:
            printf("ACK\n");
            break;
        case TIPO_NACK:
            printf("NACK\n");
            break;
        case TIPO_LISTA:
        // ver melhor essa funcao
            lista_arquivos(diretorio);
            break;
        case TIPO_BAIXAR:
            printf("BAIXAR\n");
            break;
        case TIPO_MOSTRA_NA_TELA:
            printf("MOSTRA NA TELA\n");
            break;
        case TIPO_DESCRITOR_ARQUIVO:
            printf("DESCRITOR ARQUIVO\n");
            break;
        case TIPO_DADOS:
            printf("DADOS\n");
            break;
        case TIPO_FIM_TX:
            printf("FIM TX\n");
            break;
        case TIPO_ERRO:
            printf("ERRO\n");
            break;
    }

    printf("Servidor recebeu: \n");
    printf("  Marcador de Início: 0x%02X\n", frame.marcador_inicio);
    printf("  Tamanho: %u\n", frame.tamanho);
    printf("  Sequência: %u\n", frame.sequencia);
    printf("  Tipo: 0x%02X\n", frame.tipo);
    printf("  Dados: %s\n", frame.data);
    printf("  CRC: 0x%02X\n", frame.crc);

    close(soquete);  // diz que a operacao terminou ver na imagem
    return 0;
}
