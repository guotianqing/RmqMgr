#include "RmqMgrBase.h"

#include <boost/make_shared.hpp>
#include <boost/bind.hpp>
#include <functional>
#include <thread>
#include <chrono>

using namespace std;

RmqMgrBase::RmqMgrBase(/* args */)
{
	m_handler = nullptr;
	m_connection = nullptr;
	m_channel = nullptr;
	m_quitFlag = false;
	m_isAutoResume = false;
	m_pTcpClient = nullptr;
}

RmqMgrBase::~RmqMgrBase()
{
}

bool RmqMgrBase::MqInfoInit(const MqInfo &mqInfo, string &err)
{
	try
	{
		m_mqInfo = mqInfo;
	}
	catch (const std::exception &e)
	{
		err.assign("初始化mqinfo失败：" + string(e.what()));
		return false;
	}

	return true;
}

bool RmqMgrBase::StartMqInstance(string &err)
{
	try
	{
		if (m_pTcpClient == nullptr)
		{
			m_pTcpClient = boost::make_shared<TcpClient>();
			m_pTcpClient->Start(m_mqInfo, boost::shared_ptr<RmqMgrBase>(this));
		}

		// 等待Rmq登录成功
		if (!m_pTcpClient->WaitforReady())
		{
			err.assign("Rmq登录失败");
			OnRtnErrMsg(err);
			OnStatusChange(false);
			ReStartMqInstance(err);
		}

		if (m_handler == nullptr)
		{
			//m_handler = boost::make_shared<MyConnectionHandler>(m_pTcpClient);
			m_handler = m_pTcpClient->GetConnectionHandler();
		}

		// make a connection
		if (!CreateMqConnection(m_mqInfo.ip, m_mqInfo.port, m_mqInfo.loginName, m_mqInfo.loginPwd, err))
		{
			OnRtnErrMsg(err);
			OnStatusChange(false);
			ReStartMqInstance(err);
		}

		// make a channel
		if (m_channel == nullptr || !(m_channel->usable()))
		{
			// make sure connection ok
			if (!CreateMqConnection(m_mqInfo.ip, m_mqInfo.port, m_mqInfo.loginName, m_mqInfo.loginPwd, err))
			{
				OnRtnErrMsg(err);
				OnStatusChange(false);
				ReStartMqInstance(err);
			}
			if (!CreateMqChannel(err))
			{
				OnRtnErrMsg(err);
				OnStatusChange(false);
				ReStartMqInstance(err);
			}
		}

		if (!CreateMqExchange(m_mqInfo.exchangeName, m_mqInfo.exchangeType, err))
		{
			OnRtnErrMsg(err);
			OnStatusChange(false);
			ReStartMqInstance(err);
		}

		if (!CreateMqQueue(m_mqInfo.queueName, err))
		{
			OnRtnErrMsg(err);
			OnStatusChange(false);
			ReStartMqInstance(err);
		}

		if (!BindQueue(m_mqInfo.queueName, m_mqInfo.exchangeName, m_mqInfo.bindingKey, err))
		{
			OnRtnErrMsg(err);
			OnStatusChange(false);
			ReStartMqInstance(err);
		}

		if (!StartConsumeMsg(m_mqInfo.queueName, err))
		{
			OnRtnErrMsg(err);
			OnStatusChange(false);
			ReStartMqInstance(err);
		}
	}
	catch (const std::exception &e)
	{
		err.assign("初始化mq实例失败：" + string(e.what()));
		OnRtnErrMsg(err);
		OnStatusChange(false);
		ReStartMqInstance(err);
	}

	return true;
}

void RmqMgrBase::ReStartMqInstance(string &err)
{
	m_quitFlag = false;
	m_isAutoResume = true;
	while (!m_quitFlag)
	{
		OnRtnErrMsg(err.append(", 尝试重新启动"));
		std::this_thread::sleep_for(std::chrono::seconds(2));
		// if (!ReleaseMqInstance(err, m_isAutoResume))
		// {
		//     OnRtnErrMsg(err);
		//     continue;
		// }
		if (!StartMqInstance(err))
		{
			OnRtnErrMsg(err);
			continue;
		}
		m_quitFlag = true;
		m_isAutoResume = false;
	}
}

