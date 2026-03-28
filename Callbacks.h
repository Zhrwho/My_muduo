#pragma once

#include <functional>
#include <memory>

class Buffer;
class TcpConnection;
class Timestamp;


using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using ConnectionCallback = std::function<void (const TcpConnectionPtr&)>; /*获取连接状态*/
using CloseCallback = std::function<void (const TcpConnectionPtr&)>;
using WriteCompleteCallback = std::function<void (const TcpConnectionPtr)>;

using MessageCallback = std::function<void (const TcpConnectionPtr&
                                            , Buffer*
                                            , Timestamp)>;

/*高水位回调？把水位控制记载水位线以下，对端接收慢，发送很快的话，数据就会丢失*/
/*到达水位线，就先暂停发送*/
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr&, std::size_t)>;
