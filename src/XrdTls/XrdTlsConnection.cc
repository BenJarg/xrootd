//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <simonm@cern.ch>
//------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

#include <errno.h>
#include <iostream>
#include <poll.h>
#include <stdio.h>
#include <time.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "XrdTls/XrdTlsConnection.hh"
#include "XrdTls/XrdTlsContext.hh"

#include <stdexcept>

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdTlsGlobal
{
extern XrdTlsContext::msgCB_t msgCB;
}
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdTlsConnection::XrdTlsConnection( XrdTlsContext &ctx, int  sfd,
                                    XrdTlsConnection::RW_Mode rwm,
                                    XrdTlsConnection::HS_Mode hsm,
                                    bool isClient )
                 : ssl(0), sFD(-1), hsDone(false), fatal(false)
{

// Simply initialize this object and throw an exception if it fails
//
   const char *eMsg = Init(ctx, sfd, rwm, hsm, isClient);
   if (eMsg) throw std::invalid_argument( eMsg );
}

/******************************************************************************/
/*                                A c c e p t                                 */
/******************************************************************************/
  
int XrdTlsConnection::Accept()
{
   int rc, error;

// An accept may require several tries, so we do that here.
//
do{if ((rc = SSL_accept( ssl )) > 0)
      {if (cOpts & xVerify)
          {rc = SSL_get_verify_result(ssl);
           if (rc != X509_V_OK)
              {FlushErrors("x509_Verify()"); return -1;} // ???
          }
       return SSL_ERROR_NONE;
      }


   // Get the actual SSL error code.
   //
   error = Diagnose(rc);

   // Check why we did not succeed. We may be able to recover.
   //
   if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE)
      {FlushErrors("SSL_accept()", error);
       SSL_free( ssl );
       ssl = 0;
       errno = ECONNABORTED;
       return SSL_ERROR_SYSCALL;
      }

  } while(Wait4OK(error == SSL_ERROR_WANT_READ));

// If we are here then we got a syscall error
//
   return SSL_ERROR_SYSCALL;
}

/******************************************************************************/
/*                               C o n n e c t                                */
/******************************************************************************/
  
int XrdTlsConnection::Connect(const char *thehost)
{

// Setup host verification of a host has been specified
//

// Do the connect.
//
   int rc = SSL_connect( ssl );
   if (rc != 1) return Diagnose(rc);

// Make sure cert verification went well

   if (xVerify)
      {rc = SSL_get_verify_result(ssl);
       if (rc != X509_V_OK) {FlushErrors("x509_Verify()"); return -1;}
      }

   return SSL_ERROR_NONE;
}

/******************************************************************************/
/* Private:                     D i a g n o s e                               */
/******************************************************************************/
  
int XrdTlsConnection::Diagnose(int sslrc)
{
int eCode = SSL_get_error( ssl, sslrc );

// Make sure we can shutdown
//
   if (eCode == SSL_ERROR_SYSCALL || eCode == SSL_ERROR_SSL) fatal = true;

// Return the errors
//
   return eCode;
}
  
/******************************************************************************/
/*                              E r r 2 T e x t                               */
/******************************************************************************/

std::string XrdTlsConnection::Err2Text(int sslerr)
{
   char *eP, eBuff[1024];

   if (sslerr == SSL_ERROR_SYSCALL)
      {int rc = errno;
       if (!rc) rc = EPIPE;
       snprintf(eBuff, sizeof(eBuff), "%s", strerror(rc));
       *eBuff = tolower(*eBuff);
       eP = eBuff;
      } else {
       ERR_error_string_n(sslerr, eBuff, sizeof(eBuff));
       if (cOpts & Debug) eP = eBuff;
          else {char *colon = rindex(eBuff, ':');
                eP = (colon ? colon+1 : eBuff);
               }

      }
   return std::string(eP);
}
  
/******************************************************************************/
/*                           F l u s h E r r o r s                            */
/******************************************************************************/
  
