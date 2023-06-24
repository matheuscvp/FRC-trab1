#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 2048
#define MAX_ROOMS 5
#define MAX_ROOM_NAME 200
#define MAX_USERS_IN_ROOM 15

struct User {
    int socket;
    char username[50];
};

struct ChatRoom {
    int id;
    char name[MAX_ROOM_NAME];
    int participants;
    int maxParticipants;
    struct User users[MAX_USERS_IN_ROOM];

};

struct Client {
    int id;
    int socket;
    int roomID;
    char username[50];
};

void send_menu(int socket);
void list_rooms(int socket, struct ChatRoom rooms[], int totalRooms);
void create_room(int socket, struct ChatRoom rooms[], int* totalRooms);
void join_room(int socket, struct ChatRoom rooms[], int totalRooms, struct Client clients[], int* totalClients);
void leave_room(int socket, struct ChatRoom rooms[], int totalRooms, struct Client clients[], int totalClients);
void disconnect_client(int socket, struct ChatRoom rooms[], int totalRooms, struct Client clients[], int totalClients);
void list_users(int socket, struct ChatRoom rooms[], int totalRooms, int roomID);

int main() {
    int server_fd, max_fd, activity, i, valread, new_socket, sd;
    int client_sockets[MAX_CLIENTS] = {0};
    struct Client clients[MAX_CLIENTS];
    struct ChatRoom rooms[MAX_ROOMS];
    fd_set read_fds;
    char buffer[BUFFER_SIZE];
    char client_message[BUFFER_SIZE] = "Connected to the chatroom.\n";
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    // Criar socket do servidor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Configurar endereço do servidor
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8888);

    // Bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Failed to bind");
        exit(EXIT_FAILURE);
    }

    // Aguardar conexões
    if (listen(server_fd, 3) < 0) {
        perror("Failed to listen");
        exit(EXIT_FAILURE);
    }

    printf("Chatroom server is running...\n");

    int totalRooms = 0;
    int totalClients = 0;
    int nextClientID = 1;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        max_fd = server_fd;

        // Adicionar sockets de clientes à lista de descritores de arquivo
        for (i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];

            if (sd > 0)
                FD_SET(sd, &read_fds);

            if (sd > max_fd)
                max_fd = sd;
        }

        // Aguardar atividade nos descritores de arquivo
        activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            perror("Error occurred in select");
            exit(EXIT_FAILURE);
        }

        // Nova conexão
        if (FD_ISSET(server_fd, &read_fds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
                perror("Failed to accept");
                exit(EXIT_FAILURE);
            }

            // Enviar mensagem de boas-vindas ao novo cliente
            send(new_socket, client_message, strlen(client_message), 0);

            // Adicionar novo socket à lista de sockets de clientes
            for (i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    printf("New client connected: socket fd is %d, IP is %s, port is %d\n",
                           new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                    // Enviar menu interativo para o cliente
                    send_menu(new_socket);
                    
                    // Adicionar cliente à lista de clientes
                    struct Client newClient;
                    newClient.id = nextClientID++;
                    newClient.socket = new_socket;
                    newClient.roomID = -1;
                    clients[totalClients++] = newClient;
                    
                    break;
                }
            }
        }

        // Verificar as mensagens recebidas de clientes
        for (i = 0; i < totalClients; i++) {
            sd = clients[i].socket;

            if (FD_ISSET(sd, &read_fds)) {
                if ((valread = read(sd, buffer, BUFFER_SIZE - 1)) <= 0) {
                    // Cliente desconectado
                    getpeername(sd, (struct sockaddr *)&address, &addrlen);
                    printf("Client disconnected: socket fd is %d, IP is %s, port is %d\n",
                           sd, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                    // Remover cliente da lista de clientes
                    for (int j = i; j < totalClients - 1; j++) {
                        clients[j] = clients[j + 1];
                    }
                    totalClients--;

                    // Se o cliente estiver em uma sala, removê-lo da sala
                    if (clients[i].roomID != -1) {
                        leave_room(sd, rooms, totalRooms, clients, totalClients);
                    }

                    // Fechar o socket e marcar como 0 na lista de sockets de clientes
                    close(sd);
                    client_sockets[i] = 0;
                } else {
                    // Tratar a mensagem recebida
                    if (buffer[valread - 1] == '\n') {
                        buffer[valread - 1] = '\0';
                    }
                      // Adicionar terminador de string

                    if (clients[i].roomID == -1) {
                        // O cliente não está em uma sala
                        if (strncmp(buffer, "/1",2) == 0) {
                            // Opção: Listar Salas
                            printf("Client %d selected: Listar Salas\n", clients[i].id);
                            list_rooms(sd, rooms, totalRooms);
                        } else if (strncmp(buffer, "/2",2) == 0) {
                            // Opção: Criar Sala
                            printf("Client %d selected: Criar Sala\n", clients[i].id);
                            create_room(sd, rooms, &totalRooms);
                        } else if (strncmp(buffer, "/3",2) == 0) {
                            // Opção: Entrar em Sala
                            printf("Client %d selected: Entrar em Sala\n", clients[i].id);
                            join_room(sd, rooms, totalRooms, clients, &totalClients);
                        } else if (strncmp(buffer, "/4",2) == 0) {
                            // Opção: Sair do Chat
                            printf("Client %d selected: Sair do Chat\n", clients[i].id);
                            disconnect_client(sd, rooms, totalRooms, clients, totalClients);
                        } else {
                            // Opção inválida
                            send(sd, "Opção inválida. Tente novamente.\n", strlen("Opção inválida. Tente novamente.\n"), 0);
                            send_menu(sd);
                        }
                    // O cliente está em uma sala
                    } else {
                            // O cliente está em uma sala
                            if (strncmp(buffer, "/6", 2) == 0) {
                                // Opção: Sair da Sala
                                printf("Client %d selected: Sair da Sala\n", clients[i].id);
                                leave_room(sd, rooms, totalRooms, clients, totalClients);
                            }else if (strncmp(buffer, "/7", 2) == 0) {
                                    // Opção: Listar Usuários da Sala
                                    printf("Client %d selected: Listar Usuários da Sala\n", clients[i].id);
                                    if (clients[i].roomID != -1) {
                                        list_users(sd, rooms, totalRooms, clients[i].roomID);
                                    } else {
                                        send(sd, "Você não está em uma sala. Entre em uma sala primeiro.\n", strlen("Você não está em uma sala. Entre em uma sala primeiro.\n"), 0);
                                        send_menu(sd);
                                    }     
                            } else {
                                // Enviar mensagem para todos os outros participantes da sala
                                for (int j = 0; j < totalClients; j++) {
                                    if (clients[j].roomID == clients[i].roomID && clients[j].socket != sd) {
                                        send(clients[j].socket, buffer, strlen(buffer), 0);
                                    }
                                }
                            }
                        }
                }
            }
        }
    }

    return 0;
}

