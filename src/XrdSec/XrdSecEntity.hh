#ifndef __SEC_ENTITY_H__
#define __SEC_ENTITY_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d S e c E n t i t y . h h                        */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

//------------------------------------------------------------------------------
//! This object is returned during authentication. This is most relevant for
//! client authentication unless mutual authentication has been implemented
//! in which case the client can also authenticate the server. It is embeded
//! in each security protocol object to facilitate mutual authentication. Note
//! that the destructor does nothing and it is the responsibility of the 
//! seurity protocol object to delete the public XrdSecEntity data members.
//!
//! Note: The host member contents are depdent on the dnr/nodnr setting and
//!       and contain a host name or an IP address. To get the real host name
//!       use addrInfo->Name(), this is required for any hostname comparisons.
//------------------------------------------------------------------------------

#include <string>

#include "XrdSys/XrdSysPthread.hh"

#define XrdSecPROTOIDSIZE 8

class XrdNetAddrInfo;
class XrdSecAttr;
class XrdSecEntityAttrs;
class XrdSecEntityXtend;
  
/******************************************************************************/
/*                          X r d S e c E n t i t y                           */
/******************************************************************************/

// The XrdSecEntity describes the client associated with a connection. One
// such object is allocated for each clent connection and it persists until
// the connection is closed. Note that when an entity has more than one
// role or vorg, the fields <vorg, role, grps> form a columnar tuple. This
// tuple must be repeated whenever any one of the values differs.
//
class XrdSecEntity
{
public:
         char    pros[XrdSecPROTOIDSIZE]; //!< Server   protocol used (e.g. http)
         char    prot[XrdSecPROTOIDSIZE]; //!< Security protocol used (e.g. krb5)
         char   *name;                    //!< Entity's name
         char   *host;                    //!< Entity's host name dnr dependent
         char   *vorg;                    //!< Entity's virtual organization(s)
         char   *role;                    //!< Entity's role(s)
         char   *grps;                    //!< Entity's group name(s)
         char   *endorsements;            //!< Protocol specific endorsements
         char   *moninfo;                 //!< Information for monitoring
         char   *creds;                   //!< Raw entity credentials or cert
         int     credslen;                //!< Length of the 'creds' data
         int     rsvd;                    //!< Reserved field
XrdNetAddrInfo  *addrInfo;                //!< Entity's connection details
const    char   *tident;                  //!< Trace identifier always preset
         void   *sessvar;                 //!< Plugin settable storage pointer,
                                          //!< now deprecated. Use settable
                                          //!< attribute objects instead.
         uid_t   uid;                     //!< Unix uid or 0 if none
         gid_t   gid;                     //!< Unix gid or 0 if none
         void   *future[3];               //!< Reserved for future expansion

//------------------------------------------------------------------------------
//! Add an attribute object to this entity.
//!
//! @param  attr    - Reference to the attribute object.
//!
//! @return True, the object was added.
//! @return False, the object was not added because such an object exists.
//------------------------------------------------------------------------------

         bool    Add(XrdSecAttr &attr);

//------------------------------------------------------------------------------
//! Add a key-value attribute to this entity. If one exists it is replaced.
//!
//! @param  key     - Reference to the key.
//! @param  val     - Reference to the value.
//! @param  replace - When true, any existing key-value is replaced. Otherwise,
//!                    the add is not performed.
//!
//! @return True, the key-value was added or replaced.
//! @return False, the key already exists so he value was not added.
//------------------------------------------------------------------------------

         bool    Add(const std::string &key,
                     const std::string &val, bool replace=false);

//------------------------------------------------------------------------------
//! Get an attribute object associated with this entity.
//!
//! @param  sigkey  - A unique attribute object signature key.
//!
//! @return Upon success a pointer to the attribute object is returned.
//!         Otherwise, a nil pointer is returned.
//------------------------------------------------------------------------------

XrdSecAttr      *Get(const void *sigkey);

//------------------------------------------------------------------------------
//! Get an attribute key value associated with this entity.
//!
//! @param  key     - The reference to the key.
//! @param  val     - The reference to the string object to receive the value.
//!
//! @return Upon success true is returned. If the key does not exist, false
//!         is returned and the val object remains unchanged.
//------------------------------------------------------------------------------

         bool    Get(const std::string &key, std::string &val);

//------------------------------------------------------------------------------
//! Reset object to it's pristine self.
//!
//! @param  isNew   - True when called the first time, false otherwise.
//! @param  spV     - The name of the security protocol.
//! @param  dpV     - The name of the data     protocol.
//------------------------------------------------------------------------------

         void    Reset(bool isnew=false, const char *spV=0, const char *dpV=0);

//------------------------------------------------------------------------------
//! Reset object attributes.
//!
//! @param  doDel   - When true, the attribute extension is deleted as well.
//------------------------------------------------------------------------------

         void    ResetAttrs(bool doDel=false);

//------------------------------------------------------------------------------
//! Constructor.
//!
//! @param  spName  - The name of the security protocol.
//! @param  dpName  - The name of the data     protocol.
//------------------------------------------------------------------------------

         XrdSecEntity(const char *spName=0, const char *dpName=0)
                     {Reset(true, dpName, spName);}

        ~XrdSecEntity() {ResetAttrs(true);}

private:
XrdSysMutex        eMutex;
XrdSecEntityAttrs *entAttrs;
XrdSecEntityXtend *entXtend;
};

#define XrdSecClientName XrdSecEntity
#define XrdSecServerName XrdSecEntity
#endif
