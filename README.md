![Logo](./MWOS.png)
# MWOS
### 全自研的UEFI操作系统

## 编译
### 你需要准备以下工具：
`Clang`

`NASM`

`MTools`

`Linker`

`Objcopy`

### 在项目**根目录**下运行：
```bash
make clean
make
```

## 待办事项
| 功能 | 状态 | 备注 |
|------|------|------|
| UEFI启动 | 已完成 | 无 |
| 进入内核 | 已完成 | 无 |
| 图形显示 | 编写中 | 目前图形库不是特别完善，需要完善一下 |
| 图形界面 | 待编写 | 需要一个WindowManager |
| 内存管理 | 编写中 | 无 |
| 磁盘操作 | 已完成 | 速度很慢，尤其是计算Free Space。只支持IDE控制器 |
| 文件系统 | 已完成 | 只支持F16 |
| 键鼠驱动 | 已完成 | 只支持PS/2 |



## 联系方式
### undefined404offical
邮箱：w.sc.2022@outlook.com  
QQ: 2480340196  
B站：UID:3546599963756811

### McLDY（突发恶疾的李大爷）
邮箱：mcldy00@outlook.com  
QQ：3777426705  
B站：UID:3546599963756811

### fyqcell（不爱写内核的哥们）
邮箱：  
QQ：  
