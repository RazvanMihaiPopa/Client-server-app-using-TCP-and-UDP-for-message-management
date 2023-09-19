/*
 * Protocoale de comunicatii
 * Laborator 7 - TCP
 * Echo Server
 * server.c
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include "common.h"
#include "helpers.h"

#define MAX_CONNECTIONS 32

int get_subscriber(int sockfd, Subscriber *subscribers, int num_subscribers) {
  for (int i = 0; i < num_subscribers; i++) {
    if (subscribers[i].socket == sockfd) {
      return i;
    }
  }
  return -1;
}

int get_topic(Subscriber subscriber, Topic topic) {
  for (int i = 0; i < subscriber.num_topics; i++) {
    if (strcmp(topic.name, subscriber.topics[i].name) == 0) {
      return i;
    }
  }
  return -1;
}

int subscribed_to_topic(Subscriber subscriber, Topic topic) {
  for (int i = 0; i < subscriber.num_topics; i++) {
    if (strcmp(subscriber.topics[i].name, topic.name) == 0) {
      return 1;
    }
  }
  return 0;
}

int send_message(int socket, char *message) {
  struct chat_packet mes;
  strcpy(mes.message, message);
  mes.len = strlen(message) + 1;
  int rc = send_all(socket, &mes, sizeof(mes));
  return rc;
}

// Primeste date de pe connfd1 si trimite mesajul receptionat pe connfd2
int receive_and_send(int connfd1, int connfd2, size_t len) {
  int bytes_received;
  char buffer[len];

  // Primim exact len octeti de la connfd1
  bytes_received = recv_all(connfd1, buffer, len);
  // S-a inchis conexiunea
  if (bytes_received == 0) {
    return 0;
  }
  DIE(bytes_received < 0, "recv");

  // Trimitem mesajul catre connfd2
  int rc = send_all(connfd2, buffer, len);
  if (rc <= 0) {
    perror("send_all");
    return -1;
  }

  return bytes_received;
}

void run_chat_multi_server(int tcp_listenfd, int udp_listenfd) {

  struct pollfd poll_fds[MAX_CONNECTIONS];
  int num_clients = 3;
  int rc;
  Subscriber subscribers[MAX_CONNECTIONS];
  int num_subscribers = 0;

  struct chat_packet received_packet;

  // Setam socket-ul tcp_listenfd pentru ascultare
  rc = listen(tcp_listenfd, MAX_CONNECTIONS);
  DIE(rc < 0, "listen");

  // se adauga noul file descriptor (socketul pe care se asculta conexiuni) in
  // multimea read_fds
  poll_fds[0].fd = tcp_listenfd;
  poll_fds[0].events = POLLIN;

  poll_fds[1].fd = udp_listenfd;
  poll_fds[1].events = POLLIN;

  poll_fds[2].fd = STDIN_FILENO;
  poll_fds[2].events = POLLIN;

  // serverul este deschis
  int open_server = 0;

  setvbuf(stdout, NULL, _IONBF, 2000);

  while (1) {
    if (open_server == -1) {
      break;
    }

    rc = poll(poll_fds, num_clients, -1);
    DIE(rc < 0, "poll");

    for (int i = 0; i < num_clients; i++) {
      if (poll_fds[i].revents & POLLIN) {
        if (poll_fds[i].fd == tcp_listenfd) {
          // a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
          // pe care serverul o accepta
          struct sockaddr_in cli_addr;
          socklen_t cli_len = sizeof(cli_addr);
          int newsockfd =
              accept(tcp_listenfd, (struct sockaddr *)&cli_addr, &cli_len);
          DIE(newsockfd < 0, "accept");

           // Dezactivam algoritmul Nagle
          int optval = 1;
          rc = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&optval, sizeof(int));
          DIE(rc < 0, "Dezactivarea algoritmului Nagle a esuat");

          // se adauga noul socket intors de accept() la multimea descriptorilor
          // de citire
          poll_fds[num_clients].fd = newsockfd;
          poll_fds[num_clients].events = POLLIN;
          num_clients++;

          // primim ID-ul noului subscriber
          memset(&received_packet, 0, sizeof(received_packet));
          rc = recv_all(newsockfd, &received_packet, sizeof(received_packet));
          DIE(rc < 0, "Nu s-a primit ID-ul");

          int subIndex = -1;
          for (int j = 0; j < num_subscribers; j++) {
            if (strcmp(received_packet.message, subscribers[j].id) == 0) {
              subIndex = j;
            }
          }
          if (subIndex == -1) {
            strcpy(subscribers[num_subscribers].id, received_packet.message);
            subscribers[num_subscribers].socket = newsockfd;
            subscribers[num_subscribers].online = 1;
            num_subscribers++;
            // afisam mesajul de conectare
            printf("New client %s connected from %s:%d.\n", received_packet.message,
                  inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
          } else if (subscribers[subIndex].online == 1) {
            printf("Client %s already connected.\n", received_packet.message);
            struct chat_packet mes;
            strcpy(mes.message, "exit");
            mes.len = strlen(mes.message) + 1;
            int rc = send_all(newsockfd, &mes, sizeof(mes));
            DIE(rc < 0, "Nu s-a inchis");
            close(newsockfd);
            num_clients--;
            poll_fds[num_clients].fd = -1;
          } else if (subscribers[subIndex].online == 0) {
            subscribers[subIndex].online = 1;
            subscribers[subIndex].socket = newsockfd;
            // afisam mesajul de conectare
            printf("New client %s connected from %s:%d.\n", received_packet.message,
                  inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
          }

        } else if (poll_fds[i].fd == udp_listenfd) {

          struct sockaddr_in client_addr;
          socklen_t clen = sizeof(client_addr);

          // am primit un mesaj de la udp
          char buffer[1552];
          memset(buffer, 0, sizeof(buffer));
          rc = recvfrom(udp_listenfd, buffer, sizeof(buffer), 0,
                    (struct sockaddr *)&client_addr, &clen);
          DIE(rc < 0, "Nu s-a primit mesajul");
          char send[1800];
          memset(send, 0, sizeof(send));

          // Luam topicul
          Topic topic;
          memcpy(topic.name, buffer, 50);
          topic.name[50] = '\0';

          // verificam tipul si continutul
          char type = buffer[50];
          if (type == 0) {
            // la buffer + 51 - bit de semn, buffer + 52 - numar
            uint32_t nr;
            memcpy(&nr, buffer + 52, sizeof(uint32_t));
            nr = ntohl(nr);
            // inmultim sau nu cu -1
            if (buffer[51] == 1) {
              nr = nr * (-1);
            }
            sprintf(send, "%s:%d - %s - INT - %d", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), topic.name, nr);
          } else if (type == 1) {
            // buffer + 51 - uint16_t
            uint16_t nr;
            memcpy(&nr, buffer + 51, sizeof(uint16_t));
            float divided = ntohs(nr) / 100.0;
            sprintf(send, "%s:%d - %s - SHORT_REAL - %.2f", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), topic.name, divided);
          } else if (type == 2) {
            // la buffer + 51 - bit de semn, buffer + 52 - uint32_t, buffer + 56 - putere
            uint32_t nr;
            memcpy(&nr, buffer + 52, sizeof(uint32_t));
            nr = ntohl(nr);
            float multiplier = 1;
            uint32_t power;
            memcpy(&power, buffer + 56, sizeof(uint32_t));
            for (int i = 0; i < power; i++) {
              multiplier *= 10;
            }
            float actual_nr = (float)nr / multiplier;
            // inmultim sau nu cu -1
            if (buffer[51] == 1) {
              actual_nr = actual_nr * (-1);
            }
            sprintf(send, "%s:%d - %s - FLOAT - %f", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), topic.name, actual_nr);
          } else if (type == 3) {
            // buffer + 51 - sir de caractere
            sprintf(send, "%s:%d - %s - STRING - %s", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), topic.name, buffer + 51);
          }

          // trimitem la toti
          for (int j = 0; j < num_subscribers; j++) {
            if (subscribers[j].online == 1 && subscribed_to_topic(subscribers[j], topic)) {
              struct chat_packet sendp;
              memset(&sendp, 0, sizeof(sendp));
              memcpy(sendp.message, send, strlen(send));
              sendp.len = strlen(send) + 1;
              rc = send_all(subscribers[j].socket, &sendp, sizeof(sendp));
              DIE(rc < 0, "Nu s-a putut trimite mesajul la subscriber");
            }
          }

        } else if (poll_fds[i].fd == STDIN_FILENO) {

          // se primesc date de la tastatura
          memset(&received_packet, 0, sizeof(received_packet));
          fgets(received_packet.message, MSG_MAXSIZE - 1, stdin);
          if (strncmp(received_packet.message, "exit", 4) == 0) {
            // se inchid serverul si conexiunile
            open_server = -1;
            for (int j = 3; j < num_clients; j++) {
              struct chat_packet exit;
              strcpy(exit.message, "exit");
              exit.len = strlen(exit.message) + 1;
              rc = send_all(poll_fds[j].fd, &exit, sizeof(exit));
              close(poll_fds[j].fd);
            }
            close(poll_fds[0].fd);
            close(poll_fds[1].fd);
            close(poll_fds[2].fd);
            break;
          } else {
            // putem primi doar comanda exit
            printf("You can only exit. Everything else will not be considered.\n");
          }

        } else {
          // s-au primit date pe unul din socketii de client,
          // asa ca serverul trebuie sa le receptioneze
          int rc = recv_all(poll_fds[i].fd, &received_packet,
                            sizeof(received_packet));
          DIE(rc < 0, "recv");
          char *p = strtok(received_packet.message, " \n\0");
          struct chat_packet send;
          
          // Se primeste comanda de subscribe 
          if (strcmp(p, "subscribe") == 0) {
            // extragem topicul si sf
            p = strtok(NULL, " ");
            char topic[51];
            strcpy(topic, p);
            char sf; 
            p = strtok(NULL, " \0\n");
            sf = *p;
            int index = get_subscriber(poll_fds[i].fd, subscribers, num_subscribers);
            Topic new_topic;
            strcpy(new_topic.name, topic);
            new_topic.sf = atoi(&sf);
            // daca topicul nu exista, il adaugam
            memset(&send, 0, sizeof(send));
            if (!subscribed_to_topic(subscribers[index], new_topic)) {
              subscribers[index].topics[subscribers[index].num_topics] = new_topic;
              subscribers[index].num_topics++;
              memcpy(send.message, "Subscribed to topic.", 20);
              send.len = 20;
            } else {
              memcpy(send.message, "You're already subscribed to this topic.", 40);
              send.len = 40;
            }
            rc = send_all(poll_fds[i].fd, &send, sizeof(send));
            DIE (rc < 0, "Nu s-a putut trimite");
          } else if (strcmp(p, "unsubscribe") == 0) {
            // pentru unsubscribe, extragem topicul
            p = strtok(NULL, " \n\0");
            char *topic = p;
            // subscriber index
            int index = get_subscriber(poll_fds[i].fd, subscribers, num_subscribers);
            Topic new_topic;
            strcpy(new_topic.name, topic);
            // topic index
            int t_index = get_topic(subscribers[i], new_topic);
            if (index == -1) {
              memcpy(send.message, "You aren't subscribed to this topic.", 36);
              send.len = 36;
            } else {
              // daca este ultimul in vector
              if (t_index == subscribers[index].num_topics - 1) {
                memset(&subscribers[index].topics[t_index], 0, sizeof(subscribers[index]).topics[t_index]);
              } else {
                for (int i = t_index; i < subscribers[index].num_topics - 1; i++) {
                  Topic aux = subscribers[index].topics[i];
                  subscribers[index].topics[i] = subscribers[index].topics[i + 1];
                  subscribers[index].topics[i + 1] = aux;
                }
              }
              subscribers[index].num_topics--;
              memcpy(send.message, "Unsubscribed from topic.", 24);
              send.len = 24;
            }
            rc = send_all(poll_fds[i].fd, &send, sizeof(send));
            DIE (rc < 0, "Nu s-a putut trimite");

          } else if (strcmp(p, "exit") == 0) {
            // conexiunea s-a inchis
            int index = get_subscriber(poll_fds[i].fd, subscribers, num_subscribers);
            printf("Client %s disconnected.\n", subscribers[index].id);
            subscribers[index].online = 0;
            subscribers[index].socket = -1;
            close(poll_fds[i].fd);

            // se scoate din multimea de citire socketul inchis
            for (int j = i; j < num_clients - 1; j++) {
              poll_fds[j].events = poll_fds[j + 1].events;
              poll_fds[j].revents = poll_fds[j + 1].revents;
              poll_fds[j].fd = poll_fds[j + 1].fd;
            }

            num_clients--;
          }
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("\n Usage: %s <port>\n", argv[0]);
    return 1;
  }

  // Parsam port-ul ca un numar
  uint16_t port;
  int rc = sscanf(argv[1], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  // Obtinem un socket TCP pentru receptionarea conexiunilor
  int tcp_listenfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(tcp_listenfd < 0, "socket");

  // Obtinem un socket UDP pentru receptionarea mesajelor
  int udp_listenfd = socket(AF_INET, SOCK_DGRAM, 0);
  DIE(udp_listenfd < 0, "socket");

  // Completăm in serv_addr adresa serverului, familia de adrese si portul
  // pentru conectare
  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  // Facem adresa socket-ului reutilizabila, ca sa nu primim eroare in caz ca
  // rulam de 2 ori rapid
  int enable = 1;
  if (setsockopt(tcp_listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");
  enable = 1;
  if (setsockopt(udp_listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");

  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  // Asociem adresa serverului cu socketul TCP creat folosind bind
  rc = bind(tcp_listenfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "bind");

  // // Asociem adresa serverului cu socketul UDP creat folosind bind
  rc = bind(udp_listenfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "bind");

  run_chat_multi_server(tcp_listenfd, udp_listenfd);

  // Inchidem tcp_listenfd si udp_listenfd
  close(tcp_listenfd);
  close(udp_listenfd);

  return 0;
}
