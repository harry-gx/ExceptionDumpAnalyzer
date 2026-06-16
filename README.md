# ExceptionDumpAnalyzer

PC 端异常现场解析与回溯工具，使用 Qt 5 Widgets 实现。

## 输入文件

- `test.bin`：设备端导出的异常记录二进制文件。
- `*.elf`：对应固件 ELF，用于 `arm-none-eabi-addr2line` 精确定位函数和源码行。
- `*.map`：对应固件 MAP，用于符号范围兜底和栈快照返回地址过滤。

## 工程结构

本工程按 `UsbCanTools-qt/UsbCanTools` 的方式整理：工程文件放在顶层目录，只使用 `ExceptionDumpAnalyzer.pro` 管理 Qt 工程，不维护源码目录内的 Makefile。

```text
ExceptionDumpAnalyzer
├─ ExceptionDumpAnalyzer.pro
├─ README.md
├─ include
│  ├─ can
│  │  ├─ can_driver.h
│  │  ├─ can_transfer_widget.h
│  │  ├─ can_worker.h
│  │  ├─ ecan_vci_driver.h
│  │  └─ zlg_can_driver.h
│  ├─ core
│  │  ├─ exception_record.h
│  │  └─ symbol_resolver.h
│  └─ ui
│     └─ main_window.h
├─ src
│  ├─ main.cpp
│  ├─ can
│  │  ├─ can_driver.cpp
│  │  ├─ can_transfer_widget.cpp
│  │  ├─ can_worker.cpp
│  │  ├─ ecan_vci_driver.cpp
│  │  └─ zlg_can_driver.cpp
│  ├─ core
│  │  ├─ exception_record.cpp
│  │  └─ symbol_resolver.cpp
│  └─ ui
│     └─ main_window.cpp
├─ lib
│  ├─ ECanVci                       # 广成科技 USB CAN 运行库和开发头文件
│  └─ zlg                           # 周立功 zlgcan 运行库、头文件和 kerneldlls
├─ resources
│  ├─ resources.qrc
│  └─ icons
├─ scripts
│  └─ deploy_mingw.bat
└─ packages
   ├─ build-ExceptionDumpAnalyzer-*      # Qt Creator/脚本构建过程目录，打包成功后会删除
   └─ ExceptionDumpAnalyzer_yyyyMMdd_HHmmss
```

## 构建

推荐用 Qt Creator 直接打开顶层 `ExceptionDumpAnalyzer.pro`，并使用 shadow build，避免在源码目录生成 Makefile 和中间文件。`.pro` 已固定实际构建产物输出目录：Release 输出到 `packages/build-ExceptionDumpAnalyzer-qmake-Release`，Debug 输出到 `packages/build-ExceptionDumpAnalyzer-qmake-Debug`。

命令行构建示例：

```powershell
cd D:\E-develop\dev_qt\exception_data_analysis
New-Item -ItemType Directory -Force build-ExceptionDumpAnalyzer | Out-Null
cd build-ExceptionDumpAnalyzer
& 'C:\Qt\Qt5.9.0\5.9\mingw53_32\bin\qmake.exe' ..\ExceptionDumpAnalyzer\ExceptionDumpAnalyzer.pro
& 'C:\Qt\Qt5.9.0\Tools\mingw530_32\bin\mingw32-make.exe'
& 'C:\Qt\Qt5.9.0\5.9\mingw53_32\bin\windeployqt.exe' ..\ExceptionDumpAnalyzer\packages\build-ExceptionDumpAnalyzer-qmake-Release\release\ExceptionDumpAnalyzer.exe
```

## 打包

双击 `scripts/deploy_mingw.bat` 即可生成可拷贝运行的发布目录：

```text
packages\ExceptionDumpAnalyzer_yyyyMMdd_HHmmss
```

脚本会优先复用 `packages/build-ExceptionDumpAnalyzer-*-Release\release\ExceptionDumpAnalyzer.exe` 中由 Qt Creator 编译出的 Release 可执行文件；未找到时才自动编译。打包完成后会删除 `packages` 下的 `build-ExceptionDumpAnalyzer-*` 构建过程目录，并清理发布目录中的 `.o`、`.c`、`.cpp`、`.h` 等编译过程文件。