void send_menu(int socket) {
    char menu[] = "/1 - Listar Salas\n/2 - Criar Sala\n/3 - Entrar em Sala\n/4 - Sair do Chat\n/6 - Sair da sala\n/7 - Listar users da sala\n";
    send(socket, menu, strlen(menu), 0);
}

void list_rooms(int socket, struct ChatRoom rooms[], int totalRooms) {
    char response[BUFFER_SIZE];

    if (totalRooms == 0) {
        send(socket, "Não existem salas disponíveis.\n", strlen("Não existem salas disponíveis.\n"), 0);
    } else {
        send(socket, "Salas disponíveis.\n", strlen("Salas disponíveis.\n"), 0);

            for (int i = 0; i < totalRooms; i++) {
                char mensagem[3072];
                sprintf(mensagem, "ID: %d, Participantes: %d/%d", rooms[i].id, rooms[i].participants, rooms[i].maxParticipants);
                send(socket, mensagem, strlen(mensagem), 0); 
                char mensagem1[1024];
                sprintf(mensagem1, ", Nome: %s\n", rooms[i].name);
                send(socket, mensagem1, strlen(mensagem1), 0);      
            }
    }
}

void create_room(int socket, struct ChatRoom rooms[], int* totalRooms) {
    char response[BUFFER_SIZE];
    if (*totalRooms >= MAX_ROOMS) {
        sprintf(response, "Limite máximo de salas atingido. Não é possível criar uma nova sala.\n");
    } else {
        struct ChatRoom newRoom;
        newRoom.id = *totalRooms + 1;

        // Solicitar nome da sala
        send(socket, "Digite o nome da sala: ", strlen("Digite o nome da sala: "), 0);
        char roomName[MAX_ROOM_NAME];
        int valread = read(socket, roomName, MAX_ROOM_NAME - 1);
        roomName[valread - 1] = '\0';  // Adicionar terminador de string

        strcpy(newRoom.name, roomName);

        // Solicitar limite de participantes
        send(socket, "Digite o limite de participantes: ", strlen("Digite o limite de participantes: "), 0);
        char limitStr[5];
        valread = read(socket, limitStr, sizeof(limitStr) - 1);

        if (limitStr[valread - 1] == '\n') {
            limitStr[valread - 1] = '\0';
        }
        
        newRoom.maxParticipants = atoi(limitStr);

        newRoom.participants = 0;

        rooms[*totalRooms] = newRoom;
        (*totalRooms)++;

        sprintf(response, "Sala %s criada com sucesso.\n", newRoom.name);
    }
    send(socket, response, strlen(response), 0);
    send_menu(socket);
}

