#include <acul/exception/exception.hpp>
#include <acul/string/sstream.hpp>
#include <acul/vector.hpp>
#include <signal.h>

namespace acul
{
    void capture_stack_trace(except_info &info)
    {
        constexpr size_t MaxFrames = 64;
        void *buffer[MaxFrames];

        int nptrs = backtrace(buffer, MaxFrames);

        vector<except_addr> addresses;
        for (int i = 0; i < nptrs; ++i)
        {
            Dl_info dlinfo;
            if (dladdr(buffer[i], &dlinfo))
                addresses.push_back({buffer[i], dlinfo.dli_fbase});
            else
                addresses.push_back({buffer[i], nullptr});
        }

        info.addresses_count = addresses.size();
        info.addresses = addresses.release();
    }

    APPLIB_API void write_exception_info(int sig, siginfo_t *info, const ucontext_t &context, stringstream &stream)
    {
        stream << format("Signal: %d (%s)\n", sig, strsignal(sig));

        if (info)
        {
            stream << format("Signal code: %d\n", info->si_code);
            stream << format("Fault address: %p\n", info->si_addr);

            if (sig == SIGSEGV || sig == SIGBUS || sig == SIGFPE || sig == SIGILL)
            {
                stream << "Signal details: ";
                switch (info->si_code)
                {
                    case SEGV_MAPERR:
                        stream << "Address not mapped to object\n";
                        break;
                    case SEGV_ACCERR:
                        stream << "Invalid permissions for mapped object\n";
                        break;
                    default:
                        stream << "Unknown reason\n";
                        break;
                }
            }
        }
    }
} // namespace acul