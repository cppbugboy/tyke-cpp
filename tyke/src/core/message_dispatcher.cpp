#include "core/message_dispatcher.h"

#include <cstring>

namespace tyke
{
    std::unordered_map<MessageType, MessageHandler>& MessageDispatcher::GetHandlers()
    {
        static std::unordered_map<MessageType, MessageHandler> handlers;
        return handlers;
    }

    void MessageDispatcher::RegisterHandler(const MessageType type, MessageHandler handler)
    {
        GetHandlers()[type] = std::move(handler);
    }

    void MessageDispatcher::Dispatch(const ClientId client_id, const std::vector<unsigned char>& data, uint32_t& used)
    {
        ProtocolHeader header;
        std::memcpy(&header, data.data(), sizeof(ProtocolHeader));

        auto& handlers = GetHandlers();
        if (const auto it = handlers.find(header.msg_type); it != handlers.end())
        {
            it->second(client_id, data, used);
        }
    }

    bool MessageDispatcher::HasHandler(const MessageType type)
    {
        return GetHandlers().find(type) != GetHandlers().end();
    }
}
