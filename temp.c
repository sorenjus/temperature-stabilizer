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
#include <math.h>

int NUM_CHILDREN = -1;
double childtemp = 0;
int childNum = -1;

char *delim = " ";
char *token;
char command[256] = "";

double alpha = 0.9;
double kval = 200;
double ctemp = 0;
pid_t children[10];
double tempArr[10];
int tempMatch[10];

int fd[20][2];

bool running = true;

void doChildWork()
{
  while (running)
  {
    read(fd[childNum][0], &ctemp, sizeof(double));
    if (ctemp == 0)
    {
      running = false;
    }
    else
    {
      childtemp = (alpha * childtemp) + ((1 - alpha) * ctemp);
      // printf("child temp : %f\n", childtemp);
      childtemp = floorf(childtemp * 1000) / 1000;
      fflush(NULL);
      write(fd[childNum + NUM_CHILDREN][1], &childtemp, sizeof(double));
    }
  }
  running = true;
}

void updateChildAlpha()
{
  read(fd[childNum][0], &alpha, sizeof(double));
  printf("%d: set alpha to : %f\n", getpid(), alpha);
  fflush(NULL);
}

int readETemp(char *command)
{
  token = strtok(command, delim);
  int counter = 0;
  char *ptr;
  int targetChild;

  /* walk through other tokens */
  while (token != NULL)
  {
    if (counter == 1)
    {
      // printf("%f", strtod(token, &ptr));
      targetChild = atoi(token);
    }
    else if (counter == 2)
    {
      childtemp = strtod(token, &ptr);
    }
    token = strtok(NULL, delim);
    counter++;
  }
  for (int i = 0; i < NUM_CHILDREN; i++)
  {
    if (children[i] == targetChild)
    {
      write(fd[i][1], &childtemp, sizeof(double));
      tempArr[i] = childtemp;
    }
  }
  return targetChild;
}

void setETemp()
{
  read(fd[childNum][0], &childtemp, sizeof(double));
  printf("%d: Ext. temperature set to %0.2f\n", getpid(), childtemp);
  fflush(NULL);
}

double setVal(char *command)
{
  token = strtok(command, delim);
  int counter = 0;
  char *ptr;

  /* walk through other tokens */
  while (token != NULL)
  {
    if (counter == 1)
    {
      // printf("%f", strtod(token, &ptr));
      return strtod(token, &ptr);
    }
    token = strtok(NULL, delim);
    counter++;
  }
  return 0;
}

int main()
{
  while (running)
  {
    memcpy(command, "", 256);

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

      for (int i = 0; i < NUM_CHILDREN * 2; i++)
      {
        pipe(fd[i]);
      }

      for (int k = 0; k < NUM_CHILDREN; k++)
      {
        id = fork();
        if (id == 0)
        {
          // Do initialization
          int id = getpid();
          childNum = k;

          // signals
          signal(SIGUSR1, doChildWork);
          signal(SIGUSR2, updateChildAlpha);
          signal(SIGCONT, setETemp);

          read(fd[k][0], &childtemp, sizeof(double));
          read(fd[k][0], &alpha, sizeof(double));

          // printf("temp : %f\nalpha : %f\n", childtemp, alpha);

          while (running)
          {
            // wait for signal to start child work
          }
          exit(32);
        }
        else
        {
          children[k] = id;

          printf("Process %d: set initial temperature to %0.2f\n", children[k], tempArr[k]);

          write(fd[k][1], &tempArr[k], sizeof(double));
          write(fd[k][1], &alpha, sizeof(double));

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
      printf("Central temp is now %0.2f\n", ctemp);
    }
    else if (strstr(command, "etemp"))
    {
      int target = readETemp(command);
      kill(target, SIGCONT);
    }
    else if (strstr(command, "alpha"))
    {
      alpha = setVal(command);
      for (int i = 0; i < NUM_CHILDREN; i++)
      {
        write(fd[i][1], &alpha, sizeof(double));
        kill(children[i], SIGUSR2);
      }
    }
    else if (strstr(command, "start"))
    {
      bool start = true;
      bool allMatch = false;
      for (int i = 0; i < NUM_CHILDREN; i++)
      {
        write(fd[i][1], &ctemp, sizeof(double));
        kill(children[i], SIGUSR1);
      }
      while (start)
      {
        printf("Temperatures: [%.2f]", ctemp);

        for (int i = 0; i < NUM_CHILDREN; i++)
        {
          printf(" %.2f", tempArr[i]);
        }
        printf("\n");

        for (int i = 0; i < NUM_CHILDREN; i++)
        {
          write(fd[i][1], &ctemp, sizeof(double));
        }
        for (int i = 0; i < NUM_CHILDREN; i++)
        {
          read(fd[i + NUM_CHILDREN][0], &tempArr[i], sizeof(double));
          // printf("read: %f", tempArr[i]);
        }
        for (int i = 0; i < NUM_CHILDREN; i++)
        {
          if (tempArr[i] != ctemp)
          {
            break;
          }
          else if (i == NUM_CHILDREN - 1)
          {
            allMatch = true;
          }
        }
        if (allMatch)
        {
          double stop = 0;
          for (int i = 0; i < NUM_CHILDREN; i++)
          {
            write(fd[i][1], &stop, sizeof(double));
          }
          start = false;
        }
        else
        {
          // calculation for new ctemp
          int sum = 0;
          for (int i = 0; i < NUM_CHILDREN; i++)
          {
            sum += tempArr[i];
          }
          ctemp = (kval * ctemp + sum) / (NUM_CHILDREN + kval);
          ctemp = floorf(ctemp * 1000) / 1000;
        }
      }
      printf("The system stabilized at %0.3f\n", ctemp);
    }
    else if (strstr(command, "status"))
    {
      printf("Alpha = %.2f\tK = %.1f\nCentral temp is %.2f\n", alpha, kval, ctemp);
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
        printf("%d is shutting down\n", children[k]);
        wait(&child_status);
      }
      return 0;
    }
    else{
      printf("Unknown command\n");
    }
  }
}
