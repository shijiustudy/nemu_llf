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
#include "isa.h"
#include "sdb.h"

#define NR_WP 32


typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  /* TODO: Add more members if necessary */
    bool flag; // use / unuse
    char expr[100];
    int new_value;
    int old_value;

} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

WP* new_wp(){
    for(WP* p = free_ ; p -> next != NULL ; p = p -> next){
        if( p -> flag == false){
            p -> flag = true;
            if(head == NULL){
                head = p;
            }
            return p;
        }
    }
    printf("No unuse point.\n");
    assert(0);
    return NULL;
}

void free_wp(WP *wp) {
    if (head == NULL || wp == NULL) return;

    // --- 第一步：从 head 正在使用的链表中摘除 ---
    if (head == wp) {
        // 如果要删的是第一个节点，直接让 head 指向老大的小弟
        head = head->next;
    } else {
        // 如果要删的不是头节点，得找到它的“前任”
        WP *p = head;
        while (p != NULL && p->next != wp) {
            p = p->next;
        }
        if (p == NULL) {
            printf("找不到该监视点！\n");
            return;
        }
        // 把“前任”和“继任”连起来，跳过中间要删的 wp
        p->next = wp->next;
    }

    // --- 第二步：清理数据并回收进 free_ 链表 ---
    wp->flag = false;           // 标记为未使用
    memset(wp->expr, 0, sizeof(wp->expr)); // 清空表达式字符串（可选，但更安全）
    
    // 把这个节点插到 free_ 链表的头部
    wp->next = free_;
    free_ = wp;

    printf("Watchpoint %d 释放成功。\n", wp->NO);
}

void sdb_watchpoint_display(){
    bool flag = true;
    for(int i = 0 ; i < NR_WP ; i ++){
        if(wp_pool[i].flag){
            printf("Watchpoint.No: %d, expr = \"%s\", value = %d\n",
                    wp_pool[i].NO, wp_pool[i].expr,wp_pool[i].old_value);
                flag = false;
        }
    }
    if(flag) printf("No watchpoint now.\n");
}
void delete_watchpoint(int no){
    for(int i = 0 ; i < NR_WP ; i ++)
        if(wp_pool[i].NO == no){
            free_wp(&wp_pool[i]);
            return ;
        }
}

void create_watchpoint(char* args){
    WP* p =  new_wp();
    strcpy(p -> expr, args);
    bool success = false;
    int tmp = expr(p -> expr,&success);
   if(success) p -> old_value = tmp;
   else printf("创建watchpoint的时候expr求值出现问题\n");
    printf("Create watchpoint No.%d success.\n", p -> NO);
}

// 在 watchpoint.c 末尾添加
bool check_watchpoints() {
  bool changed = false;
  for (int i = 0; i < NR_WP; i++) {
    if (wp_pool[i].flag) {
      bool success = false;
      // 重新计算当前表达式的值
      int val = expr(wp_pool[i].expr, &success);
      if (!success) {
        printf("Error: Expr evaluation failed for Watchpoint No.%d\n", wp_pool[i].NO);
        continue;
      }

      // 检查值是否改变
      if (val != wp_pool[i].old_value) {
        printf("Watchpoint %d triggered: %s\n", wp_pool[i].NO, wp_pool[i].expr);
        printf("Old value = %d\nNew value = %d\n", wp_pool[i].old_value, val);
        printf("Old value = 0x%08x\nNew value = 0x%08x\n", wp_pool[i].old_value, val);

        wp_pool[i].old_value = val; // 更新旧值，以便下次比较
        changed = true;
      }
    }
  }
  return changed;
}