void XrdTlsConnection::FlushErrors(const char *what, int sslrc)
{
  unsigned long eCode;
  char emsg[1024];

// First print the specific error
//
   if (sslrc)
      {std::string sslmsg = Err2Text(sslrc);
       snprintf(emsg,sizeof(emsg),"%s failed; %s",what,sslmsg.c_str());
       XrdTlsGlobal::msgCB(traceID, emsg, false);
      }

// Now flush all the ssl errors
//
  while((eCode = ERR_get_error()))
       {ERR_error_string_n(eCode, emsg, sizeof(emsg));
        XrdTlsGlobal::msgCB(traceID, emsg, true);
       }
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

const char *XrdTlsConnection::Init( XrdTlsContext &ctx, int sfd,
                                    XrdTlsConnection::RW_Mode rwm,
                                    XrdTlsConnection::HS_Mode hsm,
                                    bool isClient, const char *tid )
{
   BIO *rbio, *wbio = 0;

// Make sure this connection is not in use
//
   if ( ssl ) return "TLS I/O: connection is still in use.";

// Get the ssl object from the context, there better be one.
//
   SSL_CTX *ssl_ctx = ctx.Context();
   if (ssl_ctx == 0) return "TLS I/O: context inialization failed.";

// Initialze values from the context.
//
   tlsctx = &ctx;
   const XrdTlsContext::CTX_Params *parms = ctx.GetParams();
   hsWait = (parms->opts & XrdTlsContext::hsto) * 1000; // Poll timeout
   if (ctx.x509Verify()) cOpts = xVerify;
      else cOpts = 0;
   if (parms->opts & XrdTlsContext::debug) cOpts |= Debug;
   traceID = tid;

// Obtain the ssl object at this point.
//
   ssl = SSL_new( ssl_ctx );
   if (ssl == 0) return "TLS I/O: failed to get ssl object.";

// Set the ssl object state to correspond to client or server type
//
   if (isClient)
      {SSL_set_connect_state( ssl );
       cAttr = 0;
      } else {
       SSL_set_accept_state( ssl );
       cAttr = isServer;
      }

// Allocate right number of bio's and initialize them as requested. Note
// that when the read and write bios have the same attribue, we use only one.
//
   switch( rwm )
   {
     case TLS_RNB_WNB:
          rbio = BIO_new_socket( sfd, BIO_NOCLOSE );
          BIO_set_nbio( rbio, 1 );
          break;

     case TLS_RNB_WBL:
          rbio = BIO_new_socket( sfd, BIO_NOCLOSE );
          BIO_set_nbio( rbio, 1 );
          wbio = BIO_new_socket( sfd, BIO_NOCLOSE );
          cAttr |= wBlocking;
          break;

     case TLS_RBL_WNB:
          rbio = BIO_new_socket( sfd, BIO_NOCLOSE );
          wbio = BIO_new_socket( sfd, BIO_NOCLOSE );
          BIO_set_nbio( wbio, 1 );
          cAttr |= rBlocking;
          break;

     case TLS_RBL_WBL:
          rbio = BIO_new_socket( sfd, BIO_NOCLOSE );
          cAttr |= (rBlocking | wBlocking);
          break;

     default: return "TLS I/O: invalid TLS rw mode."; break;
   }

// Set correct handshake mode
//
   switch( hsm )
   {
     case TLS_HS_BLOCK: hsMode = rwBlock; break;
     case TLS_HS_NOBLK: hsMode = noBlock; break;
     case TLS_HS_XYBLK: hsMode = xyBlock; break;

     default: return "TLS I/O: invalid TLS hs mode."; break;
    }

// Finally attach the bios to the ssl object. When the ssl object is freed
// the bios will be freed as well.
//
   sFD = sfd;
   if (wbio == 0) wbio = rbio;
   SSL_set_bio( ssl, rbio, wbio );

// Set timeouts on this socket to allow SSL to not block
//
/*??? Likely we don't need to do this, but of we do, include
   #include <time.h>
   #include <sys/socket.h>

   struct timeval tv;
   tv.tv_sec = 10;
   tv.tv_usec = 0;
   setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
   setsockopt(sfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
*/

// All done. The caller will do an Accept() or Connect() afterwards.
//
   return 0;
}

/******************************************************************************/
/*                                  P e e k                                   */
/******************************************************************************/
  
  int XrdTlsConnection::Peek( char *buffer, size_t size, int &bytesPeek )
  {
    int error;

    //------------------------------------------------------------------------
    // If necessary, SSL_read() will negotiate a TLS/SSL session, so we don't
    // have to explicitly call SSL_connect or SSL_do_handshake.
    //------------------------------------------------------------------------
 do{int rc = SSL_peek( ssl, buffer, size );

    // Note that according to SSL whenever rc > 0 then SSL_ERROR_NONE can be
    // returned to the caller. So, we short-circuit all the error handling.
    //
    if( rc > 0 )
      {bytesPeek = rc;
       return SSL_ERROR_NONE;
      }

    // We have a potential error. Get the SSL error code and whether or
    // not the handshake actually is finished (semi-accurate)
    //
    hsDone = bool( SSL_is_init_finished( ssl ) );
    error = Diagnose(rc);

    // The connection creator may wish that we wait for the handshake to
    // complete. This is a tricky issue for non-blocking bio's as a read
    // may force us to wait until writes are possible. All of this is rare!
    //
    if ((!hsMode || hsDone || (error != SSL_ERROR_WANT_READ &&
                               error != SSL_ERROR_WANT_WRITE))
    ||   (hsMode == xyBlock && error == SSL_ERROR_WANT_READ)) return error;

   } while(Wait4OK(error == SSL_ERROR_WANT_READ));

    return SSL_ERROR_SYSCALL;
  }

/******************************************************************************/
/*                               P e n d i n g                                */
/******************************************************************************/

int XrdTlsConnection::Pending(bool any)
{
   if (!any) return SSL_pending(ssl);
#if OPENSSL_VERSION_NUMBER < 0x10100000L
   return SSL_pending(ssl) != 0;
#else
   return SSL_has_pending(ssl);
#endif
}

/******************************************************************************/
/*                                  R e a d                                   */
/******************************************************************************/
  
  int XrdTlsConnection::Read( char *buffer, size_t size, int &bytesRead )
  {
    int error;
    //------------------------------------------------------------------------
    // If necessary, SSL_read() will negotiate a TLS/SSL session, so we don't
    // have to explicitly call SSL_connect or SSL_do_handshake.
    //------------------------------------------------------------------------
 do{int rc = SSL_read( ssl, buffer, size );

    // Note that according to SSL whenever rc > 0 then SSL_ERROR_NONE can be
    // returned to the caller. So, we short-circuit all the error handling.
    //
    if( rc > 0 )
      {bytesRead = rc;
       return SSL_ERROR_NONE;
      }

    // We have a potential error. Get the SSL error code and whether or
    // not the handshake actually is finished (semi-accurate)
    //
    hsDone = bool( SSL_is_init_finished( ssl ) );
    error = Diagnose(rc);

    // The connection creator may wish that we wait for the handshake to
    // complete. This is a tricky issue for non-blocking bio's as a read
    // may force us to wait until writes are possible. All of this is rare!
    //
    if ((!hsMode || hsDone || (error != SSL_ERROR_WANT_READ &&
                               error != SSL_ERROR_WANT_WRITE))
    ||   (hsMode == xyBlock && error == SSL_ERROR_WANT_READ)) return error;

   } while(Wait4OK(error == SSL_ERROR_WANT_READ));

    return SSL_ERROR_SYSCALL;
  }

/******************************************************************************/
/*                              S h u t d o w n                               */
/******************************************************************************/
  
void XrdTlsConnection::Shutdown(XrdTlsConnection::SDType sdType)
{
   int sdMode, rc;

// Make sure we have an ssl object
//
   if (ssl == 0) return;

// Perform shutdown as needed. This is required before freeing the ssl object.
// If we previously encountered a SYSCALL or SSL error, shutdown is prohibited!
// The following code is patterned after code in the public TomCat server.
//
   if (!fatal)
      {switch(sdType)
             {case sdForce: // Forced shutdown which violate TLS standard!
                   sdMode = SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN;
                   break;
              case sdWait:  // Wait for client acknowledgement
                   sdMode = 0;
              default:      // Fast shutdown, don't wait for ack (compliant)
                   sdMode = SSL_RECEIVED_SHUTDOWN;
                   break;
             }

       SSL_set_shutdown(ssl, sdMode);

       for (int i = 0; i < 4; i++)
           {rc = SSL_shutdown( ssl );
            if (rc > 0) break;
            if (rc < 0)
               {rc = SSL_get_error( ssl, rc );
                if (rc == SSL_ERROR_WANT_READ || rc == SSL_ERROR_WANT_WRITE)
                   {if (Wait4OK(rc == SSL_ERROR_WANT_READ)) continue;
                    rc = SSL_ERROR_SYSCALL;
                   }
                char msgBuff[512];
                std::string eMsg = Err2Text(rc);
                snprintf(msgBuff, sizeof(msgBuff),
                        "FD %d TLS shutdown failed; %s.\n",sFD,eMsg.c_str());
                XrdTlsGlobal::msgCB(traceID, msgBuff, false);
                break;
               }
           }
      }

// Now free the ssl object which will free all the BIO's associated with it
//
   SSL_free( ssl );
   ssl = 0;
}

/******************************************************************************/
/*                                 W r i t e                                  */
/******************************************************************************/
  
  int XrdTlsConnection::Write( const char *buffer, size_t size,
                               int &bytesWritten )
  {
    int error;

    //------------------------------------------------------------------------
    // If necessary, SSL_write() will negotiate a TLS/SSL session, so we don't
    // have to explicitly call SSL_connect or SSL_do_handshake.
    //------------------------------------------------------------------------
 do{int rc = SSL_write( ssl, buffer, size );

    // Note that according to SSL whenever rc > 0 then SSL_ERROR_NONE can be
    // returned to the caller. So, we short-circuit all the error handling.
    //
    if( rc > 0 )
      {bytesWritten = rc;
       return SSL_ERROR_NONE;
      }

    // We have a potential error. Get the SSL error code and whether or
    // not the handshake actually is finished (semi-accurate)
    //
    hsDone = bool( SSL_is_init_finished( ssl ) );
    error = SSL_get_error( ssl, rc );

    // The connection creator may wish that we wait for the handshake to
    // complete. This is a tricky issue for non-blocking bio's as a write
    // may force us to wait until reads are possible. All of this is rare!
    //
    if ((!hsMode || hsDone || (error != SSL_ERROR_WANT_READ &&
                               error != SSL_ERROR_WANT_WRITE))
    ||   (hsMode == xyBlock && error == SSL_ERROR_WANT_WRITE)) return error;

   } while(Wait4OK(error == SSL_ERROR_WANT_READ));

    return SSL_ERROR_SYSCALL;
  }

/******************************************************************************/
/*                               V e r s i o n                                */
/******************************************************************************/

const char *XrdTlsConnection::Version()
{
   return SSL_get_version(ssl);
}
  
/******************************************************************************/
/* Private:                      W a i t 4 O K                                */
/******************************************************************************/
  
bool XrdTlsConnection::Wait4OK(bool wantRead)
{
   static const short rdOK = POLLIN |POLLRDNORM;
   static const short wrOK = POLLOUT|POLLWRNORM;
   struct pollfd polltab = {sFD, (wantRead ? rdOK : wrOK), 0};
   int rc, timeout;

   // Establish how long we will wait.
   //
   timeout = (hsDone ? hsWait : -1);

   do {rc = poll(&polltab, 1, timeout);} while(rc < 0 && errno == EINTR);

   // Make sure we have a clean state, otherwise indicate we failed. The
   // caller will need to perform the correct action.
   //
   if (rc == 1)
      {if (polltab.revents & (wantRead ? rdOK : wrOK)) return true;
       if (polltab.revents & POLLERR) errno = EIO;
          else if (polltab.revents & (POLLHUP|POLLNVAL)) errno = EPIPE;
                  else errno = EINVAL;
      } else if (!rc) errno = ETIMEDOUT; // This is not possible
   return false;
}
