# README

> 引脚资源和代码————xjs的pid测试板
>
> 目前的sdk版本为：20.10.00.04。SYSCFG版本为：1.27.0

```txt
root
│                 │.clang-format 代码格式化设置 可以按照自己风格改
│                 │.gitignore    Git忽略文件配置
│                 │CMakeLists.txt 顶层CMake构建文件 我写的几乎不用动
│                 │mspm0g350x_base.cmake  MSPM0基础配置文件,在里面需要修改路径
│                 │README.md     本文件
│                 │run_syscfg_main.bat  运行syscfg脚本
│                 |---
├─.vscode         |VSCode配置目录，有调试设置，task设置
├─build           |构建输出目录 里面有`compile_commands.json`（clangd）
├─Flash           |烧录脚本及相关配置，需要阅读一下，现在写的一般不用改
├─FreeRTOS        |FreeRTOS实时操作系统源码
├─SysConfig       |TI系统配置文件，ti生成的文件保存到这里
└─User            |---
    ├─Bsp         |板载支持驱动
    ├─Device      |设备层
    ├─Module      |模块层
    ├─Algorithm   |算法层
    ├─Service     |服务层
    ├─App         |应用层 / c cpp混编接口层
    └─Protocol    |协议层
```

这个工程取自25年电赛国一的ti，里面存在freertos。我把它整理成了一种方便管理的文件样式，写了一个具体的操作指南

## 拿到工程要干嘛

在配置好cmake整个开发环境的基础上

打开 `mspm0g350x_base.cmake`

```bash
# 这一行要看你用SDK的安装路径去更改，改成你用的SDK的路径
set(MSPM0_SDK_PATH  D:\\Users\\admin\\Desktop\\work\\Toolchain\\ti\\mspm0_sdk_2_07_00_05)

set(MSPM0_SDK_PATH  /home/rh/ti/mspm0_sdk_2_10_00_04)
```

同时，如果要打开syscfg配置引脚，需要把那个文件拖拽到`TI syscfg`图标的上面打开

拿到一个新工程，或者转移了文件路径，要删掉vscode中原有的build目录，重新配置生成cmake，工具链选择arm_gcc，配置和编译应该都会成功

在user文件夹下，对应文件夹中的文件，会自动添加头文件和源文件，可以直接编译

已知bug：windows下需要改成使用ninja编译，默认不是ninja，需要稍微修改

## 编译烧录调试

编译：使用cmake，`F7`

烧录：使用JLink或者OpenOCD都写好了烧录脚本，几乎可以移植了直接用还不需要修改对应的文件名

说实话就得用JLink 我用OpenOCD会锁芯片（JLink，需要提前配置环境变量+安装JLink）

调试：写好了调试器模板OpenOCD和JLink的都有

烧录脚本规则。你生成的ELF文件要和根文件夹名字相同即可
