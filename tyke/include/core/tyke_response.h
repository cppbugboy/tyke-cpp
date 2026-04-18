/**
 * @file tyke_response.h
 * @brief Tyke响应对象
 * @author Nick
 * @date 2026/04/17
 *
 * TykeResponse封装了响应的所有信息，包括协议头、元数据和内容数据。
 * 支持对象池管理，提供同步和异步发送方式。
 */

#ifndef TYKE_RESPONSE_H
#define TYKE_RESPONSE_H

#include "response_metadata.h"
#include "common/tyke_def.h"
#include "common/tyke_result.h"
#include "ipc/ipc_types.h"
#include "component/object_pool.hpp"

namespace tyke
{
    /// 发送数据的回调函数类型
    using SendDataHandler = std::function<bool(ClientId, const std::vector<unsigned char> &)>;

    /**
     * @brief Tyke响应类
     *
     * 表示一个完整的响应对象，包含响应元数据和内容数据。
     * 支持同步发送（通过IPC连接）和异步发送（通过目标UUID）两种模式。
     * 使用对象池管理内存，提高性能。
     *
     * 使用示例：
     * @code
     *   auto response = tyke::MakeResponsePtr();
     *   response->SetModule("user")
     *           ->SetRoute("/user/login")
     *           ->SetResult(200, "OK")
     *           ->SetContent(tyke::ContentType::kJson, json_data);
     *   response->Send();
     * @endcode
     */
    class TykeResponse
    {
        friend class DataProc;
        friend class RequestStub;

    public:
        /**
         * @brief 从对象池获取响应对象
         * @return 响应对象指针
         */
        static TykeResponse* Acquire();

        /**
         * @brief 将响应对象释放回对象池
         * @param resp 响应对象指针
         */
        static void Release(TykeResponse* resp);

        /**
         * @brief 重置响应对象状态
         *
         * 清空所有成员变量，使对象恢复到初始状态。
         */
        void Reset();

        TykeResponse();

        /**
         * @brief 获取协议魔数
         * @return 4字节魔数字符串
         */
        const char* GetMagic() const;

        /**
         * @brief 设置消息类型
         * @param msg_type 消息类型枚举值
         * @return 当前响应引用，支持链式调用
         */
        TykeResponse& SetMessageType(MessageType msg_type);

        /**
         * @brief 获取消息类型
         * @return 消息类型枚举值
         */
        MessageType GetMessageType() const;

        /**
         * @brief 设置模块名称
         * @param module 模块名称
         * @return 当前响应引用，支持链式调用
         */
        TykeResponse& SetModule(const std::string& module);

        /**
         * @brief 获取模块名称
         * @return 模块名称字符串
         */
        std::string GetModule() const;

        /**
         * @brief 设置消息UUID
         * @param msg_uuid 消息UUID
         * @return 当前响应引用，支持链式调用
         */
        TykeResponse& SetMsgUuid(const std::string& msg_uuid);

        /**
         * @brief 获取消息UUID
         * @return 消息UUID字符串
         */
        std::string GetMsgUuid() const;

        /**
         * @brief 设置路由路径
         * @param route 路由路径
         * @return 当前响应引用，支持链式调用
         */
        TykeResponse& SetRoute(const std::string& route);

        /**
         * @brief 获取路由路径
         * @return 路由路径字符串
         */
        std::string GetRoute() const;

        /**
         * @brief 获取响应内容
         * @param content_type 输出内容类型字符串
         * @param content 输出内容数据
         */
        void GetContent(std::string& content_type, std::vector<unsigned char>& content) const;

        /**
         * @brief 设置响应内容
         * @param content_type 内容类型
         * @param response_content 内容数据
         * @return 当前响应引用，支持链式调用
         */
        TykeResponse& SetContent(const ContentType& content_type,
                                 const std::vector<unsigned char>& response_content);

        /**
         * @brief 添加自定义元数据
         * @param key 元数据键名
         * @param value 元数据值
         * @return 成功返回true，失败返回错误信息
         */
        nonstd::expected<bool, std::string> AddMetadata(const std::string& key, const JsonValue& value);

        /**
         * @brief 获取自定义元数据
         * @param key 元数据键名
         * @return 存在返回值，不存在返回nullopt
         */
        nonstd::optional<JsonValue> GetMetadata(const std::string& key);

        /**
         * @brief 设置响应结果
         * @param status 状态码（如200、404、500等）
         * @param reason 原因描述
         * @return 当前响应引用，支持链式调用
         */
        TykeResponse& SetResult(int status, const std::string& reason);

        /**
         * @brief 获取响应结果
         * @param status 输出状态码
         * @param reason 输出原因描述
         */
        void GetResult(int& status, std::string& reason) const;

        /**
         * @brief 设置异步目标UUID
         * @param target_uuid 目标服务器UUID
         * @return 当前响应引用，支持链式调用
         */
        TykeResponse& SetAsyncUuid(const std::string& target_uuid);

        /**
         * @brief 获取异步目标UUID
         * @return 目标服务器UUID字符串
         */
        std::string GetAsyncUuid() const;

        /**
         * @brief 设置发送数据回调函数
         * @param send_data_handler 发送数据的回调函数
         * @return 当前响应引用，支持链式调用
         */
        TykeResponse& SetSendDataHandler(const SendDataHandler& send_data_handler);

        /**
         * @brief 设置IPC客户端标识
         * @param client_id 客户端标识
         * @return 当前响应引用，支持链式调用
         */
        TykeResponse& SetIpcFD(ClientId client_id);

        /**
         * @brief 同步发送响应
         * @return 成功返回true，失败返回错误信息
         *
         * 通过设置的send_data_handler回调发送响应数据。
         */
        BoolResult Send();

        /**
         * @brief 异步发送响应
         * @return 成功返回true，失败返回错误信息
         *
         * 通过目标UUID异步发送响应数据。
         */
        BoolResult SendAsync();

    private:
        ProtocolHeader protocol_header_;                    ///< 协议头
        ResponseMetadata metadata_;                          ///< 响应元数据
        std::vector<unsigned char> content_;                 ///< 内容数据
        bool is_send_ = false;                               ///< 是否已发送标志
        ClientId client_id_{};                               ///< 客户端标识
        SendDataHandler send_data_handler_;                  ///< 发送数据回调函数
        std::string target_uuid_;                            ///< 异步目标UUID

        static ObjectPool<TykeResponse> pool_;               ///< 对象池实例
    };

    /**
     * @brief TykeResponse对象池删除器
     *
     * 用于unique_ptr自动将对象释放回对象池。
     */
    struct TykeResponseDeleter
    {
        void operator()(TykeResponse* p) const
        {
            TykeResponse::Release(p);
        }
    };

    /// TykeResponse的unique_ptr类型别名
    using TykeResponsePtr = std::unique_ptr<TykeResponse, TykeResponseDeleter>;

    /**
     * @brief 从对象池获取响应对象并包装为unique_ptr
     * @return 包装了响应对象的unique_ptr
     */
    inline TykeResponsePtr MakeResponsePtr()
    {
        return TykeResponsePtr(TykeResponse::Acquire());
    }
} // tyke

#endif //TYKE_RESPONSE_H