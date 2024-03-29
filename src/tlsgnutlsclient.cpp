/*
  Copyright (c) 2005-2023 by Jakob Schröter <js@camaya.net>
  This file is part of the gloox library. http://camaya.net/gloox

  This software is distributed under a license. The full license
  agreement can be found in the file LICENSE in this distribution.
  This software may not be copied, modified, sold or distributed
  other than expressed in the named license agreement.

  This software is distributed without any warranty.
*/



#include "tlsgnutlsclient.h"

#ifdef HAVE_GNUTLS

#include <errno.h>

namespace gloox
{

  GnuTLSClient::GnuTLSClient( TLSHandler* th, const std::string& server )
    : GnuTLSBase( th, server )
  {
  }

  GnuTLSClient::~GnuTLSClient()
  {
  }

  void GnuTLSClient::cleanup()
  {
    GnuTLSBase::cleanup();
    if( m_credentials )
      gnutls_certificate_free_credentials( m_credentials );
    init();
  }

  bool GnuTLSClient::init( const std::string& /*clientKey*/,
                           const std::string& /*clientCerts*/,
                           const StringList& /*cacerts*/ )
  {
    if( m_initLib && gnutls_global_init() != 0 )
      return false;

    if( gnutls_certificate_allocate_credentials( &m_credentials ) < 0 )
      return false;

    if( gnutls_init( m_session, GNUTLS_CLIENT ) != 0 )
    {
      gnutls_certificate_free_credentials( m_credentials );
      return false;
    }

#if GNUTLS_VERSION_NUMBER >= 0x020600
    int ret = gnutls_priority_set_direct( *m_session, "SECURE128:+PFS:+COMP-ALL:+VERS-TLS-ALL:-VERS-SSL3.0:+SIGN-ALL:+CURVE-ALL", 0 );
    if( ret != GNUTLS_E_SUCCESS )
      return false;
#else
    const int protocolPriority[] = {
      GNUTLS_TLS1_3,
      GNUTLS_TLS1_2,
      GNUTLS_TLS1_1, GNUTLS_TLS1, 0 };
    const int kxPriority[]       = { GNUTLS_KX_RSA, GNUTLS_KX_DHE_RSA, GNUTLS_KX_DHE_DSS, 0 };
    const int cipherPriority[]   = { GNUTLS_CIPHER_AES_256_CBC, GNUTLS_CIPHER_AES_128_CBC,
                                     GNUTLS_CIPHER_3DES_CBC, GNUTLS_CIPHER_ARCFOUR, 0 };
    const int compPriority[]     = { GNUTLS_COMP_ZLIB, GNUTLS_COMP_NULL, 0 };
    const int macPriority[]      = { GNUTLS_MAC_SHA, GNUTLS_MAC_MD5, 0 };
    gnutls_protocol_set_priority( *m_session, protocolPriority );
    gnutls_cipher_set_priority( *m_session, cipherPriority );
    gnutls_compression_set_priority( *m_session, compPriority );
    gnutls_kx_set_priority( *m_session, kxPriority );
    gnutls_mac_set_priority( *m_session, macPriority );
#endif

    gnutls_certificate_set_x509_system_trust( m_credentials );
    gnutls_credentials_set( *m_session, GNUTLS_CRD_CERTIFICATE, m_credentials );

    gnutls_transport_set_ptr( *m_session, static_cast<gnutls_transport_ptr_t>( this ) );
    gnutls_transport_set_push_function( *m_session, pushFunc );
    gnutls_transport_set_pull_function( *m_session, pullFunc );

    m_valid = true;
    return true;
  }

  void GnuTLSClient::setCACerts( const StringList& cacerts )
  {
    m_cacerts = cacerts;

    StringList::const_iterator it = m_cacerts.begin();
    for( ; it != m_cacerts.end(); ++it )
      gnutls_certificate_set_x509_trust_file( m_credentials, (*it).c_str(), GNUTLS_X509_FMT_PEM );
  }

  void GnuTLSClient::setClientCert( const std::string& clientKey, const std::string& clientCerts )
  {
    m_clientKey = clientKey;
    m_clientCerts = clientCerts;

    if( !m_clientKey.empty() && !m_clientCerts.empty() )
    {
      gnutls_certificate_set_x509_key_file( m_credentials, m_clientCerts.c_str(),
                                            m_clientKey.c_str(), GNUTLS_X509_FMT_PEM );
    }
  }

