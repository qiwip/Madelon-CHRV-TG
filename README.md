# 迈迪龙新风接入天猫精灵

本方案使用安信可tg-12f（[文档](https://docs.ai-thinker.com/tg)）进行开发，基于安信可提供的SDK([仓库地址](https://github.com/Ai-Thinker-Open/Ai-Thinker-Open-TG7100C_SDK))进行了改造。主要基于example里的smart_outlet改造而来，例程中自带了蓝牙辅助配网、ota、消息处理等模块，基本不需要改动。

因为要和485模块进行通信，必然要使用开发板的uart设备，但是原SDK
安信可的教程中有个致命错误：
tg-12f-kit，把uart0接到了micro-usb，波特率默认为115200，日志也从这个串口输出。而官方文档里说的日志串口uart1，波特率921600，并没有使用。所以本项目可以直接使用uart1，在aos.main.c中稍作修改，初始化为你需要的波特率，就能用于和485通信等其他功能。
然后即可调用SDK里的串口收发函数
```
int32_t hal_uart_recv_II(uart_dev_t *uart, void *data, uint32_t expect_size, uint32_t *recv_size, uint32_t timeout);
int32_t hal_uart_send(uart_dev_t *uart, const void *data, uint32_t size, uint32_t timeout);
```

本方案中modbus通信部分使用了开源的实现[nanoModbus](https://github.com/debevv/nanoMODBUS)。

## 编译
参考[安信可教程](https://aithinker.blog.csdn.net/article/details/110559410)。
### 1.环境配置
* Ubuntu系统

```bash
sudo apt-get update
sudo apt-get -y install libssl-dev:i386
sudo apt-get -y install libncurses-dev:i386
sudo apt-get -y install libreadline-dev:i386

sudo apt-get update
sudo apt-get -y install git wget make flex bison gperf unzip
sudo apt-get -y install gcc-multilib
sudo apt-get -y install libssl-dev
sudo apt-get -y install libncurses-dev
sudo apt-get -y install libreadline-dev
sudo apt-get -y install python3 python3-pip

python -m pip install --trusted-host=mirrors.aliyun.com -i https://mirrors.aliyun.com/pypi/simple/ --upgrade pip
pip install --trusted-host=mirrors.aliyun.com -i https://mirrors.aliyun.com/pypi/simple/ setuptools
pip install --trusted-host=mirrors.aliyun.com -i https://mirrors.aliyun.com/pypi/simple/ wheel
pip install --trusted-host=mirrors.aliyun.com -i https://mirrors.aliyun.com/pypi/simple/ aos-cube

# 克隆仓库
git clone git@github.com:qiwip/Madelon-CHRV-TG.git
```
### 2.编译固件

在本仓库下，直接运行命令
```bash
cd Madelon-CHRV-TG
./build.sh
# build之后，生成的固件在./out/air_fresh@tg7100cevb/air_fresh@tg7100cevb.bin，自行复制出来
```
## 烧录
### 1.烧录固件
下载[TG7100C_FlashEnv烧录调试工具](https://occ-oss-prod.oss-cn-hangzhou.aliyuncs.com/userFiles/3706713731985244160/resource/3706713731985244160smJBaJkFPK.zip)
* 在下载的文件包里面打开文件夹docs下的TG Flash Environment用户手册
打开 TGFlashEnv.exe
* 按照TG Flash Environment用户手册步骤配置好。
* 开发板通过数据线连接电脑。
* 在烧录工具中选择开发板的设备号，点击下载。

### 2.写入激活码
烧录后，重启设备，用调试助手(推荐使用sscom)通过AT命令进行激活码五元组烧录。命令发送时要加回车换行！！！

> 安信可文档的文档中，模组支持很多AT命令，实测下来，安信可文档里列的AT命令应该是安信可的原始AT固件，烧录天猫精灵固件后，AT命令由天猫精灵SDK内部实现，路径Living_SDK/platform/mcu/tg7100c/aos/aos_main.c，仅支持代码中列出的命令。

> 如果直接重启，会进等待烧录模式，也支持AT命令回显，但是无法写入激活码，因此需要把IO8拉低或者串口开启DTR再重启，进入天猫精灵固件。

启动后，写入五元组操作如下：

```
AT+LINKKEYCONFIG="ProductKey","DeviceName","DeviceSecret","ProduceSecret","ProductID"
```
例如: 
```
AT+LINKKEYCONFIG="a1R9jEoNWlN","7cb94c47e86f","cc7508d60d4f3abcec8369cdca420aa5","Z630ZNZAho4yEGk1","21937002"
```
查询五元组命令
```
AT+LINKKEYCONFIG?
```
## 硬件连接
将tg-12f开发板的IO21(tx)、IO12(rx)连接串口转485模块，再连接新风的485的AB接口即可。