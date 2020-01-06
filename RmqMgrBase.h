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
	string routingKey; // 用于发送端发布消息
	string bindingKey; // 接收端绑定queue时使用
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
	// 启动rabbitmq client，内部会启动线程异步处理rabbitmq指令
	void ReStartMqInstance(string &err);

	// 注册回调函数
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

	// 返回错误信息，由子函数记录日志并处理
	virtual void OnRtnErrMsg(string &err) = 0;
protected:
	// 由子函数实现
	// 接收到数据，请勿进行耗时操作，否则会造成消息堆积。如果客户端负荷过重，建议使用负载均衡模式
	virtual void OnRecvedData(const char *data, const uint64_t len) = 0;
	// 返回状态
	virtual void OnStatusChange(const bool isOk) = 0;
};

// 处理TCP连接的类
class MyConnectionHandler : public AMQP::ConnectionHandler
{
private:
	boost::shared_ptr<TcpClient> m_pTcpClient;

public:
	MyConnectionHandler(boost::shared_ptr<TcpClient> pTcpClient);
	~MyConnectionHandler();

	// 数据待发送出去
	virtual void onData(AMQP::Connection *connection, const char *data, size_t size);
	// Rmq登录成功
	virtual void onReady(AMQP::Connection *connection);
	// 发生错误，一般发生此错误后连接不再可用
	virtual void onError(AMQP::Connection *connection, const char *message);
	// 对端关闭连接
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
	bool WaitforReady(); // 向上层反馈登录状态，注意，此函数会阻塞，直到就绪或失败
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
	std::vector<char> m_recvedBuf;//数据接收缓冲区
	boost::shared_ptr<MyConnectionHandler> m_pHandler;
	boost::shared_ptr<AMQP::Connection> m_pConnect;
	boost::shared_ptr<RmqMgrBase> m_pRmqMgrBase;
	std::mutex m_lock;
	bool m_bparse;
	std::promise<bool> m_bready;//是否准备好
};

#endif
