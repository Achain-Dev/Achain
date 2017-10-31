#include <fc/thread/thread.hpp>
#include <fc/thread/mutex.hpp>
#include <fc/thread/scoped_lock.hpp>
#include <fc/thread/future.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/enum_type.hpp>

#include <net/MessageOrientedConnection.hpp>
#include <net/StcpSocket.hpp>
#include <net/Config.hpp>

#include <blockchain/api_extern.hpp>

#ifdef DEFAULT_LOGGER
# undef DEFAULT_LOGGER
#endif
#define DEFAULT_LOGGER "p2p"

#ifndef NDEBUG
# define VERIFY_CORRECT_THREAD() assert(_thread->is_current())
#else
# define VERIFY_CORRECT_THREAD() do {} while (0)
#endif

namespace thinkyoung {
    namespace net {
        namespace detail
        {
            class MessageOrientedConnectionImpl
            {
            private:
                MessageOrientedConnection* _self;
                MessageOrientedConnectionDelegate *_delegate;
                StcpSocket _sock;
                fc::future<void> _read_loop_done;
                uint64_t _bytes_received;
                uint64_t _bytes_sent;

                fc::time_point _connected_time;
                fc::time_point _last_message_received_time;
                fc::time_point _last_message_sent_time;

                bool _send_message_in_progress;

#ifndef NDEBUG
                fc::thread* _thread;
#endif

                void read_loop();
                void start_read_loop();
            public:
                fc::tcp_socket& get_socket();
                void accept();
                void connect_to(const fc::ip::endpoint& remote_endpoint);
                void bind(const fc::ip::endpoint& local_endpoint);

                MessageOrientedConnectionImpl(MessageOrientedConnection* self,
                    MessageOrientedConnectionDelegate* delegate = nullptr);
                ~MessageOrientedConnectionImpl();

                void send_message(const Message& message_to_send);
                void close_connection();
                void destroy_connection();

                uint64_t get_total_bytes_sent() const;
                uint64_t get_total_bytes_received() const;

                fc::time_point get_last_message_sent_time() const;
                fc::time_point get_last_message_received_time() const;
                fc::time_point get_connection_time() const { return _connected_time; }
                fc::sha512 get_shared_secret() const;
            };

            MessageOrientedConnectionImpl::MessageOrientedConnectionImpl(MessageOrientedConnection* self,
                MessageOrientedConnectionDelegate* delegate)
                : _self(self),
                _delegate(delegate),
                _bytes_received(0),
                _bytes_sent(0),
                _send_message_in_progress(false)
#ifndef NDEBUG
                , _thread(&fc::thread::current())
#endif
            {
            }
            MessageOrientedConnectionImpl::~MessageOrientedConnectionImpl()
            {
                VERIFY_CORRECT_THREAD();
                destroy_connection();
            }

            fc::tcp_socket& MessageOrientedConnectionImpl::get_socket()
            {
                VERIFY_CORRECT_THREAD();
                return _sock.get_socket();
            }

            void MessageOrientedConnectionImpl::accept()
            {
                VERIFY_CORRECT_THREAD();
                _sock.accept();
                assert(!_read_loop_done.valid()); // check to be sure we never launch two read loops
                _read_loop_done = fc::async([=](){ read_loop(); }, "message read_loop");
            }

            void MessageOrientedConnectionImpl::connect_to(const fc::ip::endpoint& remote_endpoint)
            {
                VERIFY_CORRECT_THREAD();
                _sock.connect_to(remote_endpoint);
                assert(!_read_loop_done.valid()); // check to be sure we never launch two read loops
                _read_loop_done = fc::async([=](){ read_loop(); }, "message read_loop");
            }

            void MessageOrientedConnectionImpl::bind(const fc::ip::endpoint& local_endpoint)
            {
                VERIFY_CORRECT_THREAD();
                _sock.bind(local_endpoint);
            }


