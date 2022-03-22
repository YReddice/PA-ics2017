#include "monitor/watchpoint.h"
#include "monitor/expr.h"

#define NR_WP 32

static WP wp_pool[NR_WP];
static WP *head, *free_;

static int next_no;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = &wp_pool[i + 1];
    wp_pool[i].oldValue = 0;
    wp_pool[i].hitTimes = 0;
  }
  wp_pool[NR_WP - 1].next = NULL;

  head = NULL;
  free_ = wp_pool;
  next_no = 0;
}

/* TODO: Implement the functionality of watchpoint */

bool new_wp(char *args){
  if (free_ == NULL){
    panic("error in new_wp(): free_ list is NULL");
  }
  WP* retWp = free_;
  free_ = free_->next;

  retWp->NO = next_no;
  next_no++;
  retWp->next = NULL;
  strcpy(retWp->expr, args);
  retWp->hitTimes = 0;
  bool success;
  retWp->oldValue=expr(retWp->expr, &success);
  if (success == false){
    printf("error in new_wp: expr() fault!\n");
    return false;
  }

  //put the new wp in front of head
  if (head == NULL){
    head = retWp;
  }
  else{
    retWp->next = head;
    head = retWp;
  }
  printf("Success: set watchponit NO.%d, oldValue = %d\n",retWp->NO,retWp->oldValue);
  return true;
}

bool free_wp(int num){
  WP* del_wp=NULL;
  if (head==NULL){
    printf("there is no watchpoint now\n");
    return false;
  }

  WP* temp = head;
  if (temp->NO == num){
    head = head->next;
  }
  else{
    while(temp->next!=NULL){
      if (temp->next->NO == num){
        del_wp = temp->next;
        temp->next = del_wp->next;
        break;
      }
      temp = temp->next;
    }
  }

  if(del_wp == NULL){
    printf("there is no NO.%d watchpoint\n", num);
    return false;
  }
  else{
    del_wp->next = free_;
    free_ = del_wp;
    return true;
  }
}

void print_wp(){
  if(head==NULL){
    printf("there is no watchpoint now\n");
    return;
  }
  printf("watchpoint:\n");
  printf("NO.    expr            hitTimes\n");
  WP* wptemp =head;
  while (wptemp!=NULL){
    printf("%d    %s          %d\n",wptemp->NO,wptemp->expr,wptemp->hitTimes);
    wptemp = wptemp->next;
  }
}

bool watch_wp(){//return false when the value of watchpoint changed
  bool success;
  bool flag = true;
  int result;
  if(head==NULL)
    return true;

  WP* wptemp = head;
  while (wptemp!=NULL){
    result = expr(wptemp->expr, &success);
    if (!success){
      printf("error in watch_up(): error in expr()\n");
      return false;
    }
    if(result!=wptemp->oldValue){
      wptemp->hitTimes += 1;
      printf("Hardware watchpoint %d:%s\n",wptemp->NO,wptemp->expr);
      printf("Old value:%d\nNew value:%d\n\n",wptemp->oldValue,result);
      wptemp->oldValue = result;
      flag = false;
    }
    wptemp=wptemp->next;
  }
  return flag;
}
