#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#define STDIN 0
#define MAX 100
#define MAX_SIZE 80

typedef struct
{
    int cliente_sd;
    char nome[256];
    int ativo;
} cliente;

typedef struct
{
    fd_set sala_fd;
    int limite;
    int quantidade;
    char nome[MAX_SIZE];
    int ativo;
    cliente *clientes;
} sala;

sala salas[MAX];

fd_set master, read_fds, write_fds;
struct sockaddr_in myaddr, remoteaddr;
int fdmax, sd, newfd, i, j, nbytes, yes = 1;
socklen_t addrlen;
char buf[256];

void envia_msg()
{
    // printf("enviando msg do file descriptor %d\n", i);
    for (j = 0; j <= fdmax; j++)
    {
        if (FD_ISSET(j, &master))
        {
            if ((j != i) && (j != sd))
            {
                send(j, buf, nbytes, 0);
            }
        }
    }
}

void abre_salas()
{
    for (int i = 0; i < MAX; i++)
    {
        FD_ZERO(&salas[i].sala_fd);
        salas[i].ativo = 0;
        salas[i].limite = 0;
        salas[i].quantidade = 0;
    }
}

int cria_sala(int limite, char nome_sala[])
{
    int sala_id;
    for (sala_id = 0; sala_id < MAX; sala_id++)
        if (salas[sala_id].ativo == 0)
            break;

    salas[sala_id].ativo = 1;
    salas[sala_id].limite = limite;
    strcpy(salas[sala_id].nome, nome_sala);
    salas[sala_id].clientes = malloc(limite * sizeof(cliente));

    for (int i = 0; i < limite; i++)
        salas[sala_id].clientes[i].ativo = 0;

    printf("Nova sala %d criada\n", sala_id);
    return sala_id;
}

int main(int argc, char *argv[])
{

    if (argc < 3)
    {
        printf("Digite IP e Porta para este servidor\n");
        exit(1);
    }

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    abre_salas();
    sd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = inet_addr(argv[1]);
    myaddr.sin_port = htons(atoi(argv[2]));

    memset(&(myaddr.sin_zero), '\0', 8);
    addrlen = sizeof(remoteaddr);
    bind(sd, (struct sockaddr *)&myaddr, sizeof(myaddr));

    listen(sd, 10);
    FD_SET(sd, &master);
    FD_SET(STDIN, &master);
    fdmax = sd;
    for (;;)
    {
        read_fds = master;
        select(fdmax + 1, &read_fds, NULL, NULL, NULL);
        for (i = 0; i <= fdmax; i++)
        {

            if (FD_ISSET(i, &read_fds))
            {
                if (i == sd)
                {
                    newfd = accept(sd, (struct sockaddr *)&remoteaddr, &addrlen);
                    FD_SET(newfd, &master);

                    char menu_nome[MAX_SIZE] = "Digite seu nome:\n";
                    send(newfd, &menu_nome, strlen(menu_nome), 0);
                    char nome_usuario[256], nome_sala[256];
                    int size_nome_usuario = recv(newfd, nome_usuario, 256, 0);

                    char menu_ops[MAX_SIZE] = "SELECIONE A OPCAO DESEJADA: \n 1 - Criar Sala \n 2 - Entrar em uma Sala\n";
                    send(newfd, &menu_ops, strlen(menu_ops), 0);
                    recv(newfd, buf, 256, 0);
                    int opcao_menu = atoi(buf);

                    char menu_sala_nome[MAX_SIZE] = "Digite o nome da Sala\n";
                    send(newfd, &menu_sala_nome, strlen(menu_sala_nome), 0);
                    int size_nome_sala = recv(newfd, menu_sala_nome, 256, 0);

                    if (opcao_menu == 1)
                    {
                        char menu_sala_limite[MAX_SIZE] = "Digite o limite de participantes da Sala\n";
                        send(newfd, &menu_sala_limite, strlen(menu_sala_limite), 0);
                        recv(newfd, buf, 256, 0);
                        int limite = atoi(buf);
                        int sala_id = cria_sala(limite, nome_sala);
                    }

                    if (newfd > fdmax)
                        fdmax = newfd;
                }
                else
                {
                    memset(&buf, 0, sizeof(buf));
                    nbytes = recv(i, buf, sizeof(buf), 0);
                    envia_msg();
                }
            }
        }
    }
    return (0);
}