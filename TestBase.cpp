#include "TestBase.h"
#include <iostream>

using namespace std;

bool TestBase::Init(const MqInfo &mqinfo)
{
	string err;
	if (!MqInfoInit(mqinfo, err))
	{
		cerr << "��ʼ��MqInfoʧ��: " << err << endl;
		return false;
	}
	if (!StartMqInstance(err))
	{
		cerr << "StartMqInstanceʧ��: " << err << endl;
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
	cerr << "mq���෵�ش�����Ϣ: " << err << endl;
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