  void GnuTLSClient::getCertInfo()
  {
    unsigned int status;
    bool error = false;

    gnutls_certificate_free_ca_names( m_credentials );

    if( gnutls_certificate_verify_peers2( *m_session, &status ) < 0 )
      error = true;

    m_certInfo.status = 0;
    if( status & GNUTLS_CERT_INVALID )
      m_certInfo.status |= CertInvalid;
    if( status & GNUTLS_CERT_SIGNER_NOT_FOUND )
      m_certInfo.status |= CertSignerUnknown;
    if( status & GNUTLS_CERT_REVOKED )
      m_certInfo.status |= CertRevoked;
    if( status & GNUTLS_CERT_SIGNER_NOT_CA )
      m_certInfo.status |= CertSignerNotCa;

    const gnutls_datum_t* certList = 0;
    unsigned int certListSize = 0;
    if( !error && ( ( certList = gnutls_certificate_get_peers( *m_session, &certListSize ) ) == 0 ) )
      error = true;

    unsigned int certListSizeFull = certListSize;

    gnutls_x509_crt_t* cert = new gnutls_x509_crt_t[certListSize];
    for( unsigned int i=0; !error && ( i<certListSize ); ++i )
    {
      if( gnutls_x509_crt_init( &cert[i] ) < 0
          || gnutls_x509_crt_import( cert[i], &certList[i], GNUTLS_X509_FMT_DER ) < 0 )
        error = true;
    }

    if( certListSize > 1 && ( gnutls_x509_crt_check_issuer( cert[certListSize-1], cert[certListSize-1] ) > 0 ) )
      certListSize--;

    for( unsigned int i=1; !error && ( i<certListSize ); ++i )
    {
      error = !verifyAgainst( cert[i-1], cert[i] );
      if( error )
        m_certInfo.status |= CertInvalid;
    }

    m_certInfo.chain = verifyAgainstCAs( cert[certListSize-1], 0 /*CAList*/, 0 /*CAListSize*/ );

    time_t t = gnutls_x509_crt_get_activation_time( cert[0] );
    if( t == -1 )
      error = true;
    else if( t > time( 0 ) )
      m_certInfo.status |= CertNotActive;
    m_certInfo.date_from = static_cast<int>( t );

    t = gnutls_x509_crt_get_expiration_time( cert[0] );
    if( t == -1 )
      error = true;
    else if( t < time( 0 ) )
      m_certInfo.status |= CertExpired;
    m_certInfo.date_to = static_cast<int>( t );

    char name[64];
    size_t nameSize = sizeof( name );
    gnutls_x509_crt_get_issuer_dn( cert[0], name, &nameSize );
    m_certInfo.issuer = name;

    nameSize = sizeof( name );
    gnutls_x509_crt_get_dn( cert[0], name, &nameSize );
    m_certInfo.server = name;

    getCommonCertInfo();

    if( !gnutls_x509_crt_check_hostname( cert[0], m_server.c_str() ) )
      m_certInfo.status |= CertWrongPeer;

    for( unsigned int i = 0; i < certListSizeFull; ++i )
      gnutls_x509_crt_deinit( cert[i] );

    delete[] cert;

    m_valid = true;
  }

  static bool verifyCert( gnutls_x509_crt_t cert, unsigned result )
  {
    return ! ( ( result & GNUTLS_CERT_INVALID )
      || gnutls_x509_crt_get_expiration_time( cert ) < time( 0 )
      || gnutls_x509_crt_get_activation_time( cert ) > time( 0 ) );
  }

  bool GnuTLSClient::verifyAgainst( gnutls_x509_crt_t cert, gnutls_x509_crt_t issuer )
  {
    unsigned int result;
    gnutls_x509_crt_verify( cert, &issuer, 1, 0, &result );
    return verifyCert( cert, result );
  }

  bool GnuTLSClient::verifyAgainstCAs( gnutls_x509_crt_t cert, gnutls_x509_crt_t* CAList, int CAListSize )
  {
    unsigned int result;
    gnutls_x509_crt_verify( cert, CAList, CAListSize, GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT, &result );
    return verifyCert( cert, result );
  }

}

#endif // HAVE_GNUTLS
