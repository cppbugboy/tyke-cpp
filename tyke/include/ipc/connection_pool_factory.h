/**
 * @file connection_pool_factory.h
 * @brief 连接池工厂声明。通过服务端UUID管理多个连接池实例。
 * @author Nick
 * @date 2026/04/20
 *
 * 单例工厂，按server_uuid创建和管理ConnectionPool实例。
 * 支持动态创建、移除和全局关闭连接池。
 */


#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "component/singleton.h"
#include "connection_pool.h"

namespace tyke
{
    class ConnectionPoolFactory
    {
    public:
        ConnectionPoolFactory();
        ~ConnectionPoolFactory();

        std::shared_ptr<ConnectionPool> GetPool(const std::string& server_uuid);

        void RemovePool(const std::string& server_uuid);

        void Shutdown();

    private:
        std::mutex mutex_;
        std::unordered_map<std::string, std::shared_ptr<ConnectionPool>> pools_;
    };
}