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

#include "../client.h"
#include "../connectionlistener.h"
#include "../registration.h"
#include "../logsink.h"
#include "../loghandler.h"
using namespace gloox;

#include <stdio.h>
#include <locale.h>
#include <string>
#include <ctime>

#include <cstdio> // [s]print[f]

class RegTest : public RegistrationHandler, ConnectionListener, LogHandler
{
  public:
    RegTest() {}
    virtual ~RegTest() {}

    void start()
    {
// README
// to create an account:
// - create the Client instance by passing it the server only, e.g. j = new Client( "example.net" );
// - in onConnect() (or some similar place) call Registration::fetchRegistrationFields()
// - in RegistrationHandler::handleRegistrationFields() set up an RegistrationFields struct
//   with the desired username/password and call Registration::createAccount()

// to change an account's password:
// - connect to the server as usual
// - in onConnect() or at any later time call Registration::changePassword()

// to delete an account:
// - connect to teh server as usual
// - in onConnect() or at any later time call Registration::removeAccount()

// be sure to read the API documentation for class Registration!

      j = new Client( "jabba.us" );
      j->disableRoster();
      j->registerConnectionListener( this );

      m_reg = new Registration( j );
      m_reg->registerRegistrationHandler( this );

      j->logInstance().registerLogHandler( LogLevelDebug, LogAreaAll, this );

      j->connect();

      delete( m_reg );
      delete( j );
    }

    virtual void onConnect()
    {
//       requesting reg fields
     m_reg->fetchRegistrationFields();

      // changing password
//        m_reg->changePassword( j->username(), "test" );

      // unregistering
//       m_reg->removeAccount();
    }

    virtual void onDisconnect( ConnectionError e ) { printf( "register_test: disconnected: %d\n", e ); }

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

    virtual void handleRegistrationFields( const JID& /*from*/, int fields, std::string instructions )
    {
      printf( "fields: %d\ninstructions: %s\n", fields, instructions.c_str() );
      RegistrationFields vals;
      vals.username = "cotest2";
      vals.password = "test2";
      m_reg->createAccount( fields, vals );
    }

    virtual void handleRegistrationResult( const JID& /*from*/, RegistrationResult result )
    {
      printf( "result: %d\n", result );
      j->disconnect();
    }

    virtual void handleAlreadyRegistered( const JID& /*from*/ )
    {
      printf( "the account already exists.\n" );
    }

    virtual void handleDataForm( const JID& /*from*/, const DataForm& /*form*/ )
    {
      printf( "datForm received\n" );
    }

    virtual void handleOOB( const JID& /*from*/, const OOB& oob )
    {
      printf( "OOB registration requested. %s: %s\n", oob.desc().c_str(), oob.url().c_str() );
    }

    virtual void handleLog( LogLevel level, LogArea area, const std::string& message )
    {
      printf("log: level: %d, area: %d, %s\n", level, area, message.c_str() );
    }

  private:
    Registration *m_reg;
    Client *j;
};

int main( int /*argc*/, char* /*argv*/[] )
{
  RegTest *r = new RegTest();
  r->start();
  delete( r );
  return 0;
}
