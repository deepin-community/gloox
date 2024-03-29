/*
  Copyright (c) 2005-2023 by Jakob Schröter <js@camaya.net>
  This file is part of the gloox library. http://camaya.net/gloox

  This software is distributed under a license. The full license
  agreement can be found in the file LICENSE in this distribution.
  This software may not be copied, modified, sold or distributed
  other than expressed in the named license agreement.

  This software is distributed without any warranty.
*/



#include "tlsgnutlsbase.h"

#ifdef HAVE_GNUTLS

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <cstdio>

namespace gloox
{

  GnuTLSBase::GnuTLSBase( TLSHandler* th, const std::string& server )
    : TLSBase( th, server ), m_session( new gnutls_session_t ), m_buf( 0 ), m_bufsize( 17000 )
  {
    m_buf = static_cast<char*>( calloc( m_bufsize + 1, sizeof( char ) ) );
  }

  GnuTLSBase::~GnuTLSBase()
  {
    free( m_buf );
    m_buf = 0;
    cleanup();
    delete m_session;
// FIXME: It segfaults if more then one account uses
// encryption at same time, so we comment it for now.
// Do we need to deinit at all?
//     gnutls_global_deinit();
  }

  bool GnuTLSBase::encrypt( const std::string& data )
  {
    if( !m_secure )
    {
      handshake();
      return true;
    }

    ssize_t ret = 0;
    std::string::size_type sum = 0;
    do
    {
      ret = gnutls_record_send( *m_session, data.c_str() + sum, data.length() - sum );
      sum += ret;
    }
    while( ( ret == GNUTLS_E_AGAIN ) || ( ret == GNUTLS_E_INTERRUPTED ) || sum < data.length() );
    return true;
  }

  int GnuTLSBase::decrypt( const std::string& data )
  {
    m_recvBuffer += data;

    if( !m_secure )
    {
      handshake();
      return static_cast<int>( data.length() );
    }

    int sum = 0;
    int ret = 0;
    bool stop = false;
    do
    {
      if( ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED)
        stop = true;
      else
        stop = false;

      ret = static_cast<int>( gnutls_record_recv( *m_session, m_buf, m_bufsize ) );
      if( ret > 0 && m_handler )
      {
        m_handler->handleDecryptedData( this, std::string( m_buf, ret ) );
        sum += ret;
      }
      if( stop )
        break;
    }
    while( ret > 0 || ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED );

    return sum;
  }

  void GnuTLSBase::cleanup()
  {
    if( !m_mutex.trylock() )
      return;

    TLSHandler* handler = m_handler;
    m_handler = 0;
    gnutls_bye( *m_session, GNUTLS_SHUT_RDWR );
    gnutls_db_remove_session( *m_session );
    gnutls_credentials_clear( *m_session );
    if( m_session )
      gnutls_deinit( *m_session );

    delete m_session;

    m_secure = false;
    m_valid = false;
    m_session = 0;
    m_session = new gnutls_session_t;
    m_handler = handler;

    m_mutex.unlock();
  }

  bool GnuTLSBase::handshake()
  {
    if( !m_handler )
      return false;

    int ret = gnutls_handshake( *m_session );
    if( ret < 0 && gnutls_error_is_fatal( ret ) )
    {
      gnutls_perror( ret );
      gnutls_db_remove_session( *m_session );
      m_valid = false;

      m_handler->handleHandshakeResult( this, false, m_certInfo );
      return false;
    }
    else if( ret == GNUTLS_E_AGAIN )
    {
      return true;
    }

    m_secure = true;

    getCertInfo();

    m_handler->handleHandshakeResult( this, true, m_certInfo );
    return true;
  }

  bool GnuTLSBase::hasChannelBinding() const
  {
#ifdef HAVE_GNUTLS_SESSION_CHANNEL_BINDING
    return true;
#else
    return false;
#endif
  }

  const std::string GnuTLSBase::channelBinding() const
  {
#ifdef HAVE_GNUTLS_SESSION_CHANNEL_BINDING
    gnutls_datum_t cb;
    int rc;
    rc = gnutls_session_channel_binding( *m_session,
#ifdef GNUTLS_CB_TLS_EXPORTER
                                         ( m_certInfo.protocol == "TLSv1.3" ) ? GNUTLS_CB_TLS_EXPORTER : GNUTLS_CB_TLS_UNIQUE,
#else
                                         GNUTLS_CB_TLS_UNIQUE,
#endif
                                         &cb );
    if( !rc )
      return std::string( reinterpret_cast<char*>( cb.data ), cb.size );
    else
#endif
      return EmptyString;
  }

  ssize_t GnuTLSBase::pullFunc( void* data, size_t len )
  {
    ssize_t cpy = ( len > m_recvBuffer.length() ) ? ( m_recvBuffer.length() ) : ( len );
    if( cpy > 0 )
    {
      memcpy( data, static_cast<const void*>( m_recvBuffer.c_str() ), cpy );
      m_recvBuffer.erase( 0, cpy );
      return cpy;
    }
    else
    {
      errno = EAGAIN;
      return GNUTLS_E_AGAIN;
    }
  }

  ssize_t GnuTLSBase::pullFunc( gnutls_transport_ptr_t ptr, void* data, size_t len )
  {
    return static_cast<GnuTLSBase*>( ptr )->pullFunc( data, len );
  }

  ssize_t GnuTLSBase::pushFunc( const void* data, size_t len )
  {
    if( m_handler )
      m_handler->handleEncryptedData( this, std::string( static_cast<const char*>( data ), len ) );

    return len;
  }

  ssize_t GnuTLSBase::pushFunc( gnutls_transport_ptr_t ptr, const void* data, size_t len )
  {
    return static_cast<GnuTLSBase*>( ptr )->pushFunc( data, len );
  }

  void GnuTLSBase::getCommonCertInfo()
  {
    const char* info;
    info = gnutls_compression_get_name( gnutls_compression_get( *m_session ) );
    if( info )
      m_certInfo.compression = info;

    info = gnutls_mac_get_name( gnutls_mac_get( *m_session ) );
    if( info )
      m_certInfo.mac = info;

    info = gnutls_cipher_get_name( gnutls_cipher_get( *m_session ) );
    if( info )
      m_certInfo.cipher = info;

    switch( gnutls_protocol_get_version( *m_session ) )
    {
      // case GNUTLS_SSL3:
      //   m_certInfo.protocol = SSL3;
      //   break;
      case GNUTLS_TLS1_0:
        m_certInfo.protocol = "TLSv1";
        break;
      case GNUTLS_TLS1_1:
        m_certInfo.protocol = "TLSv1.1";
        break;
      case GNUTLS_TLS1_2:
        m_certInfo.protocol = "TLSv1.2";
        break;
      case GNUTLS_TLS1_3:
        m_certInfo.protocol = "TLSv1.3";
        break;
      case GNUTLS_DTLS1_0:
        m_certInfo.protocol = "DTLSv1";
        break;
      case GNUTLS_DTLS1_2:
        m_certInfo.protocol = "DTLSv1.2";
        break;
      default:
        m_certInfo.protocol = "Unknown protocol";
        break;
    }
  }

}

#endif // HAVE_GNUTLS
