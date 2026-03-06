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
#include "local-include/reg.h"

const char *regs[] = {
  "$0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
  "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
  "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
  "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

void isa_reg_display() {
  printf("--- CPU Registers ---\n");
  for (int i = 0; i < 32; i++) {
    // regs_name 是框架自带的一个数组，存着 "s0", "a0" 这种名字
    printf("%-5s : 0x%08x\n", regs[i], gpr(i));
    if ((i + 1) % 4 == 0) printf("\n");
  }
  printf("%-5s : 0x%08x\n", "pc", cpu.pc);
}

word_t isa_reg_str2val(const char *s, bool *success) {
  // 1. 先检查是否是 PC
  if (strcmp(s, "pc") == 0) {
    *success = true;
    return cpu.pc;
  }

  // 2. 遍历通用寄存器 gpr (32个)
  for (int i = 0; i < 32; i++) {
    if (strcmp(s, reg_name(i)) == 0) { // reg_name 获取第 i 个寄存器的名字
      *success = true;
      return cpu.gpr[i];
    }
  }

  // 3. 如果都没找到，报错
  *success = false;
  return 0;
}
