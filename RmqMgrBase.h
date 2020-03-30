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
	string routingKey; // 用于发送端发布消息
	string bindingKey; // 接收端绑定queue时使用
} MqInfo;

class MyConnectionHandler;
class TcpMgr;

class RmqMgrBase
{
public:
	RmqMgrBase();
	virtual ~RmqMgrBase();

	// 初始化时调用，设置RabbitMq基本信息
	bool MqInfoInit(const MqInfo &mqInfo, string &err);
	// 初始化时调用，启动连接、建立通道、创建组件并开启消费
	bool StartMqInstance(string &err);
	// 发送消息时调用。特别注意：该函数非线程安全，需要确保在单一线程中调用
	bool PublishMsg(const string &msg, string &err);
	// 释放实例
	bool ReleaseMqInstance(string &err, const bool isAutoResume = false);
	// 返回错误信息，由子函数记录日志并处理
	virtual void OnRtnErrMsg(string &err) = 0;

protected:
	// 由子函数实现
	// 接收到数据，请勿进行耗时操作，否则会造成消息堆积。如果客户端负荷过重，建议使用负载均衡模式
	virtual void OnRecvedData(const char *data, const uint64_t len) = 0;
	// 返回状态
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

	// 注册回调函数
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

// 处理TCP连接的类
class MyConnectionHandler : public AMQP::ConnectionHandler
{
private:
	boost::shared_ptr<TcpMgr> m_pTcpMgr;

public:
	MyConnectionHandler(boost::shared_ptr<TcpMgr> pTcpMgr);

	// 数据待发送出去
	virtual void onData(AMQP::Connection *connection, const char *data, size_t size);
	// Rmq登录成功
	virtual void onReady(AMQP::Connection *connection);
	// 发生错误，一般发生此错误后连接不再可用
	virtual void onError(AMQP::Connection *connection, const char *message);
	// 对端关闭连接
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
	static const int kNumOfWorkThreads = 1; // 处理tcp的线程数量，如果是多线程，需要考虑同步问题
	boost::thread_group m_threads;
	bool m_socketStarted;
	std::vector<char> m_recv_buffer;
	std::vector<char> m_parseBuf; // 数据解析缓冲区
	std::list<int32_t> m_pending_recvs;
	static const int kReceiveBufferSize = 4096; // 默认每次接收数据量
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
