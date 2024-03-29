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
#include "../discohandler.h"
#include "../disco.h"
#include "../privatexml.h"
using namespace gloox;

#include <stdio.h>
#include <locale.h>
#include <string>
#include <ctime>

#include <cstdio> // [s]print[f]

class PrivateXMLTest : public PrivateXMLHandler, ConnectionListener
{
  public:
    PrivateXMLTest() {}
    virtual ~PrivateXMLTest() {}

    void start()
    {

      JID jid( "hurkhurk@example.org/gloox" );
      j = new Client( jid, "hurkhurks" );

      j->registerConnectionListener(this );
      j->disco()->setVersion( "privateXMLTest", GLOOX_VERSION );
      j->disco()->setIdentity( "client", "bot" );

      p = new PrivateXML( j );

      j->connect();

      delete( p );
      delete( j );
    }

    virtual void onConnect()
    {
      p->requestXML( "test", "http://camaya.net/jabber/test", this );
    }

    virtual void onDisconnect( ConnectionError /*e*/ ) { printf( "disco_test: disconnected\n" ); }

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

    virtual void handlePrivateXML( const Tag* /*xml*/ )
    {
      printf( "received privatexml...\n" );
      Tag *x = new Tag( "test" );
      x->addAttribute( "xmlns", "http://camaya.net/jabber/test" );
      std::string id = j->getID();
      Tag *b = new Tag( "blah", id );
      x->addChild( b );
      p->storeXML( x, this );
    }

    virtual void handlePrivateXMLResult( const std::string& /*uid*/, PrivateXMLResult /*result*/ )
    {
    }

  private:
    Client *j;
    PrivateXML *p;
};

int main( int /*argc*/, char** /*argv*/ )
{
  PrivateXMLTest *r = new PrivateXMLTest();
  r->start();
  delete( r );
  return 0;
}
