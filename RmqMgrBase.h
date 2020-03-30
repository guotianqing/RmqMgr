#ifndef TG_MGR_BASE_H_
#define TG_MGR_BASE_H_

#include <string>
#include <mutex>
#include <condition_variable>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/thread.hpp>


#include "amqpcpp.h"

using std::string;

typedef struct _mqinfo
{
	string ip;
	unsigned short port;
	string loginName;
	string loginPwd;
	string exchangeName;
	string exchangeType;
	string queueName;
	string routingKey; // ���ڷ��Ͷ˷�����Ϣ
	string bindingKey; // ���ն˰�queueʱʹ��
} MqInfo;

class MyConnectionHandler;
class TcpMgr;

class RmqMgrBase
{
public:
	RmqMgrBase();
	virtual ~RmqMgrBase();

	// ��ʼ��ʱ���ã�����RabbitMq������Ϣ
	bool MqInfoInit(const MqInfo &mqInfo, string &err);
	// ��ʼ��ʱ���ã��������ӡ�����ͨ���������������������
	bool StartMqInstance(string &err);
	// ������Ϣʱ���á��ر�ע�⣺�ú������̰߳�ȫ����Ҫȷ���ڵ�һ�߳��е���
	bool PublishMsg(const string &msg, string &err);
	// �ͷ�ʵ��
	bool ReleaseMqInstance(string &err, const bool isAutoResume = false);
	// ���ش�����Ϣ�����Ӻ�����¼��־������
	virtual void OnRtnErrMsg(string &err) = 0;

protected:
	// ���Ӻ���ʵ��
	// ���յ����ݣ�������к�ʱ����������������Ϣ�ѻ�������ͻ��˸��ɹ��أ�����ʹ�ø��ؾ���ģʽ
	virtual void OnRecvedData(const char *data, const uint64_t len) = 0;
	// ����״̬
	virtual void OnStatusChange(const bool isOk) = 0;

private:
	MqInfo m_mqInfo;
	boost::shared_ptr<AMQP::Connection > m_connection;
	boost::shared_ptr<AMQP::Channel> m_channel;
	boost::shared_ptr<AMQP::Channel> m_channelPub;
	bool m_isAutoResume;
	boost::shared_ptr<TcpMgr> m_pTcpMgr;

	void GetMqConnection();
	bool CloseMqConnection(string &err);
	bool CreateMqChannel(string &err);
	void CreateMqPubChannel();
	bool CloseMqChannel(string &err);
	bool CreateMqExchange(const string &exchangeName, const string &exchangeType, string &err);
	bool CreateMqQueue(const string &queueName, string &err);
	bool BindQueue(const string &queueName, const string &exchangeName, const string &routingKey, string &err);
	bool SetQosValue(const uint16_t val, string &err);
	bool StartConsumeMsg(const string &queueName, string &err);

	// ע��ص�����
	void ChannelOkCb();
	void ChannelErrCb(const char *msg);
	void ChannelCloseErrCb(const char *msg);
	void CreatMqExchangeErrCb(const char *msg);
	void CreatMqQueueErrCb(const char *msg);
	void BindQueueErrCb(const char *msg);
	void SetQosValueErrCb(const char *msg);
	void ConsumeRecvedCb(const AMQP::Message &message, uint64_t deliveryTag, bool redelivered);
	void ConsumeErrorCb(const char *msg);
};

// ����TCP���ӵ���
class MyConnectionHandler : public AMQP::ConnectionHandler
{
private:
	boost::shared_ptr<TcpMgr> m_pTcpMgr;

public:
	MyConnectionHandler(boost::shared_ptr<TcpMgr> pTcpMgr);

	// ���ݴ����ͳ�ȥ
	virtual void onData(AMQP::Connection *connection, const char *data, size_t size);
	// Rmq��¼�ɹ�
	virtual void onReady(AMQP::Connection *connection);
	// ��������һ�㷢���˴�������Ӳ��ٿ���
	virtual void onError(AMQP::Connection *connection, const char *message);
	// �Զ˹ر�����
	virtual void onClosed(AMQP::Connection *connection);
};

class TcpMgr : public boost::enable_shared_from_this<TcpMgr>, boost::noncopyable
{
public:
	static boost::shared_ptr<TcpMgr> Init(const MqInfo& mqInfo, RmqMgrBase *pRmqMgrBase);
	boost::shared_ptr<AMQP::Connection> GetConnection();
	bool SendData(const std::string &msg, std::string errmsg);
	bool WaitForReady();
	void SetLoginReady();
	void Finit();
	void OnErrMsg(std::string& msg);

private:
	using error_code = boost::system::error_code;
	MqInfo m_mqInfo;
	boost::asio::io_service m_ios;
	boost::asio::io_service::work m_work;
	boost::asio::io_service::strand m_strand;
	boost::asio::ip::tcp::socket m_sock;
	static const int kNumOfWorkThreads = 1; // ����tcp���߳�����������Ƕ��̣߳���Ҫ����ͬ������
	boost::thread_group m_threads;
	bool m_socketStarted;
	std::vector<char> m_recv_buffer;
	std::vector<char> m_parseBuf; // ���ݽ���������
	std::list<int32_t> m_pending_recvs;
	static const int kReceiveBufferSize = 4096; // Ĭ��ÿ�ν���������
	std::list<boost::shared_ptr<std::string>> m_pending_sends;
	RmqMgrBase *m_pRmqMgrBase;
	MyConnectionHandler* m_pHandler;
	boost::shared_ptr<AMQP::Connection> m_pConnect;
	static const int kTcpRetryInterval = 1000; //ms
	static const int kLoginRmqTimeOut = 10; //s
	std::condition_variable m_cv_login_succ;
	bool m_is_ready;
	std::mutex m_mtx_login;

	TcpMgr();
	void Start(const MqInfo& mqInfo, RmqMgrBase *pRmqMgrBase);
	void OnConnect(const error_code &err);
	void HandleWrite(const error_code &err);
	void ReConnectServer();
	void RecvData(int32_t total_bytes = 0);
	void DispatchRecv(int32_t total_bytes);
	void StartRecv(int32_t total_bytes);
	void HandleRecv(const error_code &err, size_t bytes);
	void SendMsg(boost::shared_ptr<string> msg);
	void StartSend();
	void Run();
	void CloseSocket();
	void StartCloseSocket();
	void ParseAmqpData();
};

#endif
