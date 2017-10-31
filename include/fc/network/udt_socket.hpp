#pragma once
#include <fc/utility.hpp>
#include <fc/fwd.hpp>
#include <fc/io/iostream.hpp>
#include <fc/time.hpp>
#include <fc/noncopyable.hpp>

namespace fc {
   namespace ip { class endpoint; }

   class udt_socket : public virtual iostream, public noncopyable
   {
     public:
       udt_socket();
       ~udt_socket();

       void bind( const fc::ip::endpoint& local_endpoint );
       void connect_to( const fc::ip::endpoint& remote_endpoint );

       fc::ip::endpoint remote_endpoint() const;
       fc::ip::endpoint local_endpoint() const;

       using istream::get;
       void get( char& c )
       {
           read( &c, 1 );
       }


       /// istream interface
       /// @{
       virtual size_t   readsome( char* buffer, size_t max );
       virtual size_t   readsome( const std::shared_ptr<char>& buf, size_t len, size_t offset );
       virtual bool     eof()const;
       /// @}

       /// ostream interface
       /// @{
       virtual size_t   writesome( const char* buffer, size_t len );
       virtual size_t   writesome( const std::shared_ptr<const char>& buf, size_t len, size_t offset );
       virtual void     flush();
       virtual void     close();
       /// @}

       void open();
       bool is_open()const;

     private:
       friend class udt_server;
       int  _udt_socket_id;
   };
   typedef std::shared_ptr<udt_socket> udt_socket_ptr;

   class udt_server : public noncopyable
   {
      public:
        udt_server();
        ~udt_server();

        void             close();
        void             accept( udt_socket& s );

        void             listen( const fc::ip::endpoint& ep );
        fc::ip::endpoint local_endpoint() const;

      private:
        int _udt_socket_id;
   };

} // fc
