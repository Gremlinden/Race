#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/sem.h>
#include "colors.h"

#define TRACK_LEN 20
#define MAX_SPEED 5

struct thread_args
{
  int sockfd;
  int semId;
  char ind;
  char *players;
  char *gameStarted;
  char *connected;
  char *positions;
  char *leaderboard;
  char *raceFinished;
};

char min(char a, char b)
{
  if (a > b)
  {
    return b;
  }
  else
  {
    return a;
  }
}

void sem(int semId, int n, int d)
{
  struct sembuf op;
  op.sem_op = d;
  op.sem_flg = 0;
  op.sem_num = n;
  semop(semId, &op, 1);
}

void semUnlock(int semId, int n)
{
  sem(semId, n, 1);
}

void semLock(int semId, int n)
{
  sem(semId, n, -1);
}

void *player_thread(void *t_args)
{
  struct thread_args *args = (struct thread_args *)t_args;
  int newsockfd = args->sockfd;
  int semId = args->semId;
  char ind = args->ind;
  char *players = args->players;
  char *gameStarted = args->gameStarted;
  char *connected = args->connected;
  char *positions = args->positions;
  char *raceFinished = args->raceFinished;
  char *leaderboard = args->leaderboard;
  free(t_args);

  while (!(*gameStarted))
  {
    char sync = 0;
    if (!write(newsockfd, &sync, sizeof(sync)))
    {
      (*connected) = 0;
      (*players)--;
      return NULL;
    }

    sleep(1);
  }
  semUnlock(semId, 1 + ind);
  semLock(semId, 0);

  char track_len = TRACK_LEN;
  write(newsockfd, players, sizeof(*players));
  write(newsockfd, &track_len, sizeof(track_len));
  write(newsockfd, &ind, sizeof(ind));

  char status = 0;
  while (!(*raceFinished))
  {
    write(newsockfd, &status, sizeof(status));
    (positions[ind]) += min(rand() % MAX_SPEED + 1, TRACK_LEN - positions[ind]);
    sleep(1);
    write(newsockfd, positions, sizeof(*positions) * (*players));
  }

  status = 1;
  write(newsockfd, &status, sizeof(status));
  write(newsockfd, leaderboard, sizeof(*leaderboard) * (*players));
  semUnlock(semId, 0);
}

int main(int argc, char *argv[])
{
  int listener, sock;

  listener = socket(PF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr, cliaddr;

  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(atoi(argv[1]));
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0)
  {
    addr.sin_port = 0;
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
      perror("Bind");
      close(listener);
      exit(2);
    }
  }

  char gameStarted = 0;
  char players = 0;
  char playerSlots = atoi(argv[2]);
  char playersConnected[playerSlots];
  char playersPositions[playerSlots];
  char playersFinished[playerSlots];
  char leaderrboard[playerSlots];
  char leaderrboardInd = 0;
  bzero(playersConnected, sizeof(*playersConnected) * playerSlots);
  bzero(playersPositions, sizeof(*playersPositions) * playerSlots);
  bzero(playersFinished, sizeof(*playersFinished) * playerSlots);

  socklen_t servlen = sizeof(addr);
  getsockname(listener, (struct sockaddr *)&addr, &servlen);
  printf("Listening on port: %d\n", ntohs(addr.sin_port));
  listen(listener, 10);

  int semId = semget(IPC_PRIVATE, 1 + playerSlots, 0600 | IPC_CREAT);

  char globalRaceFinished = 0;
  while (players < playerSlots)
  {
    socklen_t clilen = sizeof(cliaddr);
    sock = accept(listener, (struct sockaddr *)&cliaddr, &clilen);
    struct thread_args *new_thread_args = malloc(sizeof(struct thread_args));
    new_thread_args->sockfd = sock;
    new_thread_args->semId = semId;
    new_thread_args->gameStarted = &gameStarted;
    new_thread_args->players = &players;
    new_thread_args->raceFinished = &globalRaceFinished;
    int i;
    for (i = 0; i < playerSlots && playersConnected[i]; i++)
      ;
    if (i >= playerSlots)
      continue;
    new_thread_args->ind = i;
    new_thread_args->connected = playersConnected + i;
    new_thread_args->positions = playersPositions;
    new_thread_args->leaderboard = leaderrboard;

    players++;
    playersConnected[i] = 1;

    pthread_t thread;
    pthread_create(&thread, NULL, player_thread, new_thread_args);
  }

  gameStarted = 1;

  for (int i = 1; i <= playerSlots; i++)
  {
    semLock(semId, i);
  }

  sem(semId, 0, playerSlots);
  char raceFinished;
  do
  {
    raceFinished = 1;
    for (int i = 0; i < playerSlots; i++)
    {
      if (playersPositions[i] < TRACK_LEN)
      {
        raceFinished = 0;
      }
      else
      {
        if (!playersFinished[i])
        {
          playersFinished[i] = 1;
          leaderrboard[leaderrboardInd] = i;
          leaderrboardInd++;
        }
      }
      printf("[");
      for (int j = 0; j < TRACK_LEN; j++)
      {
        if (playersPositions[i] > j)
        {
          printf("%s▓", colors[i]);
          printf(RESET_COLOR);
        }
        else
        {
          printf("░");
        }
      }
      printf("]\n");
    }
    sleep(1);
    system("clear");

  } while (!raceFinished);
  globalRaceFinished = 1;

  for (int i = 0; i < playerSlots; i++)
  {
    printf("%d postion - %s%d player%s\n", i + 1, colors[leaderrboard[i]], leaderrboard[i] + 1, RESET_COLOR);
  }

  sem(semId, 0, -players);
  semctl(semId, 0, IPC_RMID);
}
