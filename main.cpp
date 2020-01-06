#include "TestBase.h"
#include <iostream>

using namespace std;

int main()
{
	MqInfo mq;
	mq.ip = "127.0.0.1";
	mq.port = 5672;
	mq.loginName = "guest";
	mq.loginPwd = "guest";
	mq.exchangeName = "RmqMgrExchangeTest";
	mq.exchangeType = "fanout"; // 广播
	mq.queueName = ""; // queuename由Rmq服务器指定，为随机值
	mq.routingKey = ""; // 广播模式无需指定
	string peerQueueName = ""; // 一般无需该值
	mq.bindingKey = ""; // 接收端设置，广播模式无需该值

	TestBase rmq;
	rmq.Init(mq);
	string err;
	rmq.PublishMsg("I love you", err); // 会接收到自己发送的消息

	cin.get();
	return 0;
}