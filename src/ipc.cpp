#include <acul/ipc.hpp>

namespace acul
{
    bool transact_ipc(const string &endpoint, const void *request, size_t request_size, void *response,
                      size_t response_size, u32 timeout_ms)
    {
        if (!request || request_size == 0 || !response || response_size == 0) return false;

        ipc_connection connection;
        if (!connect_ipc(connection, endpoint, timeout_ms)) return false;
        const bool ok = write_ipc(connection, request, request_size, timeout_ms) &&
                        read_ipc(connection, response, response_size, timeout_ms);
        close_ipc(connection);
        return ok;
    }
} // namespace acul
