#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <pthread.h>
#include "colors.h"

int main(int argc, char *argv[])
{
  int port = atoi(strchr(argv[1], ':') + 1);
  int pointer = strchr(argv[1], ':') - argv[1];
  char ip[pointer + 1];
  strncpy(ip, argv[1], pointer);
  ip[pointer] = '\0';

  int sock;
  sock = socket(PF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr;

  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_aton(ip, &addr.sin_addr);

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
  {
    perror("Connect");
    exit(2);
  }

  char players;

  do
  {
    read(sock, &players, sizeof(players));
  } while (players == 0);

  char track_len;
  read(sock, &track_len, sizeof(track_len));

  char ind;
  read(sock, &ind, sizeof(ind));

  char positions[players];

  while (1)
  {
    system("clear");
    char status;
    if (!read(sock, &status, sizeof(status)))
    {
      break;
    }
    if (status == 1)
    {
      break;
    }
    if (!read(sock, positions, sizeof(*positions) * players))
    {
      break;
    }
    printf("\n"); 
    for (int i = 0; i < players; i++)
    {
      printf("[");
      for (int j = 0; j < track_len; j++)
      {
        if (positions[i] > j)
        {
          printf("%s▓", colors[i]);
          printf(RESET_COLOR);
        }
        else
        {
          printf("░");
        }
      }
      printf("]");
      if (i == ind)
      {
        printf(" - its You!");
      }
      printf("\n");
    }
    sleep(1);
  }
  char leaderrboard[players];
  read(sock, leaderrboard, sizeof(*leaderrboard) * players);
  for (int i = 0; i < players; i++)
  {
    printf("%d postion - %s%d player", i + 1, colors[leaderrboard[i]], leaderrboard[i] + 1);
    if (leaderrboard[i] == ind)
    {
      printf("(You)");
    }
    printf("%s\n", RESET_COLOR);
  }

  close(sock);
}
