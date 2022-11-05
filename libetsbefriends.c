#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
// TODO: Implement your signal handling code here!

void alarm_handler(int signal, siginfo_t* info, void* ctx);

__attribute__((constructor)) void init() {
  printf("This code runs at program startup\n");

  struct sigaction sa;
  memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_sigaction = alarm_handler;
  sa.sa_flags = SA_SIGINFO;

  if (sigaction(SIGSEGV, &sa, NULL) != 0) {
    perror("sigaction failed");
    exit(2);
  }

  /*
  1. Need Alarms to go off
  2. Will need an alarm Handler
  3. Use sigaction to identify the sig fault
  4.
  */
}

void alarm_handler(int signal, siginfo_t* info, void* ctx) {
  char* str[8] = {"Try again!",          "You got this!",           "ALmost There",
                  "Tuff luck",           "give it another go!",     "Seg Error, sucks",
                  "check your pointers", "Go to the mentor session"};

  srand(time(NULL));
  int random = rand() % 8;

  printf("%s\n", str[random]);
  exit(2);
}
