/**
 * @file tyke_framework.h
 * @brief 框架主类声明。提供框架初始化、启动、停止及路由获取等生命周期管理接口。
 * @author Nick
 * @date 2026/04/19
 */



#pragma once

#include <string>

#include "request_router.h"
#include "response_router.h"
#include "component/singleton.h"
#include "ipc/ipc_server.h"

namespace tyke
{

    class TykeFramework
    {
    public:

        TykeFramework();

        ~TykeFramework();

        TykeFramework& SetThreadPoolCount(uint32_t thread_pool_count);


        TykeFramework& SetLogConfig(const std::string& log_path, const std::string& log_level, uint32_t file_size_mb,
                                    uint32_t file_count);


        [[nodiscard]] BoolResult Start(std::string_view listen_uuid) const;


        void Shutdown();


        static std::shared_ptr<RequestRouter> GetRequestRouter();


        static std::shared_ptr<ResponseRouter> GetResponseRouter();

    private:
        uint32_t thread_pool_count_ = 4;
        std::string log_path_;
        std::string log_level_ = "info";
        uint32_t file_size_mb_ = 1024;
        uint32_t file_count_ = 5;
    };


    TykeFramework* App();
}