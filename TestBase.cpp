#include "TestBase.h"
#include <iostream>

using namespace std;

bool TestBase::Init(const MqInfo &mqinfo)
{
	string err;
	if (!MqInfoInit(mqinfo, err))
	{
		cerr << "初始化MqInfo失败: " << err << endl;
		return false;
	}
	if (!StartMqInstance(err))
	{
		cerr << "StartMqInstance失败: " << err << endl;
		return false;
	}

	return true;
}

void TestBase::Finit()
{
	string err;
	ReleaseMqInstance(err);
}

void TestBase::OnRtnErrMsg(string &err)
{
	cerr << "mq基类返回错误信息: " << err << endl;
}

void TestBase::OnStatusChange(const bool isOk)
{
	if (isOk)
	{
		m_mqConnState = true;
	}
	else
	{
		m_mqConnState = false;
	}
}

void TestBase::OnRecvedData(const char *data, const uint64_t len)
{
	cout << data << endl;
}
