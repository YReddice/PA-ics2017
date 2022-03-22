#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  /* TODO: Add more members if necessary */
  int oldValue;
  char expr[32];
  int hitTimes;

} WP;

bool new_wp(char *arg);
bool free_wp(int no);
void print_wp();
bool watch_wp();

#endif
