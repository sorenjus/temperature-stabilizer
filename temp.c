#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

int NUM_CHILDREN = -1;
char *delim = " ";
char *token;
int counter;
bool running = true;
pid_t children[20];
double tempArr[20];

void iteration()
{
}

void doChildWork(int childNum, double temp)
{
  // Do initialization
  double childtemp = temp;
  double alpha = 0;
  int id = getpid();

  // signals
  signal(SIGUSR1, iteration);

  fflush(NULL);
  // Do main child work
  pause(); // wait for signal to start
  // Do child cleanup
}

void external(char *command)
{
}

int setVal(char *command)
{
  token = strtok(command, delim);
  counter = 0;

  /* walk through other tokens */
  while (token != NULL)
  {
    if (counter == 1)
    {
      return atoi(token);
    }
    token = strtok(NULL, delim);
    counter++;
  }
  return 0;
}

int main()
{
  double kval = 200;
  double ctemp = 0;
  double alpha = 0;

  // double temp[NUM_CHILDREN];

  int fd[6][2];

  while (running)
  {
    char command[256] = "";

    fgets(command, sizeof command, stdin);

    if (strstr(command, "external"))
    {
      token = strtok(command, delim);

      /* walk through other tokens */
      while (token != NULL)
      {
        NUM_CHILDREN++;
        if (NUM_CHILDREN > 0)
          tempArr[NUM_CHILDREN - 1] = atoi(token);
        token = strtok(NULL, delim);
      }
      pid_t id;
      printf("Create %d external processes\n", NUM_CHILDREN);

      for (int i = 0; i < NUM_CHILDREN; i++)
      {
        pipe(fd[i]);
      }

      for (int k = 0; k < NUM_CHILDREN; k++)
      {
        id = fork();
        if (id == 0)
        {
          double tempIn = 0;
          read(fd[k][0], &tempIn, sizeof(double));
          printf("%f\n", tempIn);
          doChildWork(k, tempIn);

          exit(32);
        }
        else
        {
          children[k] = id;
          printf("Process %d: set initial temperature to %f\n", children[k], tempArr[k]);
          write(fd[k][1], &tempArr[k], sizeof(double));
          // more code for parent here
          // close pipe, send temp to chld
        }
      }
    }
    else if (strstr(command, "k"))
    {
      kval = setVal(command);
    }
    else if (strstr(command, "ctemp"))
    {
      ctemp = setVal(command);
    }
    else if (strstr(command, "alpha"))
    {
      alpha = setVal(command);
    }
    else if (strstr(command, "start"))
    {
    }
    else if (strstr(command, "status"))
    {
      printf("Alpha = %.2f\tK = %.1f\nCentral temp is %.2f\n", alpha, kval, ctemp);
      printf("Parent %d\n", getpid());
      if (NUM_CHILDREN > 0)
      {
        printf(" #    PID   Enabled  Temperature\n--- ------- -------  -----------\n");
        for (size_t i = 0; i < NUM_CHILDREN; i++)
        {
          int k = i + 1;

          printf("%d   %d    YES       %.2f\n", k, children[i], tempArr[i]);
        }
      }
    }
    else if (strstr(command, "quit"))
    {
      for (int k = 0; k < NUM_CHILDREN; k++)
      {
        int child_status;
        kill(children[k], SIGINT);
        wait(&child_status);
      }
      return 0;
    }
  }
}