            void MessageOrientedConnectionImpl::read_loop()
            {
                VERIFY_CORRECT_THREAD();
                const int BUFFER_SIZE = 16;
                const int LEFTOVER = BUFFER_SIZE - sizeof(MessageHeader);
                static_assert(BUFFER_SIZE >= sizeof(MessageHeader), "insufficient buffer");

                _connected_time = fc::time_point::now();

                fc::oexception exception_to_rethrow;
                bool call_on_connection_closed = false;

                try
                {
                    Message m;
                    while (true)
                    {
                        char buffer[BUFFER_SIZE];
                        _sock.read(buffer, BUFFER_SIZE);
                        _bytes_received += BUFFER_SIZE;
                        memcpy((char*)&m, buffer, sizeof(MessageHeader));

                        FC_ASSERT(m.size <= MAX_MESSAGE_SIZE, "", ("m.size", m.size)("MAX_MESSAGE_SIZE", MAX_MESSAGE_SIZE));

                        size_t remaining_bytes_with_padding = 16 * ((m.size - LEFTOVER + 15) / 16);
                        m.data.resize(LEFTOVER + remaining_bytes_with_padding); //give extra 16 bytes to allow for padding added in send call
                        std::copy(buffer + sizeof(MessageHeader), buffer + sizeof(buffer), m.data.begin());
                        if (remaining_bytes_with_padding)
                        {
                            _sock.read(&m.data[LEFTOVER], remaining_bytes_with_padding);
                            _bytes_received += remaining_bytes_with_padding;
                        }
                        m.data.resize(m.size); // truncate off the padding bytes

                        _last_message_received_time = fc::time_point::now();

                        if (thinkyoung::client::g_client_quit)
                            return;

                        try
                        {
                            // message handling errors are warnings...
                            _delegate->on_message(_self, m);
                        }
                        /// Dedicated catches needed to distinguish from general fc::exception
                        catch (const fc::canceled_exception& e) { throw e; }
                        catch (const fc::eof_exception& e) { throw e; }
                        catch (const fc::exception& e)
                        {
                            /// Here loop should be continued so exception should be just caught locally.
                            wlog("message transmission failed ${er}", ("er", e.to_detail_string()));
                            throw;
                        }
                    }
                }
                catch (const fc::canceled_exception& e)
                {
                    wlog("caught a canceled_exception in read_loop.  this should mean we're in the process of deleting this object already, so there's no need to notify the delegate: ${e}", ("e", e.to_detail_string()));
                    throw;
                }
                catch (const fc::eof_exception& e)
                {
                    wlog("disconnected ${e}", ("e", e.to_detail_string()));
                    call_on_connection_closed = true;
                }
                catch (const fc::exception& e)
                {
                    elog("disconnected ${er}", ("er", e.to_detail_string()));
                    call_on_connection_closed = true;
                    exception_to_rethrow = fc::unhandled_exception(FC_LOG_MESSAGE(warn, "disconnected: ${e}", ("e", e.to_detail_string())));
                }
                catch (const std::exception& e)
                {
                    elog("disconnected ${er}", ("er", e.what()));
                    call_on_connection_closed = true;
                    exception_to_rethrow = fc::unhandled_exception(FC_LOG_MESSAGE(warn, "disconnected: ${e}", ("e", e.what())));
                }
                catch (...)
                {
                    elog("unexpected exception");
                    call_on_connection_closed = true;
                    exception_to_rethrow = fc::unhandled_exception(FC_LOG_MESSAGE(warn, "disconnected: ${e}", ("e", fc::except_str())));
                }

                if (thinkyoung::client::g_client_quit)
                    return;
                
                if (call_on_connection_closed)
                    _delegate->on_connection_closed(_self);

                if (exception_to_rethrow)
                    throw *exception_to_rethrow;
            }

            void MessageOrientedConnectionImpl::send_message(const Message& message_to_send)
            {
                VERIFY_CORRECT_THREAD();
#if 0 // this gets too verbose
#ifndef NDEBUG
                fc::optional<fc::ip::endpoint> remote_endpoint;
                if (_sock.get_socket().is_open())
                    remote_endpoint = _sock.get_socket().remote_endpoint();
                struct scope_logger {
                    const fc::optional<fc::ip::endpoint>& endpoint;
                    scope_logger(const fc::optional<fc::ip::endpoint>& endpoint) : endpoint(endpoint) { dlog("entering message_oriented_connection::send_message() for peer ${endpoint}", ("endpoint", endpoint)); }
                    ~scope_logger() { dlog("leaving message_oriented_connection::send_message() for peer ${endpoint}", ("endpoint", endpoint)); }
                } send_message_scope_logger(remote_endpoint);
#endif
#endif
                struct verify_no_send_in_progress {
                    bool& var;
                    verify_no_send_in_progress(bool& var) : var(var)
                    {
                        if (var)
                            elog("Error: two tasks are calling message_oriented_connection::send_message() at the same time");
                        assert(!var);
                        var = true;
                    }
                    ~verify_no_send_in_progress() { var = false; }
                } _verify_no_send_in_progress(_send_message_in_progress);

                try
                {
                    size_t size_of_message_and_header = sizeof(MessageHeader) + message_to_send.size;
                    //pad the message we send to a multiple of 16 bytes
                    size_t size_with_padding = 16 * ((size_of_message_and_header + 15) / 16);
                    std::unique_ptr<char[]> padded_message(new char[size_with_padding]);
                    memcpy(padded_message.get(), (char*)&message_to_send, sizeof(MessageHeader));
                    memcpy(padded_message.get() + sizeof(MessageHeader), message_to_send.data.data(), message_to_send.size);
                    _sock.write(padded_message.get(), size_with_padding);
                    _sock.flush();
                    _bytes_sent += size_with_padding;
                    _last_message_sent_time = fc::time_point::now();
                } FC_RETHROW_EXCEPTIONS(warn, "unable to send message");
            }

