/*
 *  Copyright (c) 2004-2023 by Jakob Schröter <js@camaya.net>
 *  This file is part of the gloox library. http://camaya.net/gloox
 *
 *  This software is distributed under a license. The full license
 *  agreement can be found in the file LICENSE in this distribution.
 *  This software may not be copied, modified, sold or distributed
 *  other than expressed in the named license agreement.
 *
 *  This software is distributed without any warranty.
 */

#define CLIENT_TEST
#define CLIENTBASE_TEST
#include "../client.h"
#include "../messagesessionhandler.h"
#include "../messageeventhandler.h"
#include "../messageeventfilter.h"
#include "../chatstatehandler.h"
#include "../chatstatefilter.h"
#include "../connectionlistener.h"
#include "../disco.h"
#include "../message.h"
#include "../gloox.h"
#include "../lastactivity.h"
#include "../loghandler.h"
#include "../logsink.h"
#include "../connectiontcpclient.h"
#include "../connectionsocks5proxy.h"
#include "../connectionhttpproxy.h"
#include "../messagehandler.h"
using namespace gloox;

#ifndef _WIN32
# include <unistd.h>
#endif

#include <stdio.h>
#include <string>

#include <cstdio> // [s]print[f]

#if defined( WIN32 ) || defined( _WIN32 )
# include <windows.h>
#endif

class MessageTest : public MessageSessionHandler, ConnectionListener, LogHandler,
                    MessageEventHandler, MessageHandler, ChatStateHandler
{
  public:
    MessageTest() : m_session( 0 ), m_reconnect( false ) {}

    virtual ~MessageTest() {}

    void start()
    {

      JID jid( "admin@jabba.us/gloox" );
      j = new Client( jid, "test" );
      j->registerConnectionListener( this );
      j->registerMessageSessionHandler( this, 0 );
      j->disco()->setVersion( "reconnectTest", GLOOX_VERSION, "Linux" );
      j->disco()->setIdentity( "client", "bot" );
      j->disco()->addFeature( XMLNS_CHAT_STATES );
//       j->setTls( TLSDisabled );
      j->setCompression( false );
      j->setStreamManagement( true, true );
      StringList ca;
      ca.push_back( "/path/to/cacert.crt" );
      j->setCACerts( ca );

      j->logInstance().registerLogHandler( LogLevelDebug, LogAreaAll, this );

//
// this code connects to a jabber server through a SOCKS5 proxy
//
//       ConnectionSOCKS5Proxy* conn = new ConnectionSOCKS5Proxy( j,
//                                   new ConnectionTCP( j->logInstance(),
//                                                      "sockshost", 1080 ),
//                                   j->logInstance(), "example.net" );
//       conn->setProxyAuth( "socksuser", "sockspwd" );
//       j->setConnectionImpl( conn );

//
// this code connects to a jabber server through a HTTP proxy through a SOCKS5 proxy
//
//       ConnectionTCP* conn0 = new ConnectionTCP( j->logInstance(), "old", 1080 );
//       ConnectionSOCKS5Proxy* conn1 = new ConnectionSOCKS5Proxy( conn0, j->logInstance(), "old", 8080 );
//       conn1->setProxyAuth( "socksuser", "sockspwd" );
//       ConnectionHTTPProxy* conn2 = new ConnectionHTTPProxy( j, conn1, j->logInstance(), "jabber.cc" );
//       conn2->setProxyAuth( "httpuser", "httppwd" );
//       j->setConnectionImpl( conn2 );


      ConnectionError ce = ConnNoError;
      if( j->connect( false ) )
      {
        while( ce == ConnNoError )
        {
          ce = j->recv();
        }
        printf( "ce: %d\n", ce );
      }

      m_reconnect = true;
      ce = ConnNoError;
      if( j->connect( false ) )
      {
        while( ce == ConnNoError )
        {
          ce = j->recv();
        }
        printf( "ce: %d\n", ce );
      }

      delete( j );
    }

    virtual void onConnect()
    {
      printf( "connected!!!\n" );
    }

    virtual void onDisconnect( ConnectionError e )
    {
      printf( "message_test: disconnected: %d\n", e );
      if( e == ConnAuthenticationFailed )
        printf( "auth failed. reason: %d\n", j->authError() );
    }

    virtual bool onTLSConnect( const CertInfo& info )
    {
      time_t from( info.date_from );
      time_t to( info.date_to );

      printf( "status: %d\nissuer: %s\npeer: %s\nprotocol: %s\nmac: %s\ncipher: %s\ncompression: %s\n",
              info.status, info.issuer.c_str(), info.server.c_str(),
              info.protocol.c_str(), info.mac.c_str(), info.cipher.c_str(),
              info.compression.c_str() );
      printf( "from: %s", ctime( &from ) );
      printf( "to:   %s", ctime( &to ) );
      return true;
    }

    virtual void handleMessage( const Message& msg, MessageSession * /*session*/ )
    {
      printf( "type: %d, subject: %s, message: %s, thread id: %s\n", msg.subtype(),
              msg.subject().c_str(), msg.body().c_str(), msg.thread().c_str() );


      if( msg.body().substr( 0, 3 ) == "ack" ) // using substr() to work around some stupid clients
        j->ackStreamManagement();
      else if( msg.body().substr( 0, 3 ) == "req" )
        j->reqStreamManagement();
      else if( msg.body().substr( 0, 4 ) == "quit" )
        j->disconnect();
      else
      {
        std::string re = "You said:\n> " + msg.body() + "\nI like that statement.";
        m_session->send( re, gloox::EmptyString );
      }
    }

    virtual void handleMessageEvent( const JID& from, MessageEventType event )
    {
      printf( "received event: %d from: %s\n", event, from.full().c_str() );
    }

    virtual void handleChatState( const JID& from, ChatStateType state )
    {
      printf( "received state: %d from: %s\n", state, from.full().c_str() );
    }

    virtual void handleMessageSession( MessageSession *session )
    {
      printf( "got new session\n");
      // this example can handle only one session. so we get rid of the old session
      j->disposeMessageSession( m_session );
      m_session = session;
      m_session->registerMessageHandler( this );
    }

    virtual void handleLog( LogLevel level, LogArea area, const std::string& message )
    {
      printf("log: level: %d, area: %d, %s\n", level, area, message.c_str() );

      if( message.substr( 0, 10 ) == "<stream:er" )
        printf( "something's foul\n" );

      if( !m_reconnect && j->m_smHandled > 10 )
        j->disconnect( ConnTlsFailed ); // fake disconnect reason so that no </stream:stream> is sent
    }

  private:
    Client *j;
    MessageSession *m_session;
    bool m_reconnect;
};

int main( int /*argc*/, char** /*argv*/ )
{
  MessageTest *r = new MessageTest();
  r->start();
  delete( r );
  return 0;
}