void join_room(int socket, struct ChatRoom rooms[], int totalRooms, struct Client clients[], int* totalClients) {
    char response[BUFFER_SIZE];
    if (totalRooms == 0) {
        sprintf(response, "Não existem salas disponíveis. Crie uma nova sala.\n");
    } else {
        // Listar salas disponíveis
        list_rooms(socket, rooms, totalRooms);

        // Solicitar ID da sala
        send(socket, "Digite o ID da sala que deseja entrar: ", strlen("Digite o ID da sala que deseja entrar: "), 0);
        char roomIDStr[5];
        int valread = read(socket, roomIDStr, sizeof(roomIDStr) - 1);
        if (valread > 0) {
            // Remover o caractere de nova linha do buffer
            if (roomIDStr[valread - 1] == '\n') {
                roomIDStr[valread - 1] = '\0';
            }

            int roomID = atoi(roomIDStr);

            // Encontrar a sala com o ID fornecido
            int found = 0;
            for (int i = 0; i < totalRooms; i++) {
                if (rooms[i].id == roomID) {
                    found = 1;

                    // Verificar se a sala atingiu o limite de participantes
                    if (rooms[i].participants >= rooms[i].maxParticipants) {
                        sprintf(response, "A sala %s já atingiu o limite de participantes. Escolha outra sala.\n", rooms[i].name);
                    } else {
                        // Associar o cliente à sala
                        for (int j = 0; j < *totalClients; j++) {
                            if (clients[j].socket == socket) {
                                clients[j].roomID = roomID;

                                // Solicitar nome de usuário
                                send(socket, "Digite seu nome de usuário: ", strlen("Digite seu nome de usuário: "), 0);
                                char username[50];
                                valread = read(socket, username, sizeof(username) - 1);
                                if (valread > 0) {
                                    // Remover o caractere de nova linha do buffer
                                    if (username[valread - 1] == '\n') {
                                        username[valread - 1] = '\0';
                                    }

                                    // Copiar o nome de usuário para a estrutura Client
                                    strncpy(clients[j].username, username, sizeof(clients[j].username));

                                    // Adicionar o usuário à sala
                                    int userIndex = rooms[i].participants;
                                    rooms[i].users[userIndex].socket = socket;
                                    strncpy(rooms[i].users[userIndex].username, username, sizeof(rooms[i].users[userIndex].username));

                                    rooms[i].participants++;
                                    break;
                                }
                            }
                        }
                        sprintf(response, "VVocê entrou na sala %s.\n", rooms[i].name);
                    }
                    break;
                }
            }

            if (!found) {
                sprintf(response, "Sala não encontrada. Tente novamente.\n");
            }
        }
    }
    send(socket, response, strlen(response), 0);
    send_menu(socket);
}


void list_users(int socket, struct ChatRoom rooms[], int totalRooms, int roomID) {
    char response[BUFFER_SIZE];
    int found = 0;
    for (int i = 0; i < totalRooms; i++) {
        if (rooms[i].id == roomID) {
            found = 1;
            sprintf(response, "UUsuários na sala %s:\n", rooms[i].name);
            for (int j = 0; j < rooms[i].participants; j++) {
                strcat(response, rooms[i].users[j].username);
                strcat(response, "\n");
            }
            break;
        }
    }

    if (!found) {
        sprintf(response, "Sala não encontrada. Tente novamente.\n");
    }

    send(socket, response, strlen(response), 0);
    send_menu(socket);
}

void leave_room(int socket, struct ChatRoom rooms[], int totalRooms, struct Client clients[], int totalClients) {
    char response[BUFFER_SIZE];
    int clientIndex = -1;
    for (int i = 0; i < totalClients; i++) {
        if (clients[i].socket == socket) {
            clientIndex = i;
            break;
        }
    }

    if (clientIndex != -1) {
        int roomID = clients[clientIndex].roomID;
        clients[clientIndex].roomID = -1;
        for (int i = 0; i < totalRooms; i++) {
            if (rooms[i].id == roomID) {
                rooms[i].participants--;
                sprintf(response, "VVocê saiu da sala %s.\n", rooms[i].name);
                send(socket, response, strlen(response), 0);
                break;
            }
        }
    } else {
        sprintf(response, "Você não está em uma sala.\n");
        send(socket, response, strlen(response), 0);
    }
    send_menu(socket);
}

void disconnect_client(int socket, struct ChatRoom rooms[], int totalRooms, struct Client clients[], int totalClients) {
    char response[BUFFER_SIZE];
    int clientIndex = -1;
    for (int i = 0; i < totalClients; i++) {
        if (clients[i].socket == socket) {
            clientIndex = i;
            break;
        }
    }

    if (clientIndex != -1) {
        // Remover cliente da lista de clientes
        for (int j = clientIndex; j < totalClients - 1; j++) {
            clients[j] = clients[j + 1];
        }
        totalClients--;

        // Se o cliente estiver em uma sala, removê-lo da sala
        if (clients[clientIndex].roomID != -1) {
            leave_room(socket, rooms, totalRooms, clients, totalClients);
        }

        // Fechar o socket e marcar como 0 na lista de sockets de clientes
        close(socket);
        for (int j = 0; j < MAX_CLIENTS; j++) {
            if (socket == clients[j].socket) {
                clients[j].socket = 0;
                break;
            }
        }

        sprintf(response, "Você saiu do chat.\n");
    } else {
        sprintf(response, "Erro ao desconectar o cliente.\n");
    }
    send(socket, response, strlen(response), 0);
    close(socket);
}