bool RmqMgrBase::ReleaseMqInstance(string &err, const bool m_isAutoResume)
{
	try
	{
		if (!m_isAutoResume)
		{
			m_quitFlag = true;
		}

		if (!CloseMqChannel(err))
		{
			return false;
		}

		if (!CloseMqConnection(err))
		{
			return false;
		}

		m_pTcpClient->Stop();
	}
	catch (const std::exception &e)
	{
		err.assign("释放mq实例失败：" + string(e.what()));
		return false;
	}

	return true;
}

bool RmqMgrBase::CreateMqConnection(const string &ip, const unsigned short &port, const string &loginName, const string &loginPwd, string &err)
{
	try
	{
		if (m_connection == nullptr || !m_connection->ready() || !m_connection->usable())
		{
			//string connInfo = "amqp://" + loginName + ":" + loginPwd + "@" + ip + ":" + to_string(port);
			//m_connection = boost::make_shared<AMQP::Connection >(m_handler.get(), AMQP::Address(connInfo));
			m_connection = m_pTcpClient->GetConnection();
			//m_connection = boost::make_shared<AMQP::Connection>(m_handler.get(), AMQP::Login(m_mqInfo.loginName, m_mqInfo.loginPwd), "/");
		}
	}
	catch (const std::exception &e)
	{
		err.assign("创建Connection失败：" + string(e.what()));
		return false;
	}

	return true;
}

bool RmqMgrBase::CloseMqConnection(string &err)
{
	try
	{
		if (m_connection->usable())
		{
			bool ret = m_connection->close();
			if (!ret)
			{
				err.assign("关闭连接失败");
				return ret;
			}
		}
	}
	catch (const std::exception &e)
	{
		err.assign("关闭连接失败" + string(e.what()));
		return false;
	}

	return true;
}

bool RmqMgrBase::CreateMqChannel(string &err)
{
	try
	{
		// we need a channel too
		// AMQP::TcpChannel channel(m_connection.get());
		m_channel = boost::make_shared<AMQP::Channel>(m_connection.get());

		// 通道发生错误时调用回调函数
		m_channel->onError(std::bind(&RmqMgrBase::ChannelErrCb, this, std::placeholders::_1));
		m_channel->onReady(std::bind(&RmqMgrBase::ChannelOkCb, this));
	}
	catch (const std::exception &e)
	{
		err.assign("创建channel失败：" + string(e.what()));
		return false;
	}

	return true;
}

void RmqMgrBase::ChannelErrCb(const char *msg)
{
	string err;
	err = "当前通道发生错误：" + string(msg);
	OnRtnErrMsg(err);
	OnStatusChange(false);
	ReStartMqInstance(err);
}

void RmqMgrBase::ChannelOkCb()
{
	OnStatusChange(true);
}

bool RmqMgrBase::CloseMqChannel(string &err)
{
	try
	{
		if (m_channel->usable())
		{
			m_channel->close()
				.onError(std::bind(&RmqMgrBase::ChannelCloseErrCb, this, std::placeholders::_1));
		}
	}
	catch (const std::exception &e)
	{
		err.assign("关闭channel失败：" + string(e.what()));
		return false;
	}

	return true;
}

void RmqMgrBase::ChannelCloseErrCb(const char *msg)
{
	string err;
	err = "关闭通道发生错误：" + string(msg);
	OnRtnErrMsg(err);
}

