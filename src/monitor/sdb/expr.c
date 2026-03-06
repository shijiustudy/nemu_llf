/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
#include <memory/vaddr.h>


enum {
  TK_NOTYPE = 256,
  TK_NUM,       // 十进制
  TK_HEX_NUM,   // 十六进制
  TK_REG,       // 寄存器
  TK_EQ,        // ==
  TK_UEQ,       // !=
  TK_AND,       // &&
  TK_OR,        // ||
  TK_LEQ,       // <=
  TK_ZUO,       // (
  TK_YOU,       // ) 
  /* 下面这两个通常在后续处理中手动标记，但保留在 enum 中 */
  TK_MINUS,     // 负号
  TK_DEREF,     // 指针解引用
  TK_NEG	// 
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */
/* 1. 无效/前缀类型：最先过滤空格 */
  {" +", TK_NOTYPE},             // spaces

  /* 2. 比较与逻辑运算符：长字符必须放在前面，防止被拆分匹配 */
  {"==", TK_EQ},                 // equal
  {"!=", TK_UEQ},                // not equal
  {"&&", TK_AND},                // logical and
  {"\\|\\|", TK_OR},             // logical or (注意：| 在正则里是特殊字符，需双反斜杠转义)
  {"<=", TK_LEQ},                // less than or equal

  /* 3. 基础运算符与符号 */
  {"\\+", '+'},                  // plus 为什么两个
  {"-", '-'},                    // subtract
  {"\\*", '*'},                  // multiply
  {"/", '/'},                    // divide
  {"\\(", '('},                  // left parenthesis
  {"\\)", ')'},                  // right parenthesis
  {"!", '!'},                    // logical not

  /* 4. 操作数：十六进制、寄存器、十进制 */
  {"0[xX][0-9a-fA-F]+", TK_HEX_NUM}, // hex number: 支持 0x 和 0X
  {"\\$[a-zA-Z0-9]+", TK_REG},       // register: 修正了之前的 0-0 错误，匹配 $eax, $t0 等
  {"[0-9]+", TK_NUM},                // decimal number: 使用 + 确保至少匹配一位数字
				     

};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

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

static Token tokens[1024] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

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

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */
       switch (rules[i].token_type) {
          case TK_NOTYPE: 
            /* 空格：直接跳过，不计入 nr_token */
            break;

          case TK_NUM:
          case TK_HEX_NUM:
          case TK_REG:
            /* 这些类型需要保存字符串内容 */
            tokens[nr_token].type = rules[i].token_type;

            /* 这里的 32 是根据您定义的 str[32] 来的 */
            if (substr_len >= 32) {
                printf("Token too long at position %d: %.*s\n", position - substr_len, substr_len, substr_start);
                return false;
            }

            /* 拷贝子串：只拷贝匹配到的长度 */
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            /* 手动补上字符串结束符，防止打印时溢出 */
            tokens[nr_token].str[substr_len] = '\0';

            nr_token++;
            break;

          default:
            /* 其他如 '+', '-', '(', ')' 等运算符 */
            tokens[nr_token].type = rules[i].token_type;
            /* 运算符不需要 str 内容，清空一下比较保险 */
            tokens[nr_token].str[0] = '\0';
            
            nr_token++;
            break;
        }
       
        break; // 匹配到了就跳出 for 循环，去处理下一个位置
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}


static int check_parentheses(int p,int q){
  //return -1:stop calculate
  //return 0 :no need to remove parentheses
  //return 1 :need to remove parentheses
//part1 合法性检查
  int cnt = 0;
  for(int i = p; i <= q;i ++)
  {
    if(tokens[i].type == '(')cnt ++;
    else if(tokens[i].type == ')')cnt --;

    if(cnt < 0)return -1;
  }
  if(cnt != 0) return -1;
//part2 看外层括号
  if(tokens[p].type != '(' || tokens[q].type != ')')return 0;
//part3
  int ret = check_parentheses(p+1,q-1);
  if(ret == -1)return 0;
  else if(ret == 0||ret == 1)return 1;
  return 2;//algorithm_error，提高算法健壮性，防止上面考虑不全而出错
}


int find_main_op(int p, int q) {
  int num = 0;          // 括号计数器
  int main_op = -1;     // 记录主操作符的位置
  int min_precedence = 100; // 记录当前找到的最低优先级数值

  for (int i = p; i <= q; i++) {
    // 1. 跳过括号内的内容
    if (tokens[i].type == '(' || tokens[i].type == TK_ZUO) { 
      num++; 
      continue; 
    }
    if (tokens[i].type == ')' || tokens[i].type == TK_YOU) { 
      num--; 
      continue; 
    }

    // 只有当 num == 0 时，才说明当前操作符在括号外，有资格成为主操作符
    if (num != 0) continue;

    int op_type = tokens[i].type;
    int prec = -1;

    // 2. 定义优先级：数值越小，执行越晚（优先级越低），越适合当主操作符
    // 优先级 1: 逻辑运算符 (==, !=)
    if (op_type == TK_EQ || op_type == TK_UEQ) prec = 1;
    // 优先级 2: 加减
    else if (op_type == '+' || op_type == '-') prec = 2;
    // 优先级 3: 乘除
    else if (op_type == '*' || op_type == '/') prec = 3;

    // 3. 核心逻辑：更新主操作符
    if (prec != -1) {
      /* * 为什么用 <= ？
       * 对于同优先级的运算符（如 1 + 2 - 3），我们要找最后出现的那个（减号），
       * 这样才能保证左结合律：计算顺序为 (1 + 2) - 3。
       */
      if (prec <= min_precedence) {
        min_precedence = prec;
        main_op = i;
      }
    }
  }
  return main_op;
}


