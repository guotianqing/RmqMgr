#ifndef TEST_BASE_H_
#define TEST_BASE_H_

#include "RmqMgrBase.h"
#include <atomic>

class TestBase : public RmqMgrBase
{
private:
	std::atomic<bool> m_mqConnState;

	virtual void OnRtnErrMsg(string &err);
	virtual void OnStatusChange(const bool isOk);
	virtual void OnRecvedData(const char *data, const uint64_t len);

public:
	bool Init(const MqInfo &mqinfo);
	void Finit();
};

#endif