bool RmqMgrBase::CreateMqExchange(const string &exchangeName, const string &exchangeType, string &err)
{
	try
	{
		AMQP::ExchangeType type;
		if (exchangeType == "topic")
		{
			type = AMQP::topic;
		}
		else if (exchangeType == "direct")
		{
			type = AMQP::direct;
		}
		else if (exchangeType == "fanout")
		{
			type = AMQP::fanout;
		}
		else if (exchangeType == "headers")
		{
			type = AMQP::headers;
		}
		else if (exchangeType == "consistent_hash")
		{
			type = AMQP::consistent_hash;
		}
		else
		{
			err.assign("创建Exchange失败：未知的交换器类型：" + exchangeType);
			return false;
		}

		m_channel->declareExchange(exchangeName, type)
			.onError(std::bind(&RmqMgrBase::CreatMqExchangeErrCb, this, std::placeholders::_1));
	}
	catch (const std::exception &e)
	{
		err.assign("创建Exchange失败：" + string(e.what()));
		return false;
	}

	return true;
}

void RmqMgrBase::CreatMqExchangeErrCb(const char *msg)
{
	string err;
	err = "创建Exchange发生错误：" + string(msg);
	OnRtnErrMsg(err);
	OnStatusChange(false);
	ReStartMqInstance(err);
}

bool RmqMgrBase::CreateMqQueue(const string &queueName, string &err)
{
	try
	{
		m_channel->declareQueue(queueName, AMQP::exclusive)
			.onError(std::bind(&RmqMgrBase::CreatMqQueueErrCb, this, std::placeholders::_1));
	}
	catch (const std::exception &e)
	{
		err.assign("创建Queue失败：" + string(e.what()));
		return false;
	}

	return true;
}

void RmqMgrBase::CreatMqQueueErrCb(const char *msg)
{
	string err;
	err = "创建Queue发生错误：" + string(msg);
	OnRtnErrMsg(err);
	OnStatusChange(false);
	ReStartMqInstance(err);
}

bool RmqMgrBase::BindQueue(const string &queueName, const string &exchangeName, const string &bindingKey, string &err)
{
	try
	{
		m_channel->bindQueue(exchangeName, queueName, bindingKey)
			.onError(std::bind(&RmqMgrBase::BindQueueErrCb, this, std::placeholders::_1));
	}
	catch (const std::exception &e)
	{
		err.assign("绑定Queue失败：" + string(e.what()));
		return false;
	}

	return true;
}

void RmqMgrBase::BindQueueErrCb(const char *msg)
{
	string err;
	err = "绑定Queue发生错误：" + string(msg);
	OnRtnErrMsg(err);
	OnStatusChange(false);
	ReStartMqInstance(err);
}

// 设置客户端同时处理的任务数量，结合多个客户端，可以实现公平的任务处理
// 默认不设置时，任务按顺序分发，当客户端负荷较重时，可能导致业务处理延迟
bool RmqMgrBase::SetQosValue(const uint16_t val, string &err)
{
	try
	{
		m_channel->setQos(val)
			.onError(std::bind(&RmqMgrBase::SetQosValueErrCb, this, std::placeholders::_1));
	}
	catch (const std::exception &e)
	{
		err.assign("设置Qos值失败：" + string(e.what()));
		return false;
	}
	return true;
}

void RmqMgrBase::SetQosValueErrCb(const char *msg)
{
	string err;
	err = "设置Qos值发生错误：" + string(msg);
	OnRtnErrMsg(err);
	OnStatusChange(false);
	ReStartMqInstance(err);
}

bool RmqMgrBase::PublishMsg(const string &msg, string &err)
{
	try
	{
		std::lock_guard<mutex> lck(m_mtxPublishMsg);
		m_channel->publish(m_mqInfo.exchangeName, m_mqInfo.routingKey, msg)
			.onSuccess(std::bind(&RmqMgrBase::PublishMsgOkCb, this))
			.onError(std::bind(&RmqMgrBase::PublishMsgErrCb, this, std::placeholders::_1));
	}
	catch (const std::exception &e)
	{
		err.assign("发布消息失败：" + string(e.what()));
		return false;
	}

	return true;
}

void RmqMgrBase::PublishMsgErrCb(const char *msg)
{
	string err;
	err = "发布消息发生错误：" + string(msg);
	OnRtnErrMsg(err);
	OnStatusChange(false);
	ReStartMqInstance(err);
}

