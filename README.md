# RmqMgr
基于AMQP-CPP的跨平台RabbitMq通信类

### 简介
---
AMQP-CPP 的地址是：https://github.com/CopernicaMarketingSoftware/AMQP-CPP

AMQP-CPP 提供了仅支持Linux的 TCP模块，可以在Linux上直接使用。

但在Windows上需要自行实现网络层TCP通信及IO操作。

RmqMgr类使用Boost Asio库完成了AMQP-CPP 的网络层，实现了网络IO功能。

### 跨平台
---

底层使用了跨平台的Boost Asio库进行网络IO，所以可以跨平台移植。

### 存在的问题
---

该类是我前段时间写的，现在看来，虽然能够使用，但存在一些问题：

- 基类存在冗余代码
- 基类的设计未满足完全抽象的概念，更像是一个公共模块
- 对RabbitMq的重连操作还存在一些问题，如连接在第一次失败后，总是失败

### 最后
---

欢迎参考，并提出意见。
