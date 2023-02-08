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

//Integer representing the number of external processes
int NUM_CHILDREN = -1;
//Represents the external temperature
double childtemp = 0;
//Represents the childs place in the parent arrays
int childNum = -1;
//deliminater used when parsing entered commands
char *delim = " ";
//Tokenized command string
char *token;
//Entered command from stdin
char command[256] = "";
//Represents the alpha variable
double alpha = 0.9;
//Represents the k variable
double kval = 200;
//Represents the central process temperature
double ctemp = 0;
//Contains the PID for all child processes
pid_t children[10];
//Contains the most recent child temperature for all external processes
double tempArr[10];
//Represents if the external process is enabled
bool isActive[10];
//Represents the number of currently enabled processes during current run
int activeChildren;
//Signal command for child process to stop doing work
double stop = __DBL_MIN__;
//Pipe array
int fd[20][2];
//Value of the current delay between cycles when running
int delay = 250;
//Represents if the current while loop should be exited
bool running = true;
//keep count of which token is being checked
  int counter = 0;
  //char pointer used to transform token to a double
  char *ptr;

/*Function for the child processes.
*reads the current value of the central process temp and adjusts
*it's own temperature accordingly and writes that value to the parent
*/
void doChildWork()
{
  while (running)
  {
    //Read the current value of the central process
    read(fd[childNum][0], &ctemp, sizeof(double));
    //if the value is equal to the stop value, exit the loop
    if (ctemp == stop)
    {
      running = false;
    }
    else
    {
      //update the child process temperature
      childtemp = (alpha * childtemp) + ((1 - alpha) * ctemp);
      //Round the temperature to 3 decimal places
      childtemp = floorf(childtemp * 1000) / 1000;
      //write the current value of childtemp to the parent process
      write(fd[childNum + NUM_CHILDREN][1], &childtemp, sizeof(double));
    }
  }
  //return the value of running to true so the child process does not exit all while loops
  running = true;
}

/*Child reads the current value inside the pipe and updates it's alpha*/
void updateChildAlpha()
{
  read(fd[childNum][0], &alpha, sizeof(double));
  printf("%d: set alpha to : %f\n", getpid(), alpha);
  fflush(NULL);
}

/*Function to read the updated external process temperature from the user command. Returns the target process' PID*/
int readETemp(char *token)
{
  //the PID of the target process to update
  int targetChild;

  /* walk through tokens */
  while (token != NULL)
  {
    //First token is the target child's PID
    if (counter == 1)
    {
      targetChild = atoi(token);
    }
    //Second token is the target's temperature
    else if (counter == 2)
    {
      childtemp = strtod(token, &ptr);
    }
    //update the current token
    token = strtok(NULL, delim);
    //update the counter
    counter++;
  }
  //Iterate through the child list
  for (int i = 0; i < NUM_CHILDREN; i++)
  {
    //When the target pid matches a child process pid
    if (children[i] == targetChild)
    {
      //write to that processes pipe the update value
      write(fd[i][1], &childtemp, sizeof(double));
      //update the value of child temp in the parent process
      tempArr[i] = childtemp;
    }
  }
  return targetChild;
}

/*Function read the updated value of the external temperature in the target process*/
void setETemp()
{
  //Read the pipe from the parent process and update external temperature
  read(fd[childNum][0], &childtemp, sizeof(double));
  
  printf("%d: Ext. temperature set to %0.2f\n", getpid(), childtemp);
  fflush(NULL);
}