            void MessageOrientedConnectionImpl::close_connection()
            {
                VERIFY_CORRECT_THREAD();
                _sock.close();
            }

            void MessageOrientedConnectionImpl::destroy_connection()
            {
                VERIFY_CORRECT_THREAD();

                fc::optional<fc::ip::endpoint> remote_endpoint;
                if (_sock.get_socket().is_open())
                    remote_endpoint = _sock.get_socket().remote_endpoint();
                ilog("in destroy_connection() for ${endpoint}", ("endpoint", remote_endpoint));

                if (_send_message_in_progress)
                    elog("Error: message_oriented_connection is being destroyed while a send_message is in progress.  "
                    "The task calling send_message() should have been canceled already");
                assert(!_send_message_in_progress);

                try
                {
                    _read_loop_done.cancel_and_wait(__FUNCTION__);
                }
                catch (const fc::exception& e)
                {
                    wlog("Exception thrown while canceling message_oriented_connection's read_loop, ignoring: ${e}", ("e", e));
                }
                catch (...)
                {
                    wlog("Exception thrown while canceling message_oriented_connection's read_loop, ignoring");
                }
            }

            uint64_t MessageOrientedConnectionImpl::get_total_bytes_sent() const
            {
                VERIFY_CORRECT_THREAD();
                return _bytes_sent;
            }

            uint64_t MessageOrientedConnectionImpl::get_total_bytes_received() const
            {
                VERIFY_CORRECT_THREAD();
                return _bytes_received;
            }

            fc::time_point MessageOrientedConnectionImpl::get_last_message_sent_time() const
            {
                VERIFY_CORRECT_THREAD();
                return _last_message_sent_time;
            }

            fc::time_point MessageOrientedConnectionImpl::get_last_message_received_time() const
            {
                VERIFY_CORRECT_THREAD();
                return _last_message_received_time;
            }

            fc::sha512 MessageOrientedConnectionImpl::get_shared_secret() const
            {
                VERIFY_CORRECT_THREAD();
                return _sock.get_shared_secret();
            }

        } // end namespace thinkyoung::net::detail


        MessageOrientedConnection::MessageOrientedConnection(MessageOrientedConnectionDelegate* delegate) :
            my(new detail::MessageOrientedConnectionImpl(this, delegate))
        {
        }

        MessageOrientedConnection::~MessageOrientedConnection()
        {
        }

        fc::tcp_socket& MessageOrientedConnection::get_socket()
        {
            return my->get_socket();
        }

        void MessageOrientedConnection::accept()
        {
            my->accept();
        }

        void MessageOrientedConnection::connect_to(const fc::ip::endpoint& remote_endpoint)
        {
            my->connect_to(remote_endpoint);
        }

        void MessageOrientedConnection::bind(const fc::ip::endpoint& local_endpoint)
        {
            my->bind(local_endpoint);
        }

        void MessageOrientedConnection::send_message(const Message& message_to_send)
        {
            my->send_message(message_to_send);
        }

        void MessageOrientedConnection::close_connection()
        {
            my->close_connection();
        }

        void MessageOrientedConnection::destroy_connection()
        {
            my->destroy_connection();
        }

        uint64_t MessageOrientedConnection::get_total_bytes_sent() const
        {
            return my->get_total_bytes_sent();
        }

        uint64_t MessageOrientedConnection::get_total_bytes_received() const
        {
            return my->get_total_bytes_received();
        }

        fc::time_point MessageOrientedConnection::get_last_message_sent_time() const
        {
            return my->get_last_message_sent_time();
        }

        fc::time_point MessageOrientedConnection::get_last_message_received_time() const
        {
            return my->get_last_message_received_time();
        }
        fc::time_point MessageOrientedConnection::get_connection_time() const
        {
            return my->get_connection_time();
        }
        fc::sha512 MessageOrientedConnection::get_shared_secret() const
        {
            return my->get_shared_secret();
        }

    }
} // end namespace thinkyoung::net
