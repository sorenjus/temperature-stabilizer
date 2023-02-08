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

// Integer representing the number of external processes
int NUM_CHILDREN = -1;
// Represents the external temperature
double childtemp = 0;
// Represents the childs place in the parent arrays
int childNum = -1;
// deliminater used when parsing entered commands
char *delim = " ";
// Tokenized command string
char *token;
// Entered command from stdin
char command[256] = "";
// Represents the alpha variable
double alpha = 0.9;
// Represents the k variable
double kval = 200;
// Represents the central process temperature
double ctemp = 0;
// Contains the PID for all child processes
pid_t children[10];
// Contains the most recent child temperature for all external processes
double tempArr[10];
// Represents if the external process is enabled
bool isActive[10];
// Represents the number of currently enabled processes during current run
int activeChildren;
// Signal command for child process to stop doing work
double stop = __DBL_MIN__;
// Pipe array
int fd[20][2];
// Value of the current delay between cycles when running
int delay = 250;
// Represents if the current while loop should be exited
bool running = true;
// keep count of which token is being checked
int counter = 0;
// char pointer used to transform token to a double
char *ptr;
// Double for the sum of the external temperatures
double sum = 0;

/*Function for the child processes.
 *reads the current value of the central process temp and adjusts
 *it's own temperature accordingly and writes that value to the parent
 */
void doChildWork()
{
  while (running)
  {
    // Read the current value of the central process
    read(fd[childNum][0], &ctemp, sizeof(double));
    // if the value is equal to the stop value, exit the loop
    if (ctemp == stop)
    {
      running = false;
    }
    else
    {
      // update the child process temperature
      childtemp = (alpha * childtemp + (1 - alpha) * ctemp);
      // Round the temperature to 3 decimal places
      childtemp = floorf(childtemp * 1000) / 1000;
      // write the current value of childtemp to the parent process
      write(fd[childNum + NUM_CHILDREN][1], &childtemp, sizeof(double));
    }
  }
  // return the value of running to true so the child process does not exit all while loops
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
  // Set the counter to 0
  counter = 0;

  // the PID of the target process to update
  int targetChild;

  /* walk through tokens */
  while (token != NULL)
  {
    // First token is the target child's PID
    if (counter == 1)
    {
      targetChild = atoi(token);
    }
    // Second token is the target's temperature
    else if (counter == 2)
    {
      childtemp = strtod(token, &ptr);
    }
    // update the current token
    token = strtok(NULL, delim);
    // update the counter
    counter++;
  }
  // Iterate through the child list
  for (int i = 0; i < NUM_CHILDREN; i++)
  {
    // When the target pid matches a child process pid
    if (children[i] == targetChild)
    {
      // write to that processes pipe the update value
      write(fd[i][1], &childtemp, sizeof(double));
      // update the value of child temp in the parent process
      tempArr[i] = childtemp;
    }
  }
  return targetChild;
}

/*Function read the updated value of the external temperature in the target process*/
void setETemp()
{
  // Read the pipe from the parent process and update external temperature
  read(fd[childNum][0], &childtemp, sizeof(double));

  printf("%d: Ext. temperature set to %0.2f\n", getpid(), childtemp);
  fflush(NULL);
}

