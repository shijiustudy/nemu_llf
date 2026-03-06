# NEMU

NEMU(NJU Emulator) is a simple but complete full-system emulator designed for teaching purpose.
Currently it supports x86, mips32, riscv32 and riscv64.
To build programs run above NEMU, refer to the [AM project](https://github.com/NJU-ProjectN/abstract-machine).

The main features of NEMU include
* a small monitor with a simple debugger
  * single step
  * register/memory examination
  * expression evaluation without the support of symbols
  * watch point
  * differential testing with reference design (e.g. QEMU)
  * snapshot
* CPU core with support of most common used instructions
  * x86
    * real mode is not supported
    * x87 floating point instructions are not supported
  * mips32
    * CP1 floating point instructions are not supported
  * riscv32
    * only RV32IM
  * riscv64
    * only RV64IM
* memory
* paging
  * TLB is optional (but necessary for mips32)
  * protection is not supported
* interrupt and exception
  * protection is not supported
* 5 devices
  * serial, timer, keyboard, VGA, audio
  * most of them are simplified and unprogrammable
* 2 types of I/O
  * port-mapped I/O and memory-mapped I/O



## 当前实现进度：简易调试器 (Monitor)

在基础架构之上，目前已实现了简易调试器（Monitor）的核心功能，包含表达式求值、内存扫描以及监视点等基础设施。

### 1. 调试器支持的命令列表

进入 `(nemu)` 提示符后，支持以下调试命令：

| 命令 | 格式 | 使用举例 | 说明 |
| :--- | :--- | :--- | :--- |
| **帮助** | `help` | `help` | 打印命令的帮助信息 |
| **继续运行** | `c` | `c` | 继续运行被暂停的程序 |
| **退出** | `q` | `q` | 退出 NEMU |
| **单步执行** | `si [N]` | `si 10` | 让程序单步执行 `N` 条指令后暂停执行，当 `N` 没有给出时，缺省为 `1` |
| **打印程序状态** | `info SUBCMD` | `info r`<br>`info w` | 打印寄存器状态<br>打印监视点信息 |
| **扫描内存** | `x N EXPR` | `x 10 $esp` | 求出表达式 `EXPR` 的值，将结果作为起始内存地址，以十六进制形式输出连续的 `N` 个 4 字节 |
| **表达式求值** | `p EXPR` | `p $eax + 1` | 求出表达式 `EXPR` 的值 |
| **设置监视点** | `w EXPR` | `w *0x2000` | 当表达式 `EXPR` 的值发生变化时，暂停程序执行 |
| **删除监视点** | `d N` | `d 2` | 删除序号为 `N` 的监视点 |

### 2. 自动化表达式测试功能

除了标准的调试命令，本项目还实现了一个用于验证表达式求值正确性的测试工具。

* **使用方法**：在启动 NEMU 并进入 `(nemu)` 提示符后，输入 `test` 命令。
* **功能说明**：系统将自动读取生成的 100 个合法的表达式，并在后台进行求值计算，最后输出求值的正确率测试结果。该功能用于确保底层表达式解析与计算逻辑的健壮性。

## 如何编译与运行

```bash
# 编译并启动 NEMU

make run

# 在 (nemu) 提示符下可直接输入上述命令进行调试或测试
