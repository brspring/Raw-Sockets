#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <string.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <sys/time.h>
#include <sys/types.h>
#include <linux/if_packet.h>
#include <pwd.h>

#include "API-raw-socket.h"
#include "frame.h"
#include "utils.h"

#define BUFFER_SIZE 1024

void processar_dados(frame_t *frame) {
    size_t new_len = 0;
    for (size_t i = 0; i < frame->tamanho; i++) {
        frame->data[new_len++] = frame->data[i];
        if (frame->data[i] == 0x88 && frame->data[i + 1] == 0x81 && frame->data[i + 2] == 0xFF) {
            i += 2; // Skip the next 0xFF
        }
    }
    frame->tamanho = new_len;
}

int recv_with_timeout(int socket, frame_t *frame, int timeout_sec) {
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

void MenuCliente(){
    printf("-----------------------------\n");
    printf("Cliente Iniciado\n");
    printf(" 1 - Listar arquivos\n");
    printf(" 2 - Baixar Arquivo\n");
    printf(" 3 - Sair\n");
    printf("-----------------------------\n");
    printf("Seu comando: ");
}

void lista(int soquete){
    frame_t frameSend;
    frame_t frameRecv;
    int sequencia_esperada = 0;

    nack_msg:

    //inicializa o frame tipo lista
    init_frame(&frameSend, 0, TIPO_LISTA);
    size_t tamanho_cliente = sizeof(frameSend) - sizeof(frameSend.crc);
    frameSend.crc = gencrc((uint8_t *)&frameSend, tamanho_cliente);

    //envia o tipo lista para o servidor
    if (send(soquete, &frameSend, sizeof(frameSend), 0) == -1)
    {
        perror("Erro ao enviar mensagem! \n");
    }

    printf("Filmes disponíveis:\n");

    // esperando resposta do servidor
    while(1){
        // faz controle por timeout
        if ((recv_with_timeout(soquete, &frameRecv, 5) == -1)) {
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
                    // printa o nome dos filme recebido
                    printf("- %s",frameRecv.data);
                    envia_ack(soquete, &frameSend);
                    sequencia_esperada++;
                } else {
                    //se recebe um frame fora de ordem ou com crc errado, envia NACK
                    envia_nack(soquete, &frameSend);
                }
                break;
            case TIPO_FIM_TX:
                envia_ack(soquete, &frameSend);
                return;
        }
    }
}

void open_with_celluloid(const char *output_filename) {
    if (chmod(output_filename, 0666) != 0) {
        perror("Erro ao ajustar permissões do arquivo");
        exit(EXIT_FAILURE);
    }

    char *logged_user = getlogin();
    if (logged_user == NULL) {
        perror("Failed to get login name");
        exit(EXIT_FAILURE);
    }

    struct passwd *pwd = getpwnam(logged_user);
    if (pwd == NULL) {
        perror("Failed to get user information");
        exit(EXIT_FAILURE);
    }

    uid_t uid = pwd->pw_uid;
    gid_t gid = pwd->pw_gid;

    if (chown(output_filename, uid, gid) != 0) {
        perror("Failed to change file ownership");
        exit(EXIT_FAILURE);
    }

    char command[256];
    snprintf(command, sizeof(command), "sudo -u %s celluloid %s", logged_user, output_filename);

    int result = system(command);
    if (result == -1) {
        fprintf(stderr, "Error executing system command\n");
    } else {
        printf("Celluloid player opened successfully\n");
    }
}

void baixar(int soquete, char* nome_arquivo){
    frame_t frameSend, frameRecv;
    char *separador;
    char buffer_tamanho[256];
    char data[50];
    char caminho_arquivo[256];
    int sequencia_esperada = 0;

    FILE *arquivo = NULL;

    snprintf(caminho_arquivo, sizeof(caminho_arquivo), "./%s", nome_arquivo);

    init_frame(&frameSend, 0, TIPO_BAIXAR);
    strcpy(frameSend.data, nome_arquivo); //manda o nome do arquivo em 1 frame, pq ele n pode ter mais de 63 bytes
    size_t tamanho_cliente_baixar = sizeof(frameSend) - sizeof(frameSend.crc);
    frameSend.crc = gencrc((uint8_t *)&frameSend, tamanho_cliente_baixar);

    if (send(soquete, &frameSend, sizeof(frameSend), 0) == -1)
    {
        perror("Erro ao enviar mensagem! \n");
    }

    while(1){
        if (recv(soquete, &frameRecv, sizeof(frameRecv), 0) == -1) {
            perror("Erro ao receber o ACK do descritor");
        }

        switch(frameRecv.tipo) {
            case TIPO_ACK:
                memset(&frameRecv, 0, sizeof(frameRecv));
                break;
            case TIPO_DESCRITOR_ARQUIVO:
                uint8_t crc_recebido_desc = frameRecv.crc;
                frameRecv.crc = 0;
                uint8_t crc_calculado_desc = gencrc((const uint8_t *)&frameRecv, sizeof(frameRecv) - sizeof(frameRecv.crc));

                //manda ack se recebeu o descritor
                if (crc_calculado_desc == crc_recebido_desc)
                {
                    envia_ack(soquete, &frameSend);

                    separador = strtok(frameRecv.data, "\n"); //o tamanho e a data vem um em cada linha, aqui separa
                    if (separador != NULL) {
                        // apenas arruma para mostrar na tela
                        strncpy(buffer_tamanho, separador, sizeof(buffer_tamanho) - 1);
                        buffer_tamanho[sizeof(buffer_tamanho) - 1] = '\0';

                        separador = strtok(NULL, "\n");
                        if (separador != NULL) {
                            strncpy(data, separador, sizeof(data) - 1);
                            data[sizeof(data) - 1] = '\0';
                        }
                    }
                    printf("Tamanho do arquivo: %s\n", buffer_tamanho);
                    printf("Data: %s\n", data);

                    // abre o arquivo para comecar a enviar seus dados
                    arquivo = fopen(nome_arquivo, "wb");
                    if (arquivo == NULL) {
                        perror("Erro ao abrir arquivo para escrita");
                        exit(-1);
                    }
                    break;
                } else {
                    envia_ack(soquete, &frameSend);
                }
            case TIPO_ERRO:
                printf("Filme indisponível\n");
                return;
                break;
            case TIPO_DADOS:
                //compara o que recebeu com o mod de 32 para nao passar de 5 bits
                uint8_t crc_recebido_dados = frameRecv.crc;
                frameRecv.crc = 0;
                uint8_t crc_calculado_dados = gencrc((const uint8_t *)&frameRecv, sizeof(frameRecv) - sizeof(frameRecv.crc));

                if (crc_calculado_dados == crc_recebido_dados) {
                        processar_dados(&frameRecv);
                        fwrite(frameRecv.data, 1, frameRecv.tamanho, arquivo);

                        envia_ack(soquete, &frameSend);
                        sequencia_esperada++;
                        if (sequencia_esperada > 31){
                            sequencia_esperada = 0;
                        }
                    } else {
                        // envia NACK erro no crc
                        envia_nack(soquete, &frameSend);
                    }
                break;
                case TIPO_FIM_TX:
                    printf("Recebimento de dados concluído.\n");
                    if (arquivo != NULL){
                        //open_with_celluloid(caminho_arquivo);
                        fclose(arquivo);
                    }
                    return;
                break;
        }
    }
}

int main(int argc, char **argv) {
    //conexao raw socket
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
            //opcao 1 de listar os arquivos "01010"
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