/*Function that takens the current token command and retrieves the double. Returns the read value*/
double setVal(char *token)
{
  // Set the counter to 0
  counter = 0;

  /* walk through tokens until the token that holds the double is reached*/
  while (token != NULL)
  {
    if (counter == 1)
    {
      // convert the token to a double and return
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
    // Reset the command string to an empty string
    memcpy(command, "", 256);

    // Read the next command input
    fgets(command, sizeof command, stdin);

    // Tokenize the string
    token = strtok(command, delim);

    // Check if the first token matches a command prompt
    if (!strcmp(token, "external"))
    {

      /* walk through other tokens for the number of child processes to create*/
      while (token != NULL)
      {
        // Incease the number of process counter
        NUM_CHILDREN++;
        if (NUM_CHILDREN > 0)
        {
          // Store the desired temperature into the tempArr for the specific process
          tempArr[NUM_CHILDREN - 1] = atoi(token);
        }
        token = strtok(NULL, delim);
      }
      // The process ID of the forked process
      pid_t id;

      printf("Create %d external processes\n", NUM_CHILDREN);

      // Set the number of active process to make the number of processes to create
      activeChildren = NUM_CHILDREN;

      // Create two pipes for each process
      for (int i = 0; i < NUM_CHILDREN * 2; i++)
      {
        pipe(fd[i]);
      }

      for (int k = 0; k < NUM_CHILDREN; k++)
      {
        // Fork and store the child process ID
        id = fork();

        // Check if the current process is the child process
        if (id == 0)
        {
          // Iterate through all pipes
          for (int i = 0; i < NUM_CHILDREN; i++)
          {
            // If the pipe is not for this process, close it
            if (i != k)
            {
              close(fd[i][0]);
              close(fd[i][1]);
              close(fd[i + NUM_CHILDREN][0]);
              close(fd[i + NUM_CHILDREN][1]);
            }
          }

          // Store the value this process is in all arrays
          childNum = k;

          // When SIGUSR1 is detected, enter the doChildWork function
          signal(SIGUSR1, doChildWork);
          // When SIGUSR2 is detected, enter the updateChildAlpha function
          signal(SIGUSR2, updateChildAlpha);
          // When SIGCONT is detected, enter the setETemp function
          signal(SIGCONT, setETemp);

          // Read the pipe and set the external process temp and alpha value
          read(fd[k][0], &childtemp, sizeof(double));
          read(fd[k][0], &alpha, sizeof(double));

          while (running)
          {
            // wait for signal
          }
          exit(32);
        }
        else
        {
          // close the read end of the pipe
          close(fd[k][0]);
          // close the write end of the pipe
          close(fd[k + NUM_CHILDREN][1]);
          // store the child ID to the array of PIDs
          children[k] = id;
          // set the child process to active
          isActive[k] = true;

          printf("Process %d: set initial temperature to %0.2f\n", children[k], tempArr[k]);

          // Send the desired starting temp and current alpha to the external process
          write(fd[k][1], &tempArr[k], sizeof(double));
          write(fd[k][1], &alpha, sizeof(double));
        }
      }
    }
    else if (!strcmp(token, "k"))
    {
      // Update the k values
      kval = setVal(token);
    }
    else if (!strcmp(token, "ctemp"))
    {
      // Update the central process temperature
      ctemp = setVal(token);
      printf("Central temp is now %0.2f\n", ctemp);
    }
    else if (!strcmp(token, "etemp"))
    {
      // Retrieve the target process from the command
      int target = readETemp(token);
      // Signal the target to update it's temperature
      kill(target, SIGCONT);
    }
    else if (!strcmp(token, "alpha"))
    {
      // Update the alpha value of the central process
      alpha = setVal(token);

      // Iterate through the child processes
      for (int i = 0; i < NUM_CHILDREN; i++)
      {
        // Send the new alpha value
        write(fd[i][1], &alpha, sizeof(double));
        // Signal the process to update their alpha value
        kill(children[i], SIGUSR2);
      }
    }
    else if (!strcmp(token, "start\n"))
    {
      // Controls if the temperature cycle must continue
      bool start = true;

      // True if temperatures are stabilized
      bool allMatch = false;

      // Iterate through the child processes
      for (int i = 0; i < NUM_CHILDREN; i++)
      {
        // Check if it is active
        if (isActive[i])
        {
          // Write the current central temperature to the pipe
          write(fd[i][1], &ctemp, sizeof(double));
          // Signal for the child process to start
          kill(children[i], SIGUSR1);
        }
      }
      while (start)
      {
        printf("Temperatures: [%.2f]", ctemp);

        // Iterate through child processes and print their current temp
        for (int i = 0; i < NUM_CHILDREN; i++)
        {
          if (isActive[i])
            printf(" %.2f", tempArr[i]);
        }
        printf("\n");

        // Iterate through the child processes
        for (int i = 0; i < NUM_CHILDREN; i++)
        {
          // Check if it is active
          if (isActive[i])
          {
            // Write the central temperature to the pipe
            write(fd[i][1], &ctemp, sizeof(double));
          }
        }
        // Iterate through the child processes
        for (int i = 0; i < NUM_CHILDREN; i++)
        {
          // Check if it is active
          if (isActive[i])
          {
            // Read the current external process temperature and store to the temp array
            read(fd[i + NUM_CHILDREN][0], &tempArr[i], sizeof(double));
          }
        }

        // Iterate through the child processes
        for (int i = 0; i < NUM_CHILDREN; i++)
        {
          // If the process is ative and the external temp is different from the central temp
          if ((tempArr[i] != ctemp) && isActive[i])
          {
            // leave the loop
            break;
          }
          // If the temps match and this is the final child in the array
          else if (i == NUM_CHILDREN - 1)
          {
            // Report the temperature as stabilized
            allMatch = true;
          }
        }
        // If the temperature is stable
        if (allMatch)
        {
          // Iterate through the child processes
          for (int i = 0; i < NUM_CHILDREN; i++)
          {
            // Check if it is active
            if (isActive[i])
              // Send the stop value to the child process
              write(fd[i][1], &stop, sizeof(double));
          }
          // Set the loop control value to false to exit the loop
          start = false;
        }
        else
        {
          // Set the current sum to zero
          sum = 0;
          // Iterate through the child processes
          for (int i = 0; i < NUM_CHILDREN; i++)
          {
            // Check if they are active
            if (isActive[i])
              // add the process temp to the sum
              sum += tempArr[i];
          }
          // Update the central temperature
          ctemp = (((kval * ctemp) + sum) / (activeChildren + kval));
          // Round the value to three decimal places
          ctemp = floorf(ctemp * 1000) / 1000;
        }
        // Pause the process for the delay value
        usleep(delay);
      }
      printf("The system stabilized at %0.3f\n", ctemp);
    }
    else if (!strcmp(token, "status\n"))
    {
      printf("Alpha = %.2f\tK = %.1f\tDelay = %d\nCentral temp is %.2f\n", alpha, kval, delay, ctemp);

      // For each external process, print the Number child it is, its PID, if it is active, and its temperature
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
      // Iterate through the child processes
      for (int k = 0; k < NUM_CHILDREN; k++)
      {
        // Check if the current process is active
        if (isActive[k])
        {
          // Stores child status
          int child_status;
          // Signal the child to quit
          kill(children[k], SIGINT);

          printf("%d is shutting down\n", children[k]);

          // Wait for the process to exit
          wait(&child_status);
        }
      }
      return 0;
    }
    else if (!strcmp(token, "delay"))
    {
      // Update the delay value
      delay = setVal(token);
      printf("Delay set to %d\n", delay);
    }
    else if (!strcmp(token, "disable"))
    {
      // Retrieve the target PID from the command
      token = strtok(NULL, delim);

      // Store the target process ID from the command
      int target = atoi(token);

      // Iterate through the child processes
      for (int i = 0; i < NUM_CHILDREN; i++)
      {
        // Check if this is the target process and it is active
        if ((target == children[i]) && isActive[i])
        {
          // Disable the process
          isActive[i] = false;

          // Decrease the number of child processes
          activeChildren = activeChildren - 1;

          printf("%d is disabled\n", children[i]);
        }
      }
    }
    else if (!strcmp(token, "enable"))
    {
      // Retrieve the target PID from the command
      token = strtok(NULL, delim);

      // Store the target process ID from the command
      int target = atoi(token);

      // Iterate through the child processes
      for (int i = 0; i < NUM_CHILDREN; i++)
      {
        // Check if this is the target process and it is inactive
        if ((target == children[i]) && !isActive[i])
        {
          // Enable the process
          isActive[i] = true;

          // Increment the number of child processes
          activeChildren = activeChildren + 1;

          printf("%d is enabled\n", children[i]);
        }
      }
    }
    else if (!strcmp(token, "kill"))
    {
      // Retrieve the target PID from the command
      token = strtok(NULL, delim);

      // Store the target process ID from the command
      int target = atoi(token);

      // Iterate through the child processes
      for (int i = 0; i < NUM_CHILDREN; i++)
      {
        // Check if this is the target process
        if (target == children[i])
        {
          // Set the process to inactive
          isActive[i] = false;

          // close the pipes to the process
          close(fd[i][1]);
          close(fd[i + NUM_CHILDREN][0]);

          // Signal the process to shut down and wait for it to exit
          int child_status;
          kill(children[i], SIGINT);
          printf("%d is shutting down\n", children[i]);
          wait(&child_status);
        }
      }
    }
    else
    {
      // Unknown command detected
      printf("Unknown command\n");
    }
  }
}