void RmqMgrBase::PublishMsgOkCb()
{
	//m_handler->UpdateHbCurrTime();
}

bool RmqMgrBase::StartConsumeMsg(const string &queueName, string &err)
{
	try
	{
		// 默认需要ack
		m_channel->consume(queueName)
			.onReceived(std::bind(&RmqMgrBase::ConsumeRecvedCb, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
			.onError(std::bind(&RmqMgrBase::ConsumeErrorCb, this, std::placeholders::_1));
	}
	catch (const std::exception &e)
	{
		err.assign("消费消息失败：" + string(e.what()));
		return false;
	}
	return true;
}

void RmqMgrBase::ConsumeRecvedCb(const AMQP::Message &message, uint64_t deliveryTag, bool redelivered)
{
	//m_handler->UpdateHbCurrTime();
	string msg(message.body(), message.bodySize());
	OnRecvedData(msg.c_str(), msg.length()); // 由相应的子函数处理

	// acknowledge the message
	m_channel->ack(deliveryTag);
}

void RmqMgrBase::ConsumeErrorCb(const char *msg)
{
	string err;
	err = "消费消息发生错误：" + string(msg);
	OnRtnErrMsg(err);
	OnStatusChange(false);
	ReStartMqInstance(err);
}

void RmqMgrBase::ConsumeOkCb(const std::string &consumertag)
{
	//m_handler->UpdateHbCurrTime();
}

// MyConnectionHandler
MyConnectionHandler::MyConnectionHandler(boost::shared_ptr<TcpClient> pTcpClient)
{
	m_pTcpClient = pTcpClient;
}

MyConnectionHandler::~MyConnectionHandler()
{
}

void MyConnectionHandler::onData(AMQP::Connection *connection, const char *data, size_t size)
{
	try
	{
		m_pTcpClient->SendData((const uint8_t*)data, size);
	}
	catch (const std::exception&e)
	{
		m_pTcpClient->OnErrMsg("向RabbitMq发送数据失败：" + string(e.what()));
	}
}

void MyConnectionHandler::onReady(AMQP::Connection *connection)
{
	m_pTcpClient->SetReadyFlag();
}

void MyConnectionHandler::onError(AMQP::Connection *connection, const char *message)
{
	m_pTcpClient->OnErrMsg("RabbitMq发生错误：" + string(message));
}

void MyConnectionHandler::onClosed(AMQP::Connection *connection)
{
	m_pTcpClient->OnErrMsg(string("RabbitMq对端关闭连接"));
}

// TcpClient
TcpClient::TcpClient()
	: m_work(m_ios),
	m_sock(m_ios)
{
	m_socketStarted = false;
	m_numOfWorkThreads = 1;
	memset(m_readBuf, 0, sizeof(m_readBuf));
	m_bparse = false;
	m_pHandler = nullptr;
	m_pConnect = nullptr;
	m_pRmqMgrBase = nullptr;
}

TcpClient::~TcpClient()
{
}

void TcpClient::Run()
{
	m_ios.run();
}

void TcpClient::Stop()
{
	m_ios.stop();
	m_threads.join_all();
}

void TcpClient::SetReadyFlag()
{
	m_bready.set_value(true);
}

void TcpClient::Start(const MqInfo& mqInfo, boost::shared_ptr<RmqMgrBase> pRmqMgrBase)
{
	m_mqInfo = mqInfo;
	m_pRmqMgrBase = pRmqMgrBase;
	m_pHandler = boost::make_shared<MyConnectionHandler>(shared_from_this());
	boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string(m_mqInfo.ip), m_mqInfo.port);
	m_sock.async_connect(ep, MEM_FN1(OnConnect, _1));
	while (m_numOfWorkThreads--)
	{
		m_threads.create_thread(boost::bind(&TcpClient::Run, this));
	}
}

bool TcpClient::WaitforReady()
{
	std::future<bool> ret = m_bready.get_future();
	if (ret.wait_for(std::chrono::seconds(INFINITE)) == std::future_status::ready)
	{
		return true;
	}
	return false;
}

void TcpClient::OnConnect(const error_code &err)
{
	if (!err)
	{
		m_socketStarted = true;
		// tcp连接建立成功，开始登录Rmq
		m_pConnect = boost::make_shared<AMQP::Connection>(m_pHandler.get(), AMQP::Login(m_mqInfo.loginName, m_mqInfo.loginPwd));
		// 开始监听是否有数据到来
		RecvData();
	}
	else
	{
		m_bready.set_value(false);
		OnErrMsg("与RabbitMq建立连接失败：" + err.message());
		m_socketStarted = false;
		ReConnectServer();
	}
}

void TcpClient::ReConnectServer()
{
	std::this_thread::sleep_for(std::chrono::seconds(2));
	boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string(m_mqInfo.ip), m_mqInfo.port);
	m_sock.async_connect(ep, MEM_FN1(OnConnect, _1));
}

