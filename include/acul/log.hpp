#ifndef APP_ACUL_LOG_H
#define APP_ACUL_LOG_H

#include <fstream>
#include <iostream>
#include <oneapi/tbb/concurrent_queue.h>
#include "hash/hashmap.hpp"
#include "io/path.hpp"
#include "string/sstream.hpp"
#include "task.hpp"

namespace acul
{
    namespace log
    {
        enum class level
        {
            Fatal,
            Error,
            Warn,
            Info,
            Debug,
            Trace
        };

        class token_handler_base
        {
        public:
            virtual ~token_handler_base() = default;
            virtual void handle(level level, const char *message, stringstream &ss) const = 0;
        };

        using token_handler_list = vector<shared_ptr<token_handler_base>>;

        class text_handler final : public token_handler_base
        {
        public:
            explicit text_handler(const string_view &text) : _text(text) {}

            void handle(level level, const char *message, stringstream &ss) const override { ss << _text; }

        private:
            const string _text;
        };

        class time_handler final : public token_handler_base
        {
        public:
            void handle(level level, const char *message, stringstream &ss) const override;
        };

        class thread_id_handler final : public token_handler_base
        {
        public:
            void handle(level level, const char *message, stringstream &ss) const override
            {

                ss << task::get_thread_id();
            }
        };

        class level_name_handler final : public token_handler_base
        {
        public:
            void handle(level level, const char *message, stringstream &ss) const override;
        };

        class message_handler final : public token_handler_base
        {
        public:
            void handle(level level, const char *message, stringstream &ss) const override { ss << message; }
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

        class color_handler final : public token_handler_base
        {
        public:
            void handle(level level, const char *message, stringstream &ss) const override;
        };

        class decolor_handler final : public token_handler_base
        {
        public:
            void handle(level level, const char *message, stringstream &ss) const override { ss << colors::reset; }
        };

        class APPLIB_API logger_base
        {
        public:
            logger_base(const string &name) : _name(name), _tokens(make_shared<token_handler_list>()) {}

            virtual ~logger_base() = default;

            void set_pattern(const string &pattern);

            string name() const { return _name; }

            virtual std::ostream &stream() = 0;

            virtual void write(const string &message) = 0;

            void parse_tokens(level level, const char *message, stringstream &ss)
            {
                for (auto &token : *_tokens) token->handle(level, message, ss);
            }

        private:
            string _name;
            shared_ptr<token_handler_list> _tokens;
        };

        class APPLIB_API file_logger final : public logger_base
        {
        public:
            file_logger(const string &name, const io::path &path, std::ios_base::openmode flags)
                : logger_base(name), _path(path)
            {
                _fs.open(path.str().c_str(), flags);
            }

            ~file_logger()
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

        class console_logger final : public logger_base
        {
        public:
            explicit console_logger(const string &name) : logger_base(name) {}

            std::ostream &stream() override { return std::cout; }

            virtual void write(const string &message) override { std::cout << message.c_str(); }
        };

        class log_service;
        extern APPLIB_API log_service *g_log_service;

        /**
         * @class The Log Service
         * @brief Manages loggers for the application.
         *
         * Provides functionality to add, get, and remove loggers. It also allows logging messages with
         * different log levels.
         */
        class APPLIB_API log_service final : public task::service_base
        {
        public:
            logger_base *default_logger;
            level level;

            log_service() : default_logger(nullptr), level(level::Error) { g_log_service = this; }
            ~log_service();

            /**
             * @brief Adds a logger with the specified name and file path.
             * @param name The name of the logger.
             * @param path The file path where the log file should be created.
             * @param flags The flags to open the log file with.
             * @return A pointer to the added Logger object.
             */
            template <typename T, typename... Args>
            T *add_logger(const string &name, Args &&...args)
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
            logger_base *get_logger(const string &name) const
            {
                auto it = _loggers.find(name);
                return it == _loggers.end() ? nullptr : it->second;
            }

            /**
             * @brief Removes the logger with the specified name.
             * @param name The name of the logger to remove.
             */
            void remove_logger(const string &name)
            {
                auto it = _loggers.find(name);
                if (it == _loggers.end()) return;
                acul::release(it->second);
                _loggers.erase(it);
            }

            __attribute__((format(printf, 4, 5))) APPLIB_API void log(logger_base *logger, enum level level,
                                                                      const char *message, ...);

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
            hashmap<string, logger_base *> _loggers;
            oneapi::tbb::concurrent_queue<std::pair<logger_base *, string>> _queue;
            std::atomic<int> _count{0};
        };

        inline logger_base *get_logger(const string &name) { return g_log_service->get_logger(name); }

        inline logger_base *get_default_logger() { return g_log_service->default_logger; }
    } // namespace log
} // namespace acul

#ifdef ACUL_LOG_ENABLE
    #define LOG_INFO(...) \
        acul::log::g_log_service->log(acul::log::get_default_logger(), acul::log::level::Info, __VA_ARGS__)
    #define LOG_DEBUG(...) \
        acul::log::g_log_service->log(acul::log::get_default_logger(), acul::log::level::Debug, __VA_ARGS__)
    #define LOG_TRACE(...) \
        acul::log::g_log_service->log(acul::log::get_default_logger(), acul::log::level::Trace, __VA_ARGS__)
    #define LOG_WARN(...) \
        acul::log::g_log_service->log(acul::log::get_default_logger(), acul::log::level::Warn, __VA_ARGS__)
    #define LOG_ERROR(...) \
        acul::log::g_log_service->log(acul::log::get_default_logger(), acul::log::level::Error, __VA_ARGS__)
    #define LOG_FATAL(...) \
        acul::log::g_log_service->log(acul::log::get_default_logger(), acul::log::level::Fatal, __VA_ARGS__)
#else
    #define LOG_INFO(...)  ((void)0)
    #define LOG_DEBUG(...) ((void)0)
    #define LOG_TRACE(...) ((void)0)
    #define LOG_WARN(...)  ((void)0)
    #define LOG_ERROR(...) ((void)0)
    #define LOG_FATAL(...) ((void)0)
#endif
#endif