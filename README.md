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
`OVMF`

**可选**
`QEMU`
`Bochs`
`VMWare`

### 在项目**根目录**下运行：
```bash
make clean
make
```

### 不想自己编译/编译不了？
在Actions里面找最新的Artifacts，下载即可。

## 运行
### 使用QEMU运行
```bash
qemu-system-x86_64 -m 1024 -drive file=os-image.img,format=raw -bios OVMF.fd
```
替换`os-image`为你编译/下载的镜像
替换`OVMF.fd`为你的OVMF

### 使用Bochs运行
首先创建bochsrc.bxrc：
```bash
touch bochsrc.bxrc
```
然后向bochsrc.bxrc写入：
``` ini
megs: 1024

romimage: file=OVMF.fd, address=0xfff80000

ata0-master: type=disk, path="os-image.img", mode=flat, translation=auto

com1: enabled=1, mode=stdio

boot: disk
```
启动：
```bash
bochs -q -f bochsrc.bxrc
```

### OVMF下载
`https://github.com/clearlinux/common/raw/refs/heads/master/OVMF.fd`
或者在Actions里面找最新的Artifacts，下载即可


## 待办事项
| 功能 | 状态 | 备注 |
|------|------|------|
| UEFI启动 | 已完成 | 无 |
| 进入内核 | 已完成 | 无 |
| 图形显示 | 编写中 | 目前图形库不是特别完善，需要完善一下 |
| 图形界面 | 待编写 | 需要一个WindowManager |
| 内存管理 | 编写中 | 无 |
| 磁盘操作 | 已完成 | 速度很慢，尤其是计算Free Space。只支持IDE控制器 |
| 文件系统 | 已完成 | 只支持F32|
| 键鼠驱动 | 已完成 | 只支持PS/2 |

## 联系方式
### undefined404offical（被李大爷颗秒的UD404）
邮箱：w.sc.2022@outlook.com  
QQ: 2480340196  
B站：UID:3546599963756811

### McLDY（突发恶疾的李大爷）
邮箱：mcldy00@outlook.com  
QQ：3777426705  
B站：UID:3546599963756811