int eval(int p, int q, bool *success) {
  
  if (p > q) {
    /* 发生这种情况通常是表达式语法错误，例如 "()" */
    *success = false;
    return 0;
  }
  else if (p == q) {
    /* 单个 Token：解析数字或寄存器 */
    uint32_t val = 0;
    if (tokens[p].type == TK_NUM) {
      sscanf(tokens[p].str, "%u", &val);
    } 
    else if (tokens[p].type == TK_HEX_NUM) {
      sscanf(tokens[p].str, "%x", &val);
    } 
    else if (tokens[p].type == TK_REG) {
      // isa_reg_str2val 是 NEMU 提供的函数，用来把 "$eax" 转换成值
      // 记得跳过寄存器名前面的 '$'，所以用 tokens[p].str + 1
      bool reg_success;
      val = isa_reg_str2val(tokens[p].str + 1, &reg_success);
      if (!reg_success) { *success = false; return 0; }
    } 
    else {
      *success = false;
      return 0;
    }
    return val;
  }
  else if (check_parentheses(p, q) == true) {
    /* 整个表达式被一对匹配的括号包围，去掉括号递归 */
    return eval(p + 1, q - 1, success);
  }
  else {
    /* 核心：寻找主运算符 */
    int op = find_main_op(p, q);
    if (op < 0) {

        // 如果找不到双目运算符，但当前 token 是负号或解引用号
      if (tokens[p].type == TK_NEG) {
        uint32_t val = eval(p + 1, q, success);
        return -val;
      }
      if (tokens[p].type == TK_DEREF) {
        uint32_t addr = eval(p + 1, q, success);

        if (addr < 0x80000000 || addr > 0x87ffffff) {
              printf("Error: Address 0x%08x is out of bound.\n", addr);
              *success = false;
              return 0;
          }
        // 调用 NEMU 的内存读取函数，读取 4 字节
        return vaddr_read(addr, 4);
      }

      *success = false;
      return 0;
    }

    // 递归计算左右两部分
    int val1 = eval(p, op - 1, success);
    if (!(*success)) return 0;
    int val2 = eval(op + 1, q, success);
    if (!(*success)) return 0;

    // 根据主运算符类型进行计算
    switch (tokens[op].type) {
      case '+': return val1 + val2;
      case '-': return val1 - val2;
      case '*': return val1 * val2;
      case '/': 
        if (val2 == 0) {
          printf("Error: Division by zero at position %d\n", op);
          *success = false;
          return 0;
        }
        return val1 / val2;
      case TK_EQ:  return val1 == val2;
      case TK_UEQ: return val1 != val2;
      default: 
        printf("Error: Unknown operator type %d\n", tokens[op].type);
        assert(0);
    }
  }
}

int char2int(char s[]){
    int s_size = strlen(s);
    int res = 0 ;
    for(int i = 0 ; i < s_size ; i ++)
    {
	res += s[i] - '0';
	res *= 10;
    }
    res /= 10;
    return res;
}
void int2char(int x, char str[]){
    int len = strlen(str);
    memset(str, 0, len);
    int tmp_index = 0;
    int tmp_x = x;
    int x_size = 0, flag = 1;
    while(tmp_x){
	tmp_x /= 10;
	x_size ++;
	flag *= 10;
    }
    flag /= 10;
    while(x)
    {
	int a = x / flag; 
	x %= flag;
	flag /= 10;
	str[tmp_index ++] = a + '0';
    }
}


word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

   for (int i = 0; i < nr_token; i ++) {
     // 1. 处理寄存器：直接把寄存器的值转成数字字符串
     if (tokens[i].type == TK_REG) {
       bool flag = true;
       uint32_t val = isa_reg_str2val(tokens[i].str + 1, &flag); // +1 是为了跳过 '$'
       if (flag) {
         sprintf(tokens[i].str, "%u", val);
         tokens[i].type = TK_NUM; // 把它变成普通数字，eval 就好处理了
       } else {
         *success = false; return 0;
       }
     }
   
     // 2. 识别负号 (TK_NEG)
     if (tokens[i].type == '-' && (i == 0 || (tokens[i-1].type != TK_NUM && tokens[i-1].type != TK_HEX_NUM && tokens[i-1].type != TK_REG && tokens[i-1].type != ')'))) {
       tokens[i].type = TK_NEG;
     }
   
     // 3. 识别解引用 (TK_DEREF)
     if (tokens[i].type == '*' && (i == 0 || (tokens[i-1].type != TK_NUM && tokens[i-1].type != TK_HEX_NUM && tokens[i-1].type != TK_REG && tokens[i-1].type != ')'))) {
       tokens[i].type = TK_DEREF;
     }
   }
	     

  *success = true; // 先假设会成功
  uint32_t result = 0; 
  result = eval(0, nr_token - 1, success);
  printf("result is %d \n",result);

  // 如果 eval 内部发现错误（如除 0），success 会被置为 false
  if (*success) {
    return result;
  } else {  

  return 0;
}

}
