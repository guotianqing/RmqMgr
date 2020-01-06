#ifndef TG_MGR_BASE_H_
#define TG_MGR_BASE_H_

#include <string>
#include <mutex>
#include <future>
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
class TcpClient;

class RmqMgrBase
{
private:
	MqInfo m_mqInfo;
	boost::shared_ptr<MyConnectionHandler> m_handler;
	boost::shared_ptr<AMQP::Connection > m_connection;
	boost::shared_ptr<AMQP::Channel> m_channel;
	bool m_quitFlag;
	bool m_isAutoResume;
	boost::shared_ptr<TcpClient> m_pTcpClient;

	bool CreateMqConnection(const string &ip, const unsigned short &port, const string &loginName, const string &loginPwd, string &err);
	bool CloseMqConnection(string &err);
	bool CreateMqChannel(string &err);
	bool CloseMqChannel(string &err);
	bool CreateMqExchange(const string &exchangeName, const string &exchangeType, string &err);
	bool CreateMqQueue(const string &queueName, string &err);
	bool BindQueue(const string &queueName, const string &exchangeName, const string &routingKey, string &err);
	bool SetQosValue(const uint16_t val, string &err);
	bool StartConsumeMsg(const string &queueName, string &err);
	// ����rabbitmq client���ڲ��������߳��첽����rabbitmqָ��
	void ReStartMqInstance(string &err);

	// ע��ص�����
	void ChannelOkCb();
	void ChannelErrCb(const char *msg);
	void ChannelCloseErrCb(const char *msg);
	void CreatMqExchangeErrCb(const char *msg);
	void CreatMqQueueErrCb(const char *msg);
	void BindQueueErrCb(const char *msg);
	void SetQosValueErrCb(const char *msg);
	void PublishMsgErrCb(const char *msg);
	void PublishMsgOkCb();
	void ConsumeRecvedCb(const AMQP::Message &message, uint64_t deliveryTag, bool redelivered);
	void ConsumeErrorCb(const char *msg);
	void ConsumeOkCb(const std::string &consumertag);

	std::mutex m_mtxPublishMsg;
public:
	RmqMgrBase();
	virtual ~RmqMgrBase();

	bool MqInfoInit(const MqInfo &mqInfo, string &err);
	bool StartMqInstance(string &err);
	bool PublishMsg(const string &msg, string &err);
	bool ReleaseMqInstance(string &err, const bool isAutoResume = false);

	// ���ش�����Ϣ�����Ӻ�����¼��־������
	virtual void OnRtnErrMsg(string &err) = 0;
protected:
	// ���Ӻ���ʵ��
	// ���յ����ݣ�������к�ʱ����������������Ϣ�ѻ�������ͻ��˸��ɹ��أ�����ʹ�ø��ؾ���ģʽ
	virtual void OnRecvedData(const char *data, const uint64_t len) = 0;
	// ����״̬
	virtual void OnStatusChange(const bool isOk) = 0;
};

// ����TCP���ӵ���
class MyConnectionHandler : public AMQP::ConnectionHandler
{
private:
	boost::shared_ptr<TcpClient> m_pTcpClient;

public:
	MyConnectionHandler(boost::shared_ptr<TcpClient> pTcpClient);
	~MyConnectionHandler();

	// ���ݴ����ͳ�ȥ
	virtual void onData(AMQP::Connection *connection, const char *data, size_t size);
	// Rmq��¼�ɹ�
	virtual void onReady(AMQP::Connection *connection);
	// ��������һ�㷢���˴�������Ӳ��ٿ���
	virtual void onError(AMQP::Connection *connection, const char *message);
	// �Զ˹ر�����
	virtual void onClosed(AMQP::Connection *connection);
};

#define MEM_FN(x) boost::bind(&self_type::x, shared_from_this())
#define MEM_FN1(x, y) boost::bind(&self_type::x, shared_from_this(), y)
#define MEM_FN2(x, y, z) boost::bind(&self_type::x, shared_from_this(), y, z)

class TcpClient : public boost::enable_shared_from_this<TcpClient>, boost::noncopyable
{
private:
	typedef boost::system::error_code error_code;
	typedef TcpClient self_type;
	bool IsSocketStarted();
	void OnConnect(const error_code &err);
	void OnReadData(const error_code &err, size_t bytes);
	void OnWrite(const error_code &err, size_t bytes);
	void ReConnectServer();
	void Run();
	void RecvData();
	void parse();

public:
	TcpClient();
	~TcpClient();
	void Start(const MqInfo& mqInfo, boost::shared_ptr<RmqMgrBase> pRmqMgrBase);
	bool WaitforReady(); // ���ϲ㷴����¼״̬��ע�⣬�˺�����������ֱ��������ʧ��
	void SetReadyFlag();
	void CloseSocket();
	void SendData(const uint8_t *pDataInfo, const size_t len);
	void Stop();
	void OnErrMsg(std::string& msg);

	boost::shared_ptr<MyConnectionHandler> GetConnectionHandler();
	boost::shared_ptr<AMQP::Connection> GetConnection();

private:
	MqInfo m_mqInfo;
	boost::asio::io_service m_ios;
	boost::asio::io_service::work m_work;
	boost::asio::ip::tcp::socket m_sock;
	int m_numOfWorkThreads;
	boost::thread_group m_threads;
	bool m_socketStarted;
	std::string m_writeBuf;
	char m_readBuf[1];
	std::vector<char> m_recvedBuf;//���ݽ��ջ�����
	boost::shared_ptr<MyConnectionHandler> m_pHandler;
	boost::shared_ptr<AMQP::Connection> m_pConnect;
	boost::shared_ptr<RmqMgrBase> m_pRmqMgrBase;
	std::mutex m_lock;
	bool m_bparse;
	std::promise<bool> m_bready;//�Ƿ�׼����
};

#endif
