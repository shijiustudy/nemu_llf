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
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"
#include "memory/paddr.h"
#include "memory/vaddr.h"

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
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
  nemu_state.state = NEMU_QUIT;
  return -1;
}
//如果返回 0：表示命令执行成功，继续等待下一个命令 ; 如果返回 非零（通常是 -1）：表示用户想要终止程序/退出模拟器。

static int cmd_help(char *args);

static int cmd_info(char *args);

static int cmd_x(char *args);

static int cmd_d(char *args){
    if(args == NULL)
        printf("No args.\n");
    else{
        delete_watchpoint(atoi(args));
    }
    return 0;
}

static int cmd_w(char* args){
    create_watchpoint(args);
    return 0;
}



static int cmd_si(char *args){
  /* 1. 尝试切分出第一个参数（即步数 N） */
  char *arg = strtok(NULL, " ");
  int step = 0;

  if (arg == NULL) {
    /* 如果用户只输入了 si，没给数字，默认走 1 步 */
    step = 1;
  } else {
    /* 如果给了数字，把字符串转成整数 */
    if (sscanf(arg, "%d", &step) != 1) {
      printf("错误：请输入正确的数字（例如 si 10）\n");
      return 0;
    }
  }

  /* 2. 核心：调用执行函数。你给它几，它就跑几步 */
  cpu_exec(step);

  return 0;
}

static int cmd_p(char* args){

    if(args == NULL){
        printf("No args\n");
        return 0;
    }
   // printf("args = %s\n", args);
    bool flag = false;
    expr(args, &flag);
    return 0;
}


static int cmd_test(char *args) {
    int right_ans = 0;
    FILE *input_file = fopen("/home/llf/ysyx-workbench/nemu/tools/gen-expr/input", "r");
    if (input_file == NULL) {
        perror("Error opening input file");
        return 0; // NEMU 命令函数通常返回 0
    }

    char record[4096]; // 表达式可能非常长，建议开大一点
    uint32_t real_val;

    for (int i = 0; i < 100; i++) {
        if (fgets(record, sizeof(record), input_file) == NULL) break;

        // 1. 去掉末尾的换行符
        record[strcspn(record, "\n")] = '\0';

        // 2. 分离标准答案和表达式字符串
        char *val_str = strtok(record, " "); // 取得第一个空格前的数字部分
        if (val_str == NULL) continue;

        real_val = (uint32_t)strtoul(val_str, NULL, 10); // 使用无符号转换
        char *expr_str = record + strlen(val_str) + 1; // 跳过数字和空格，剩下全是表达式

        // 3. 执行计算
        bool success = false;
        uint32_t res = expr(expr_str, &success);

        // 4. 比较与输出
        if (success && res == real_val) {
            right_ans++;
        } else {
            // 打印失败的用例，方便你针对性调试
            printf("[FAILED] No.%d | Real: %u, Mine: %u, Expr: %s\n", i + 1, real_val, res, expr_str);
        }
    }

    printf("\nTest Finished! Accuracy: %d/100\n", right_ans);
    fclose(input_file);
    return 0;
}


static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si","Let the program execute N instructions and then suspend the excution,while N is not given,the default value is 1",cmd_si},
  { "info", "Display register status (r) or watchpoint status (w)", cmd_info },
  { "x", "Scan memory starting from address EXPR for N words", cmd_x },
  { "p","Calculate the result of expression",cmd_p },
  { "d","Delete the watchpoint",cmd_d},
  { "w","Set the watchpoint",cmd_w},
  { "test","Test the accurancy of expression acculate",cmd_test }
};

#define NR_CMD ARRLEN(cmd_table)

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

static int cmd_info(char *args) {
  // 1. 看看后面跟着什么参数
  char *arg = strtok(NULL, " ");

  if (arg == NULL) {
    printf("告诉我要 info 什么（比如 info r 或 info w）\n");
  } 
  else if (strcmp(arg, "r") == 0) {
    /* 2. 如果参数是 'r'，就去调用那个专门打印寄存器的 API */
    isa_reg_display();
  } 
  else if (strcmp(arg, "w") == 0) {
    /* 这是以后要实现的监视点，现在可以先留个位置 */
    sdb_watchpoint_display();
  } 
  else {
    printf("未知的 info 参数: %s\n", arg);
  }
  return 0;
}

static int cmd_x(char *args){

    char *arg = strtok(NULL, " ");
    char *arg_addr = strtok(NULL, " ");

    if((arg == NULL)||(arg_addr == NULL)){
	    printf("用法不对哦,试着输入x N EXPR(表达式) \n");
	    return 0;
    }


   int n;
   uint64_t addr;
   sscanf(arg, "%d", &n);      // 读入要看的个数
   sscanf(arg_addr, "%lx", &addr);  // 读入起始地址

   printf("Memory Scan at 0x%08lx:\n", addr);
   for (int i = 0; i < n; i++) {
    
	   uint32_t val = vaddr_read(addr, 4);
	   printf("0x%08lx:  0x%08x\n", addr, val);
	   addr += 4;
  }

  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
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

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { 
          return; 
        }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