/*Function that takens the current token command and retrieves the double. Returns the read value*/
double setVal(char *token)
{
  /* walk through tokens until the token that holds the double is reached*/
  while (token != NULL)
  {
    if (counter == 1)
    {
      //convert the token to a double and return
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

    token = strtok(command, delim);

    if (!strcmp(token, "external"))
    {

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
      activeChildren = NUM_CHILDREN;
      for (int i = 0; i < NUM_CHILDREN * 2; i++)
      {
        pipe(fd[i]);
      }

      for (int k = 0; k < NUM_CHILDREN; k++)
      {
        id = fork();
        if (id == 0)
        {
          for (int i = 0; i < NUM_CHILDREN; i++)
          {
            if (i != k)
            {
              close(fd[i][0]);
              close(fd[i][1]);
              close(fd[i + NUM_CHILDREN][0]);
              close(fd[i + NUM_CHILDREN][1]);
            }
          }

          // Do initialization
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
          close(fd[k][0]);
          close(fd[k + NUM_CHILDREN][1]);
          children[k] = id;
          isActive[k] = true;
          printf("Process %d: set initial temperature to %0.2f\n", children[k], tempArr[k]);

          write(fd[k][1], &tempArr[k], sizeof(double));
          write(fd[k][1], &alpha, sizeof(double));

          // more code for parent here
          // close pipe, send temp to chld
        }
      }
    }
    else if (!strcmp(token, "k"))
    {
      kval = setVal(token);
    }
    else if (!strcmp(token, "ctemp"))
    {
      ctemp = setVal(token);
      printf("Central temp is now %0.2f\n", ctemp);
    }
    else if (!strcmp(token, "etemp"))
    {
      int target = readETemp(token);
      kill(target, SIGCONT);
    }
    else if (!strcmp(token, "alpha"))
    {
      alpha = setVal(token);
      for (int i = 0; i < NUM_CHILDREN; i++)
      {
        write(fd[i][1], &alpha, sizeof(double));
        kill(children[i], SIGUSR2);
      }
    }
    else if (!strcmp(token, "start\n"))
    {
      bool start = true;
      bool allMatch = false;
      for (int i = 0; i < NUM_CHILDREN; i++)
      {
        if (isActive[i])
        {
          write(fd[i][1], &ctemp, sizeof(double));
          kill(children[i], SIGUSR1);
        }
      }
      while (start)
      {
        printf("Temperatures: [%.2f]", ctemp);

        for (int i = 0; i < NUM_CHILDREN; i++)
        {
          if (isActive[i])
            printf(" %.2f", tempArr[i]);
        }
        printf("\n");

        for (int i = 0; i < NUM_CHILDREN; i++)
        {
          if (isActive[i])
            write(fd[i][1], &ctemp, sizeof(double));
        }
        for (int i = 0; i < NUM_CHILDREN; i++)
        {
          if (isActive[i])
            read(fd[i + NUM_CHILDREN][0], &tempArr[i], sizeof(double));
          // printf("read: %f", tempArr[i]);
        }
        for (int i = 0; i < NUM_CHILDREN; i++)
        {
          if ((tempArr[i] != ctemp) && isActive[i])
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
          for (int i = 0; i < NUM_CHILDREN; i++)
          {
            if (isActive[i])
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
            if (isActive[i])
              sum += tempArr[i];
          }
          ctemp = (kval * ctemp + sum) / (activeChildren + kval);
          ctemp = floorf(ctemp * 1000) / 1000;
        }
        usleep(delay);
      }
      printf("The system stabilized at %0.3f\n", ctemp);
    }
    else if (!strcmp(token, "status\n"))
    {
      printf("Alpha = %.2f\tK = %.1f\tDelay = %d\nCentral temp is %.2f\n", alpha, kval, delay, ctemp);
      if (NUM_CHILDREN > 0)
      {
        printf(" #    PID   Enabled  Temperature\n--- ------- -------  -----------\n");
        for (size_t i = 0; i < NUM_CHILDREN; i++)
        {
          int k = i + 1;

          printf("%d   %d    %s       %.2f\n", k, children[i], isActive[i] ? "YES" : "NO ", tempArr[i]);
        }
      }
    }
    else if (!strcmp(token, "quit\n"))
    {
      for (int k = 0; k < NUM_CHILDREN; k++)
      {
        if (isActive[k])
        {
          int child_status;
          kill(children[k], SIGINT);
          printf("%d is shutting down\n", children[k]);
          wait(&child_status);
        }
      }
      return 0;
    }
    else if (!strcmp(token, "delay"))
    {
      delay = setVal(token);
      printf("Delay set to %d\n", delay);
    }
    else if (!strcmp(token, "disable"))
    {
      token = strtok(NULL, delim);

      int target = atoi(token);

      for (int i = 0; i < NUM_CHILDREN; i++)
      {
        if ((target == children[i]) && isActive[i])
        {
          isActive[i] = false;
          activeChildren = activeChildren - 1;
          printf("%d is disabled\n", children[i]);
        }
      }
    }
    else if (!strcmp(token, "enable"))
    {
      token = strtok(NULL, delim);

      int target = atoi(token);

      for (int i = 0; i < NUM_CHILDREN; i++)
      {
        if ((target == children[i]) && !isActive[i])
        {
          isActive[i] = true;
          activeChildren = activeChildren + 1;
          printf("%d is enabled\n", children[i]);
        }
      }
    }
    else if (!strcmp(token, "kill"))
    {
      token = strtok(NULL, delim);

      int target = atoi(token);

      for (int i = 0; i < NUM_CHILDREN; i++)
      {
        if (target == children[i])
        {
          isActive[i] = false;
          close(fd[i][1]);
          close(fd[i + NUM_CHILDREN][0]);
          int child_status;
          kill(children[i], SIGINT);
          printf("%d is shutting down\n", children[i]);
          wait(&child_status);
        }
      }
    }
    else
    {
      printf("Unknown command\n");
    }
  }
}
