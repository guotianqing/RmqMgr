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
	mq.exchangeType = "fanout"; // �㲥
	mq.queueName = ""; // queuename��Rmq������ָ����Ϊ���ֵ
	mq.routingKey = ""; // �㲥ģʽ����ָ��
	string peerQueueName = ""; // һ�������ֵ
	mq.bindingKey = ""; // ���ն����ã��㲥ģʽ�����ֵ

	TestBase rmq;
	rmq.Init(mq);
	string err;
	rmq.PublishMsg("I love you", err); // ����յ��Լ����͵���Ϣ

	cin.get();
	return 0;
}