#pragma once
#include "google/protobuf/service.h"
#include "tcpconnection.h"
#include <unordered_map>
#include <string>

class RpcProvider
{
public:
    void NotifyService(google::protobuf::Service *service);
    void Run();

private:
    struct ServiceInfo
    {
        google::protobuf::Service *m_service; 
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor*> m_methodMap;
    };

    std::unordered_map<std::string, ServiceInfo> m_serviceMap;

    void OnConnection(const TcpConnectionPtr& conn);
    void OnMessage(const TcpConnectionPtr& conn, Buffer* buffer);
    void SendRpcResponse(const TcpConnectionPtr& conn, google::protobuf::Message* response);
};