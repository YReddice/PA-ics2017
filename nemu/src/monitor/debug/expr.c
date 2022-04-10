#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>

enum {
  TK_NOTYPE = 256, TK_EQ,

  /* TODO: Add more token types */
  TK_HEXNUMBER,
  TK_REGISTER,
  TK_NUMBER,
  TK_NEQ,
  TK_AND,
  TK_OR,
  TK_NOT,
  TK_DEREF,
  TK_MINUS   
};

static struct rule {
  char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},    // spaces
  {"\\+", '+'},         // plus
  {"==", TK_EQ},         // equal
  {"!=", TK_NEQ},
  {"&&", TK_AND},
  {"\\|\\|", TK_OR},
  {"0x0|0x[0-9a-fA-F]*", TK_HEXNUMBER},
  {"0|[1-9][0-9]*", TK_NUMBER},
  {"-", '-'},
  {"\\*", '*'},
  {"\\/", '/'},
  {"\\(", '('},
  {"\\)", ')'},
  {"!", TK_NOT},
  {"\\$(eax|ebx|ecx|edx|esp|ebp|esi|edi|eip|ax|bx|cx|dx|sp|bp|si|di|al|bl|cl|dl|ah|bh|ch|dh)", TK_REGISTER}
  //distinguishing of '*' and '-' not in here, it's in expr()
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

Token tokens[32];
int nr_token;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        // Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            // i, rules[i].regex, position, substr_len, substr_len, substr_start);
        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
          case TK_NOTYPE://ignore space
            nr_token--;
            break;
          case TK_NUMBER://number case
            tokens[nr_token].type = TK_NUMBER;
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            *(tokens[nr_token].str+substr_len)='\0';
            break;
          case TK_HEXNUMBER://hex number case
            tokens[nr_token].type = TK_HEXNUMBER;
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            *(tokens[nr_token].str+substr_len)='\0';
            break;
          case TK_REGISTER://register case
            tokens[nr_token].type = TK_REGISTER;
            strncpy(tokens[nr_token].str, substr_start+1, substr_len-1);//don't put '$' in str
            *(tokens[nr_token].str+substr_len-1)='\0';
            break;
          default: 
            tokens[nr_token].type = rules[i].token_type;
        }
        nr_token++;
        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

int regi_char2value(int p){
  for (int i=0;i<8;i++){
    if (strcmp(tokens[p].str, regsl[i])==0)
      return reg_l(i);
    if (strcmp(tokens[p].str, regsw[i])==0)
      return reg_w(i);
    if (strcmp(tokens[p].str, regsb[i])==0)
      return reg_b(i);
  }
  if (strcmp(tokens[p].str, "eip")==0)
    return cpu.eip;
  printf("%s",tokens[p].str);
  panic("no matching regsiter in regi_char2value()");
  return 0;
}
int deref_regi(int p){//'*': read the memory of address in register
  int addr = regi_char2value(p);
  return vaddr_read(addr, 1);
}
bool check_parentheses(int p, int q){
  if(tokens[p].type!='(' || tokens[q].type!=')'){
    return false;
  }
  int flag = 0;
  for (int i=p+1;i<q;i++){
    if (tokens[i].type == '('){
      flag ++;
    }
    if (tokens[i].type == ')'){
      if(flag <= 0)
        return false;
      else
        flag --;
    }
  }
  return true;
}
int dominant_operator(int p, int q){
  int op = -2;
  int level = -2;//!-:-1  */:0  +-:1  &&:2  ||:3  DEFER:4  ==!=:5
  for (int i=p;i<=q;i++){
    if(tokens[i].type=='('){
      while(tokens[i].type!=')'){
        i++;
      }
      i++;
    }
    if(tokens[i].type==TK_DEREF){
      if(level ==-2){
        op = i;
      }
      continue;
    }
    if(tokens[i].type==TK_MINUS || tokens[i].type==TK_NOT){//it's special that should choose the most left one(not right)
      if(level==-2){
        op = i;
        level = -1;
      }
      continue;
    }
    if(tokens[i].type=='*' || tokens[i].type=='/'){
      if(level <= 0){
        level = 0;
        op = i;
      }
      continue;
    }
    if(tokens[i].type=='+' || tokens[i].type=='-'){
      if(level <= 1){
        level = 1;
        op = i;
      }
      continue;
    }
    if(tokens[i].type==TK_EQ || tokens[i].type==TK_NEQ){
      if(level <=3){
        level = 3;
        op = i;
      }
      continue;
    }
    if(tokens[i].type==TK_OR){
      if(level <=4){
        level = 4;
        op = i;
      }
      continue;
    }
    if(tokens[i].type==TK_AND){
      if(level <= 5){
        level = 5;
        op = i;
      }
      continue;
    }
  }
  return op;
}
int eval(int p,int q){
  if (p > q){
    // printf("p:%d  q:%d\n",p,q);
    panic("error: p > q in eval()");
  }

  else if (p == q){// return the value of number/register
    if (tokens[p].type == TK_NUMBER)
      return atoi(tokens[p].str);
    if (tokens[p].type == TK_HEXNUMBER)
      return strtol(tokens[p].str,NULL,16);//hex number
    if (tokens[p].type == TK_REGISTER)
      return regi_char2value(p);//get the value of register
    // printf("%d",tokens[p].type);
    panic("wrong number in eval() when p==q");
  }

  else if(p==q-1&&(tokens[p].type == TK_DEREF || tokens[p].type == TK_MINUS)){//distinguish '*' '-'
    if (tokens[p].type == TK_DEREF){//*$eax
      return deref_regi(q);
    }
    if (tokens[p].type == TK_MINUS){//minus number
      if (tokens[q].type == TK_NUMBER)
        return -atoi(tokens[q].str);
      if (tokens[q].type == TK_HEXNUMBER)
        return -strtol(tokens[q].str,NULL,16);
    }
    panic("error in eval when p==q-1");
  }

  else if (check_parentheses(p,q) == true){
    return eval(p+1, q-1);
  }

  else{
    int op = dominant_operator(p,q);
    int op_type = tokens[op].type;
    if(op == p){//case: -(1+2)  !(1+2)
      int val = eval(p+1,q);
      switch (op_type){
        case TK_MINUS: return -val;
        case TK_NOT: return !val;
        case TK_DEREF: return vaddr_read(val, 1);
        default: panic("error in finding dominant operator when op==0");
      }
    }
    int val1 = eval(p,op-1);
    int val2 = eval(op+1,q);
    switch (op_type){
      case '+': return val1+val2;
      case '-': return val1-val2;
      case '*': return val1*val2;
      case '/': return val1/val2;
      case TK_EQ: return val1==val2;
      case TK_NEQ: return val1!=val2;
      case TK_AND: return val1&&val2;
      case TK_OR: return val1||val2;
      case TK_NOT: return !val2;
      default: panic("error in finding dominant operator");
    }
  }
}

uint32_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */
  // TODO();
  
  // distinguish different token type
  for (int i=0;i<nr_token;i++){
    //implement '-' minus
    if (tokens[i].type=='-' && (i==0 || (tokens[i-1].type != TK_NUMBER && tokens[i-1].type != TK_HEXNUMBER))){
      tokens[i].type = TK_MINUS;
    }
    //implement '*' dereference:
    if (tokens[i].type=='*' && (i==0 || (tokens[i-1].type != TK_NUMBER && tokens[i-1].type != TK_HEXNUMBER))){
      tokens[i].type = TK_DEREF;
    }
  }


  *success = true;
  return eval(0, nr_token-1);
}
