#pragma once

#include <functional>
#include <unordered_map>

#include "common/tyke_def.h"
#include "ipc/ipc_types.h"

namespace tyke
{
    using MessageHandler = std::function<void(ClientId, const std::vector<unsigned char>&, uint32_t&)>;

    class MessageDispatcher
    {
    public:
        MessageDispatcher() = delete;
        ~MessageDispatcher() = delete;

        static void RegisterHandler(MessageType type, MessageHandler handler);
        static void Dispatch(ClientId client_id, const std::vector<unsigned char>& data, uint32_t& used);
        static bool HasHandler(MessageType type);

    private:
        static std::unordered_map<MessageType, MessageHandler>& GetHandlers();
    };
}
