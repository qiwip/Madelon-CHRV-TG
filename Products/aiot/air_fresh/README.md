# 迈迪龙新风接入天猫精灵

本方案使用安信可tg-12f进行开发，基于安信可提供的SDK进行了改造。主要基于example的smart_outlet改造而来，例子中自带了蓝牙辅助配网、ota、消息处理等模块，基本不需要改动。
安信可的教程中的致命错误：
tg-12f-kit，只把uart0接到了micro-usb，波特率默认为115200，日志也从这个串口输出。而所谓的日志串口uart1，波特率921600并没有使用。而uart1可以在aos.main.c中稍作修改，初始化为你需要的波特率，用于和485通信等其他功能。
然后即可调用SDK里的串口收发函数
```
int32_t hal_uart_recv_II(uart_dev_t *uart, void *data, uint32_t expect_size, uint32_t *recv_size, uint32_t timeout);
int32_t hal_uart_send(uart_dev_t *uart, const void *data, uint32_t size, uint32_t timeout);
```

本方案中modbus部分使用了nanoModbus模块。

## 烧录命令
```
AT+LINKKEYCONFIG="ProductKey","DeviceName","DeviceSecret","ProduceSecret","ProductID"
```
例如: 
```
AT+LINKKEYCONFIG="a1R9jEoNWlN","7cb94c47e86f","cc7508d60d4f3abcec8369cdca420aa5","Z630ZNZAho4yEGk1","21937002"
```
查询三元组命令
```
AT+LINKKEYCONFIG?
```

IO21和IO12是uart1