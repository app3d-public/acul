#ifndef APP_CORE_LOG_H
#define APP_CORE_LOG_H

#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include "std/array.hpp"
#include "std/string.hpp"
#include "task.hpp"

enum class LogLevel
{
    Fatal,
    Error,
    Warn,
    Info,
    Debug,
    Trace
};

namespace logging
{
    struct LogMessageData
    {
        LogLevel level;
        const std::string &message;
        std::thread::id thread_id;
    };

    class TokenHandler
    {
    public:
        virtual ~TokenHandler() = default;
        virtual std::string handle(const LogMessageData &context) const = 0;
    };

    class TextTokenHandler : public TokenHandler
    {
    public:
        explicit TextTokenHandler(const std::string &text) : _text(text) {}

        std::string handle(const LogMessageData &context) const override { return _text; }

    private:
        std::string _text;
    };

    class TimeTokenHandler : public TokenHandler
    {
    public:
        std::string handle(const LogMessageData &context) const override;
    };

    class ThreadIdTokenHandler : public TokenHandler
    {
    public:
        std::string handle(const LogMessageData &context) const override;
    };

    class LevelNameTokenHandler : public TokenHandler
    {
    public:
        std::string handle(const LogMessageData &context) const override;
    };

    class MessageTokenHandler : public TokenHandler
    {
    public:
        std::string handle(const LogMessageData &context) const override { return context.message; }
    };

    namespace colors
    {
        constexpr std::string_view red = "\x1b[31m";
        constexpr std::string_view green = "\x1b[32m";
        constexpr std::string_view yellow = "\x1b[33m";
        constexpr std::string_view blue = "\x1b[34m";
        constexpr std::string_view magenta = "\x1b[35m";
        constexpr std::string_view cyan = "\x1b[36m";
        constexpr std::string_view reset = "\x1b[0m";
    }; // namespace colors

    class ColorizeTokenHandler : public TokenHandler
    {
    public:
        std::string handle(const LogMessageData &context) const override;
    };

    class DecolorizeTokenHandler : public TokenHandler
    {
    public:
        std::string handle(const LogMessageData &context) const override { return std::string(colors::reset); }
    };

    using TokenHandlerList = Array<std::shared_ptr<TokenHandler>>;

    template <typename... Args>
    std::string formatMessage(const std::string &message, Args &&...args)
    {
        if constexpr (sizeof...(Args) > 0)
            return format(message, std::forward<Args>(args)...);
        else
            return message;
    }

    class Logger
    {
    public:
        Logger(const std::string &name) : _name(name), _tokens(std::make_shared<TokenHandlerList>()) {}

        virtual ~Logger() = default;

        void setPattern(const std::string &pattern);

        std::string name() const { return _name; }

        LogLevel level() const { return _level; }

        void level(LogLevel level) { _level = level; }

        virtual std::ostream &stream() = 0;

        virtual void write(const std::string &message) = 0;

        std::string getParsedStr(const LogMessageData &context) const;

    private:
        std::string _name;
        LogLevel _level{LogLevel::Error};
        std::shared_ptr<TokenHandlerList> _tokens;
    };

    class FileLogger : public Logger
    {
    public:
        FileLogger(const std::string &name, const std::string &path, std::ios_base::openmode flags);

        ~FileLogger();

        std::ostream &stream() override { return _fs; }

        void write(const std::string &message) override;

    private:
        std::string _filepath;
        std::ofstream _fs;
    };

    class ConsoleLogger : public Logger
    {
    public:
        explicit ConsoleLogger(const std::string &name) : Logger(name) {}

        std::ostream &stream() override { return std::cout; }

        void write(const std::string &message) override { std::cout << message << std::endl; }
    };

    /// \brief Represents a task manager specifically for logging tasks.
    ///
    /// This class reserves only one worker thread for logging tasks.
    class LogTaskManager : public TaskQueue
    {
    public:
        /// \brief Get the singleton instance of the LogTaskManager.
        ///
        /// \return The singleton instance of the LogTaskManager.
        static LogTaskManager &getSingleton();

        /// \brief Perform the task processing in the worker thread.
        void taskWorkerThread() override;

        /// \brief Stop the worker threads for task processing.
        void stopWorkerThreads();

    private:
        bool _running{true};
        std::thread _taskThread;

        LogTaskManager();
        ~LogTaskManager();
        LogTaskManager(const LogTaskManager &) = delete;
        LogTaskManager &operator=(const LogTaskManager &) = delete;
    };

    /**
     * @class LogManager
     * @brief Manages loggers for the application.
     *
     * The LogManager class provides functionality to add, get, and remove loggers. It also allows logging messages with
     * different log levels. The LogManager is a singleton class, meaning only one instance of it can exist in the
     * application.
     */
    class LogManager
    {
    public:
        /**
         * @brief Gets the singleton instance of the LogManager.
         * @return The singleton instance of the LogManager.
         */
        static LogManager &getSingleton();

