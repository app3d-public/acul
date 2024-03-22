#include <core/log.hpp>
#include <ctime>
#include <regex>
#include <thread>

namespace logging
{
    std::string TimeTokenHandler::handle(const LogMessageData &context) const
    {
        using namespace std::chrono;

        auto now = system_clock::now();
        auto now_ns = now.time_since_epoch();
        long long ns = duration_cast<nanoseconds>(now_ns).count() % 1000000000;

        time_t time_t_now = system_clock::to_time_t(now);
        std::tm tm_now;

#ifdef _WIN32
        localtime_s(&tm_now, &time_t_now);
#else
        localtime_r(&time_t_now, &tm_now);
#endif

        return format("%04d-%02d-%02d %02d:%02d:%02d.%09lld", tm_now.tm_year + 1900, tm_now.tm_mon + 1,
                            tm_now.tm_mday, tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec, ns);
    }

    std::string ThreadIdTokenHandler::handle(const LogMessageData &context) const
    {
        std::stringstream ss;
        ss << std::this_thread::get_id();
        return ss.str();
    }

    std::string LevelNameTokenHandler::handle(const LogMessageData &context) const
    {
        switch (context.level)
        {
            case LogLevel::Info:
                return "INFO";
            case LogLevel::Debug:
                return "DEBUG";
            case LogLevel::Trace:
                return "TRACE";
            case LogLevel::Warn:
                return "WARN";
            case LogLevel::Error:
                return "ERROR";
            case LogLevel::Fatal:
                return "FATAL";
            default:
                return "UNKNOWN";
        }
    }

    std::string ColorizeTokenHandler::handle(const LogMessageData &context) const
    {
        switch (context.level)
        {
            case LogLevel::Fatal:
                return std::string(colors::magenta);
            case LogLevel::Error:
                return std::string(colors::red);
            case LogLevel::Warn:
                return std::string(colors::yellow);
            case LogLevel::Info:
                return std::string(colors::green);
            case LogLevel::Debug:
                return std::string(colors::blue);
            case LogLevel::Trace:
                return std::string(colors::cyan);
            default:
                return std::string(colors::reset);
        }
    };

    FileLogger::FileLogger(const std::string &name, const std::string &path, std::ios_base::openmode flags)
        : Logger(name), _filepath(path)
    {
        _fs.open(path, flags);
    }

    FileLogger::~FileLogger()
    {
        if (_fs.is_open())
            _fs.close();
    }

    void FileLogger::write(const std::string &message)
    {
        if (_fs.is_open())
            _fs << message << std::endl;
    }

    void Logger::setPattern(const std::string &pattern)
    {
        _tokens->clear();
        std::regex token_regex("%\\((.*?)\\)");
        std::sregex_iterator it(pattern.begin(), pattern.end(), token_regex);
        std::sregex_iterator end;

        std::size_t last_pos = 0;
        for (; it != end; ++it)
        {
            std::size_t pos = it->position();
            if (pos != last_pos)
            {
                std::string text = pattern.substr(last_pos, pos - last_pos);
                _tokens->push_back(std::make_shared<TextTokenHandler>(text));
            }

            std::unordered_map<std::string, std::shared_ptr<TokenHandler>> tokenHandlers = {
                {"ascii_time", std::make_shared<TimeTokenHandler>()},
                {"level_name", std::make_shared<LevelNameTokenHandler>()},
                {"thread", std::make_shared<ThreadIdTokenHandler>()},
                {"message", std::make_shared<MessageTokenHandler>()},
                {"color_auto", std::make_shared<ColorizeTokenHandler>()},
                {"color_off", std::make_shared<DecolorizeTokenHandler>()}};

            std::string token = it->str(1);
            auto handlerIter = tokenHandlers.find(token);
            if (handlerIter != tokenHandlers.end())
                _tokens->push_back(handlerIter->second);

            last_pos = pos + it->length();
        }
        if (last_pos != pattern.length())
        {
            std::string text = pattern.substr(last_pos);
            _tokens->push_back(std::make_shared<TextTokenHandler>(text));
        }
    }

    std::string Logger::getParsedStr(const LogMessageData &context) const
    {
        std::stringstream ss;
        for (auto &token : *_tokens)
            ss << token->handle(context);
        return ss.str();
    }

    LogTaskManager::LogTaskManager() { _taskThread = std::thread(&LogTaskManager::taskWorkerThread, this); }

    LogTaskManager &LogTaskManager::getSingleton()
    {
        static LogTaskManager singleton;
        return singleton;
    }

    void LogTaskManager::taskWorkerThread()
    {
        while (_running)
        {
            ITask* task;
            if (_tasks.try_pop(task))
            {
                task->onRun();
                delete task;
            }
            else
            {
                std::unique_lock<std::mutex> lock(_taskMutex);
                _taskAvailable.wait(lock, [this]() { return !_tasks.empty() || !_running; });
            }
        }
    }

    LogTaskManager::~LogTaskManager()
    {
        {
            std::unique_lock<std::mutex> lockTask(_taskMutex);
            _running = false;
            _taskAvailable.notify_one();
        }
        _taskThread.join();
    }

    LogManager &LogManager::getSingleton()
    {
        static LogManager singleton;
        return singleton;
    }

    std::shared_ptr<Logger> LogManager::getLogger(const std::string &name) const
    {
        auto it = _loggers.find(name);
        return it == _loggers.end() ? nullptr : it->second;
    }

    void LogManager::removeLogger(const std::string &name) { _loggers.erase(name); }
} // namespace logging