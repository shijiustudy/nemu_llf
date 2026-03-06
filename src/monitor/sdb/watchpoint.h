#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"
#include "sdb.h"
// 将结构体定义搬到这里，这样 sdb.c 和 watchpoint.c 都能看到它
typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  char expr[128];     // 存储表达式字符串
  uint32_t value;     // 存储表达式最近一次的值
  bool isused;        // 标记是否正在被使用
} WP;

// 声明函数，相当于给这些函数发“准考证”
void init_wp_pool();
WP* new_wp(char *str, uint32_t value);
void free_wp(int no);
void print_wp();
bool check_watchpoints(); // 用来在 cpu_exec 中检查值是否变化

#endif