脚本会递归复制 `lib` 目录下的运行时 DLL 到发布目录。当前会带上广成科技 `ECanVci.dll`、`ECanVci64.dll`、`CHUSBDLL64.dll`，以及周立功 `zlgcan.dll` 和相关内核 DLL。

周立功 `zlgcan.dll` 还依赖 `kerneldlls` 下的 `dll_cfg.ini`、设备 XML 和内核库。打包脚本会把 `lib/zlg/kerneldlls` 原样复制到发布目录的 `kerneldlls`，发布时不要删除这个目录。

打包目录会包含 `tools/arm-none-eabi/bin/arm-none-eabi-addr2line.exe`。程序启动后会优先使用该内置工具解析 ELF 源码行，因此拷贝到未安装 ARM 工具链的 PC 上也能使用 ELF 精确回溯功能。

## CAN 收发

主界面顶部现在分为 `异常解析` 和 `CAN收发` 两个页签。`异常解析` 保留原来的 `test.bin + ELF + MAP` 解析流程；`CAN收发` 用于通过 USB CAN 接收设备端发回的异常现场数据。

当前驱动实现包括 `广成科技 ECanVci` 和 `周立功 USBCANFD`。UI 和收发线程只依赖 `ICanDriver` 接口；切换驱动后，设备类型下拉框会自动刷新。周立功当前只开放 `USBCANFD-200U` 和 `USBCANFD-MINI` 两种设备类型。

周立功 USBCANFD 设备按官方 demo 的方式初始化：先设置 CANFD 仲裁段波特率，再以 CANFD 设备通道启动；本工具当前仍只收发普通 CAN 2.0 数据帧。

CAN 页支持打开设备、发送请求帧、显示 TX/RX 日志、按接收顺序追加保存数据帧 payload。保存时会把已接收的数据按原始顺序写成二进制文件，默认路径为程序目录下的 `test.bin`；`保存并解析` 会自动回填解析页的二进制文件路径并执行回溯。

## 二进制帧格式

工具按 8KB 异常记录帧解析：

- `RecordHeader`：32 bytes，包含 `magic`、`version[4]`、`recordIndex`、`recordState`、`reserved`。
- `RecordHwStackFrame`：32 bytes，R0/R1/R2/R3/R12/LR/PC/xPSR。
- `RecordSwRegs`：32 bytes，R4~R11。
- `RecordSpecialRegs`：32 bytes，MSP/PSP/EXC_RETURN/CONTROL/PRIMASK/BASEPRI/FAULTMASK/faultType。
- `RecordFaultRegs`：32 bytes，CFSR/HFSR/DFSR/AFSR/MMFAR/BFAR/SHCSR/CCR。
- `RecordStackInfo`：32 bytes，stackStartAddr/stackSize/stackLimit/stackTop/reserved[4]。
- `stackData[4096]`。
- `reserved[3900]`。
- `crc32`：最后 4 bytes，校验前 8188 bytes。

## 应用图标

- `resources/icons/app_icon.ico`：通过 `RC_ICONS` 嵌入 Windows exe。
- `resources/icons/app_icon_256.png`：通过 Qt resource 设置运行时窗口、标题栏和任务栏图标。
- `resources/icons/app_icon_source.png`：用户提供的图标源图，后续调整图标时使用。

## 回溯说明

工具先定位异常 `PC`，再用 `LR` 找上一层调用者。

默认不勾选 `使用 Frame Pointer(R7) 回溯` 时，使用栈快照扫描策略：扫描栈快照中疑似 Thumb 返回地址的字值，并结合 MAP 的 `.text` 符号范围过滤。

勾选 `使用 Frame Pointer(R7) 回溯` 后，只使用 Frame Pointer 链策略。默认读取 `[FP]=prevFP, [FP+4]=LR`；如果该布局不成立，会在当前 `FP` 往高地址 256 bytes 内搜索 `prevFP + LR` 组合，以兼容不同编译器保存栈帧链时的偏移差异。

两种策略都会结合 ELF 的 `addr2line` 和 MAP 符号，尽可能向上回溯到 `main`。