        /**
         * @brief Adds a logger with the specified name and file path.
         * @param name The name of the logger.
         * @param path The file path where the log file should be created.
         * @param flags The flags to open the log file with.
         * @return A shared pointer to the added Logger object.
         */
        template <typename T, typename... Args>
        std::shared_ptr<T> addLogger(const std::string &name, Args &&...args)
        {
            std::shared_ptr<T> logger = std::make_shared<T>(name, std::forward<Args>(args)...);
            _loggers[name] = logger;
            return logger;
        }

        /**
         * @brief Gets the logger with the specified name.
         * @param name The name of the logger to retrieve.
         * @return A shared pointer to the Logger object, or nullptr if the logger was not found.
         */
        std::shared_ptr<Logger> getLogger(const std::string &name) const;

        /**
         * @brief Removes the logger with the specified name.
         * @param name The name of the logger to remove.
         */
        void removeLogger(const std::string &name);

        template <typename... Args>
        void log(const std::shared_ptr<Logger> &logger, LogLevel level, const std::string &message, Args &&...args)
        {
            if (!logger)
                return;
            static LogTaskManager &logManager = LogTaskManager::getSingleton();
            logManager.addTask([logger, level, message, ... args{std::forward<Args>(args)}]() {
                if (level > logger->level())
                    return;
                std::thread::id thread_id = std::this_thread::get_id();
                std::string parsed_str = logger->getParsedStr({level, message, thread_id});
                logger->write(formatMessage(parsed_str, args...));
            });
        }

        template <typename... Args>
        void log(LogLevel level, const std::string &message, Args &&...args)
        {
            log(_defaultLogger, level, message, std::forward<Args>(args)...);
        }

        /**
         * @brief Logs a message with the specified log level for the specified logger and waits for the log to
         * complete.
         * @param logger The name of the logger to log the message to.
         * @param level The log level of the message.
         * @param message The message to log.
         * @param args The additional arguments to format the log message.
         * @return A future object that can be used to wait for the log to complete.
         */
        template <typename... Args>
        std::future<void> logAndWait(const std::string &logger, LogLevel level, const std::string &message,
                                     Args &&...args)
        {
            auto logger_it = _loggers.find(logger);
            if (logger_it == _loggers.end())
                return std::future<void>();
            static LogTaskManager &logManager = LogTaskManager::getSingleton();

            auto prom = std::make_shared<std::promise<void>>();
            auto result = prom->get_future();

            logManager.addTask([logger_it, level, message, prom, ... args{std::forward<Args>(args)}]() mutable {
                if (level > logger_it->second->level())
                {
                    prom->set_value();
                    return;
                }
                std::thread::id thread_id = std::this_thread::get_id();
                std::string parsed_str = logger_it->second->getParsedStr({level, message, thread_id});
                logger_it->second->write(formatMessage(parsed_str, args...));
                prom->set_value();
            });

            return result;
        }

        std::shared_ptr<Logger> defaultLogger() const { return _defaultLogger; }
        void defaultLogger(std::shared_ptr<Logger> logger) { _defaultLogger = logger; }

    private:
        LogManager() = default;
        LogManager &operator=(const LogManager &) = delete;

        std::unordered_map<std::string, std::shared_ptr<Logger>> _loggers;
        std::shared_ptr<Logger> _defaultLogger;
    };

    inline std::shared_ptr<Logger> getLogger(const std::string &name)
    {
        return LogManager::getSingleton().getLogger(name);
    }
} // namespace logging

#define DEFINE_LOG_FUNCTION(level)                                                                              \
    template <typename... Args>                                                                                 \
    void log##level(const std::shared_ptr<logging::Logger> &logger, const std::string &message, Args &&...args) \
    {                                                                                                           \
        static logging::LogManager &manager = logging::LogManager::getSingleton();                              \
        manager.log(logger, LogLevel::level, message, std::forward<Args>(args)...);                             \
    }                                                                                                           \
    template <typename... Args>                                                                                 \
    void log##level(const std::string &message, Args &&...args)                                                 \
    {                                                                                                           \
        static logging::LogManager &manager = logging::LogManager::getSingleton();                              \
        manager.log(LogLevel::level, message, std::forward<Args>(args)...);                                     \
    }

DEFINE_LOG_FUNCTION(Info)
DEFINE_LOG_FUNCTION(Debug)
DEFINE_LOG_FUNCTION(Warn)
DEFINE_LOG_FUNCTION(Error)
DEFINE_LOG_FUNCTION(Fatal)
DEFINE_LOG_FUNCTION(Trace)

#undef DEFINE_LOG_FUNCTION

#endif