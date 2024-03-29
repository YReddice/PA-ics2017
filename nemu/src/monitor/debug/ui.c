#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint64_t);

/* We use the `readline' library to provide more flexibility to read from stdin. */
char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_q(char *args) {
  return -1;
}

static int cmd_help(char *args);
static int cmd_si(char *args);
static int cmd_info(char *args);
static int cmd_x(char *args);
static int cmd_p(char *args);
static int cmd_w(char *args);
static int cmd_d(char *args);

static struct {
  char *name;
  char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display informations about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  /* TODO: Add more commands */
  {"si", "args:[N] the default is 1; [si N] means execute [N] instructions step by step", cmd_si},//si
  {"info", "args:[r/w]; print information about registers or watchpoint by using args[r] or args[w]", cmd_info},//info
  {"x", "args:[N] [EXPR]; scan the memory from [EXPR] to [EXPR+N*4] bytes", cmd_x},
  {"p", "args:[EXPR]; calculate the value of [EXPR]", cmd_p},
  {"w", "args:[EXPR]; set the watchpoint on [EXPR]", cmd_w},
  {"d", "args:[num]; delete the NO.[num] watchpoint if exist", cmd_d}
};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))
/* Add more cmd_ function*/
//d [num]
static int cmd_d(char *args){
  int num = 0;
  int nRet=sscanf(args, "%d", &num);
  if (nRet<=0){
    printf("wrong args in cmd_d\n");
    return 0;
  }

  int del=free_wp(num);
  if(del)
    printf("Success delete watchpoint NO.%d\n", num);
  else
    printf("delete watchpointer failed\n");
  return 0;
}

//w [EXPR]
static int cmd_w(char *args){
  if (!new_wp(args)){
    printf("set new watchpoint failed!\n");
  }
  return 0;
}

//p [EXPR] instruction
static int cmd_p(char *args){
  bool success;
  int res = expr(args,&success);
  if (success==false)
    printf("cmd_p: error in expr()\n");
  else
    printf("the value of expr is:%d\n",res);
  return 0;
}

//x [N] [EXPR] instruction
static int cmd_x(char *args){
  int len = 0;
  vaddr_t addr;
  if(sscanf(args,"%d 0x%x",&len,&addr) == EOF){
    printf("args error in cmd_x\n");
    return 0;
  }
  bool success;
  int space;//record the index of EXPR
  for (space=0;space<strlen(args);space++){
    if (args[space] == ' ')
      break;
  }
  addr = expr(args+space+1,&success);
  if (success==false)
    panic("cmd_p: error in expr()\n");
  printf("Memory:");
  for(int i=0;i<len;i++){
    if(i%4==0)
      printf("\n0x%x:  0x%02x",addr+i,vaddr_read(addr+i,1));
    else
      printf("  0x%02x",vaddr_read(addr+i,1));
  }
  printf("\n");
  return 0;
}

//info [r/w] instruction
static int cmd_info(char *args){
  char a;
  if (args==NULL){
    printf("args error: info command needs argument\n");
    return 0;
  }
  if(sscanf(args,"%c",&a) == EOF){
    printf("args error in cmd_info\n");
    return 0;
  }
  if(a=='r'){//print register
    int i;
    for (i=0;i<8;i++){
      printf("%s  0x%08x    %s  0x%04x\n",regsl[i],reg_l(i),regsw[i],reg_w(i));
      if(i<4){
        int j=i+4;
        printf("%s   0x%02x          %s  0x%02x\n",regsb[i],reg_b(i),regsb[j],reg_b(j));
      }
        
    }
    printf("eip  0x%08x\n",cpu.eip);
    return 0;
  }
  if(a=='w'){//print watchpoint
    // printf("info w command\n");
    print_wp();
    return 0;
  }
  printf("wrong args in cmd_info\n");
  return 0;
}

//si [N] instruction
static int cmd_si(char *args){
  uint64_t N=0;
  if (args==NULL){
    // printf("args is NULL\n");
    N = 1;
  }
  else{
    if(sscanf(args, "%llu",&N) == EOF){
      printf("args error in cmd_si\n");
      return 0;
    }
  }
  cpu_exec(N);
  return 0;
}

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void ui_mainloop(int is_batch_mode) {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  while (1) {
    char *str = rl_gets();
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef HAS_IOE
    extern void sdl_clear_event_queue(void);
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}
