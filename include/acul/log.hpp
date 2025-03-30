#ifndef APP_ACUL_LOG_H
#define APP_ACUL_LOG_H

#include <iostream>
#include "io/path.hpp"
#include "string/sstream.hpp"
#include "task.hpp"

namespace acul
{
    namespace log
    {
        enum class Level
        {
            Fatal,
            Error,
            Warn,
            Info,
            Debug,
            Trace
        };

        class TokenHandler
        {
        public:
            virtual ~TokenHandler() = default;
            virtual void handle(Level level, const char *message, stringstream &ss) const = 0;
        };

        using TokenHandlerList = acul::vector<acul::shared_ptr<TokenHandler>>;

        class TextTokenHandler final : public TokenHandler
        {
        public:
            explicit TextTokenHandler(const string_view &text) : _text(text) {}

            void handle(Level level, const char *message, stringstream &ss) const override { ss << _text; }

        private:
            const string _text;
        };

        class TimeTokenHandler final : public TokenHandler
        {
        public:
            void handle(Level level, const char *message, stringstream &ss) const override;
        };

        class ThreadIdTokenHandler final : public TokenHandler
        {
        public:
            void handle(Level level, const char *message, stringstream &ss) const override
            {

                ss << task::get_thread_id();
            }
        };

        class LevelNameTokenHandler final : public TokenHandler
        {
        public:
            void handle(Level level, const char *message, stringstream &ss) const override;
        };

        class MessageTokenHandler final : public TokenHandler
        {
        public:
            void handle(Level level, const char *message, stringstream &ss) const override { ss << message; }
        };

        namespace colors
        {
            constexpr string_view red = "\x1b[31m";
            constexpr string_view green = "\x1b[32m";
            constexpr string_view yellow = "\x1b[33m";
            constexpr string_view blue = "\x1b[34m";
            constexpr string_view magenta = "\x1b[35m";
            constexpr string_view cyan = "\x1b[36m";
            constexpr string_view reset = "\x1b[0m";
        }; // namespace colors

        class ColorizeTokenHandler final : public TokenHandler
        {
        public:
            void handle(Level level, const char *message, stringstream &ss) const override;
        };

        class DecolorizeTokenHandler final : public TokenHandler
        {
        public:
            void handle(Level level, const char *message, stringstream &ss) const override { ss << colors::reset; }
        };

        class APPLIB_API Logger
        {
        public:
            Logger(const string &name) : _name(name), _tokens(acul::make_shared<TokenHandlerList>()) {}

            virtual ~Logger() = default;

            void setPattern(const string &pattern);

            string name() const { return _name; }

            virtual std::ostream &stream() = 0;

            virtual void write(const string &message) = 0;

            void parseTokens(Level level, const char *message, stringstream &ss)
            {
                for (auto &token : *_tokens) token->handle(level, message, ss);
            }

        private:
            string _name;
            acul::shared_ptr<TokenHandlerList> _tokens;
        };

        class APPLIB_API FileLogger final : public Logger
        {
        public:
            FileLogger(const string &name, const io::path &path, std::ios_base::openmode flags)
                : Logger(name), _path(path)
            {
                _fs.open(path.str().c_str(), flags);
            }

            ~FileLogger()
            {
                if (_fs.is_open()) _fs.close();
            }

            std::ostream &stream() override { return _fs; }

            virtual void write(const string &message) override
            {
                if (_fs.is_open()) _fs << message.c_str();
            }

        private:
            io::path _path;
            std::ofstream _fs;
        };

        class ConsoleLogger final : public Logger
        {
        public:
            explicit ConsoleLogger(const string &name) : Logger(name) {}

            std::ostream &stream() override { return std::cout; }

            virtual void write(const string &message) override { std::cout << message.c_str(); }
        };

        /**
         * @class The Log Service
         * @brief Manages loggers for the application.
         *
         * Provides functionality to add, get, and remove loggers. It also allows logging messages with
         * different log levels.
         */
        class APPLIB_API LogService final : public task::IService
        {
        public:
            ~LogService();

            /**
             * @brief Adds a logger with the specified name and file path.
             * @param name The name of the logger.
             * @param path The file path where the log file should be created.
             * @param flags The flags to open the log file with.
             * @return A pointer to the added Logger object.
             */
            template <typename T, typename... Args>
            T *addLogger(const string &name, Args &&...args)
            {
                auto *logger = acul::alloc<T>(name, std::forward<Args>(args)...);
                _loggers[name] = logger;
                return logger;
            }

            /**
             * @brief Gets the logger with the specified name.
             * @param name The name of the logger to retrieve.
             * @return A pointer to the Logger object, or nullptr if the logger was not found.
             */
            Logger *getLogger(const string &name) const
            {
                auto it = _loggers.find(name);
                return it == _loggers.end() ? nullptr : it->second;
            }

            /**
             * @brief Removes the logger with the specified name.
             * @param name The name of the logger to remove.
             */
            void removeLogger(const string &name)
            {
                auto it = _loggers.find(name);
                if (it == _loggers.end()) return;
                acul::release(it->second);
                _loggers.erase(it);
            }

            __attribute__((format(printf, 4, 5))) APPLIB_API void log(Logger *logger, Level level, const char *message,
                                                                      ...);

            Level level() const { return _level; }

            void level(Level level) { _level = level; }

            virtual std::chrono::steady_clock::time_point dispatch() override;

            virtual void await(bool force = false) override
            {
                if (force)
                {
                    _queue.clear();
                    return;
                }
                while (_count.load(std::memory_order_relaxed) > 0) std::this_thread::yield();
            }

        private:
            Level _level{Level::Error};
            acul::hashmap<string, Logger *> _loggers;
            oneapi::tbb::concurrent_queue<std::pair<Logger *, string>> _queue;
            std::atomic<int> _count{0};
        };

        extern APPLIB_API LogService *g_LogService;
        extern APPLIB_API Logger *g_DefaultLogger;

        inline Logger *getLogger(const string &name) { return g_LogService->getLogger(name); }
    } // namespace log
} // namespace acul

#ifdef ACUL_LOG_ENABLE
    #define logInfo(...)  acul::log::g_LogService->log(acul::log::g_DefaultLogger, acul::log::Level::Info, __VA_ARGS__)
    #define logDebug(...) acul::log::g_LogService->log(acul::log::g_DefaultLogger, acul::log::Level::Debug, __VA_ARGS__)
    #define logTrace(...) acul::log::g_LogService->log(acul::log::g_DefaultLogger, acul::log::Level::Trace, __VA_ARGS__)
    #define logWarn(...)  acul::log::g_LogService->log(acul::log::g_DefaultLogger, acul::log::Level::Warn, __VA_ARGS__)
    #define logError(...) acul::log::g_LogService->log(acul::log::g_DefaultLogger, acul::log::Level::Error, __VA_ARGS__)
    #define logFatal(...) acul::log::g_LogService->log(acul::log::g_DefaultLogger, acul::log::Level::Fatal, __VA_ARGS__)
#else
    #define logInfo(...)  ((void)0)
    #define logDebug(...) ((void)0)
    #define logTrace(...) ((void)0)
    #define logWarn(...)  ((void)0)
    #define logError(...) ((void)0)
    #define logFatal(...) ((void)0)
#endif
#endif