void TcpClient::OnReadData(const error_code &err, size_t bytes)
{
	if (!err)
	{
		{
			// 保存数据到已接收缓存
			std::lock_guard<std::mutex> guard(m_lock);
			m_recvedBuf.insert(m_recvedBuf.end(), m_readBuf, m_readBuf + bytes);
		}
		RecvData();
		parse();
	}
	else
	{
		OnErrMsg("接收数据发生错误：" + err.message());
		CloseSocket();
		ReConnectServer();
	}
}

void TcpClient::parse()
{
	try
	{
		uint64_t use = 0;

		///> 不阻塞线程，parse一次只能一个线程调用，不能多个线程
		std::unique_lock<std::mutex> guard(m_lock, std::try_to_lock);
		if (!guard.owns_lock())
			return;
		if (m_bparse)
			return;
		m_bparse = true;
		size_t size = m_recvedBuf.size();
		while (size - use >= m_pConnect->expected())
		{
			std::vector<char> buff(m_recvedBuf.begin() + use, m_recvedBuf.begin() + use + m_pConnect->expected());
			///> 解析期间打开锁，允许接收数据
			guard.unlock();
			use += m_pConnect->parse(buff.data(), buff.size());
			guard.lock();
		}

		m_recvedBuf.erase(m_recvedBuf.begin(), m_recvedBuf.begin() + use);
		m_bparse = false;
	}
	catch (const std::exception&e)
	{
		OnErrMsg("解析Rmq数据发生错误：" + string(e.what()));
	}
}

void TcpClient::OnWrite(const error_code &err, size_t bytes)
{
	if (err)
	{
		OnErrMsg("发送数据失败：" + err.message());
	}
	else
	{
		//cout << "data send succ, len = " << bytes << endl;
	}
}

void TcpClient::SendData(const uint8_t *data, const size_t len)
{
	if (!IsSocketStarted())
	{
		OnErrMsg(string("发送数据失败：socket尚未启动"));
		return;
	}
	m_writeBuf.assign(data, data + len);
	m_sock.async_write_some(boost::asio::buffer(data, len), MEM_FN2(OnWrite, _1, _2));
}

void TcpClient::RecvData()
{
	async_read(m_sock, boost::asio::buffer(m_readBuf, sizeof(m_readBuf)), MEM_FN2(OnReadData, _1, _2));
}

void TcpClient::CloseSocket()
{
	if (!m_socketStarted)
		return;
	m_socketStarted = false;
	m_sock.close();
	OnErrMsg(string("本端关闭连接"));
}

bool TcpClient::IsSocketStarted()
{
	return m_socketStarted;
}

boost::shared_ptr<MyConnectionHandler> TcpClient::GetConnectionHandler()
{
	return m_pHandler;
}

boost::shared_ptr<AMQP::Connection> TcpClient::GetConnection()
{
	return m_pConnect;
}

void TcpClient::OnErrMsg(std::string& msg)
{
	m_pRmqMgrBase->OnRtnErrMsg(msg);
}