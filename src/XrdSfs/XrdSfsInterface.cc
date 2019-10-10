/******************************************************************************/
/*                                                                            */
/*                    X r d S f s I n t e r f a c e . h h                     */
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

#include "XrdOuc/XrdOucCRC.hh"
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdSfs/XrdSfsInterface.hh"

/******************************************************************************/
/*                        S t a t i c   S y m b o l s                         */
/******************************************************************************/
  
namespace
{
static const XrdSfsFileOffset pgSize = XrdSfsPageSize;
}

/******************************************************************************/
/*                                p g R e a d                                 */
/******************************************************************************/
  
XrdSfsXferSize XrdSfsFile::pgRead(XrdSfsFileOffset   offset,
                                  char              *buffer,
                                  XrdSfsXferSize     rdlen,
                                  uint32_t          *csvec,
                                  bool               verify)
{
   XrdSfsXferSize bytes;

// Make sure the offset is on a 4K boundary and the size if a multiple of
// 4k as well (we use simple and for this).
//
   if ((offset & ~pgSize) || (rdlen & ~XrdSfsPageSize))
      {error.setErrInfo(EINVAL,"Offset or readlen not a multiple of pagesize.");
       return SFS_ERROR;
      }

// Read the data into the buffer
//
   bytes = read(offset, buffer, rdlen);

// Calculate checksums if so wanted
//
   if (bytes > 0 && csvec)
      XrdOucCRC::Calc32C((void *)buffer, rdlen, csvec, XrdSfsPageSize);

// All done
//
   return bytes;
}

/******************************************************************************/
  
XrdSfsXferSize XrdSfsFile::pgRead(XrdSfsAio *aioparm, bool verify)
{
   aioparm->Result = this->pgRead((XrdSfsFileOffset)aioparm->sfsAio.aio_offset,
                                            (char *)aioparm->sfsAio.aio_buf,
                                    (XrdSfsXferSize)aioparm->sfsAio.aio_nbytes,
                                                    aioparm->cksVec, verify);
   aioparm->doneRead();
   return SFS_OK;
}

/******************************************************************************/
/*                               p g W r i t e                                */
/******************************************************************************/
  
XrdSfsXferSize XrdSfsFile::pgWrite(XrdSfsFileOffset   offset,
                                   char              *buffer,
                                   XrdSfsXferSize     wrlen,
                                   uint32_t          *csvec,
                                   bool               verify)
{
// Make sure the offset is on a 4K boundary
//
   if (offset & ~pgSize)
      {error.setErrInfo(EINVAL,"Offset or readlen not a multiple of pagesize.");
       return SFS_ERROR;
      }

// If a virtual end of file marker is set, make sure we are not trying to
// write past it.
//
   if (pgwrEOF && (offset+wrlen) > pgwrEOF)
      {error.setErrInfo(ESPIPE,"Attempt to write past virtual EOF.");
       return SFS_ERROR;
      }

// If this is a short write then establish the virtual eof
//
   if (wrlen & ~XrdSfsPageSize) pgwrEOF = (offset + wrlen) & ~pgSize;

// If we have a checksum vector and verify is on, make sure the data
// in the buffer corresponds to he checksums.
//
   if (csvec && verify)
      {int pgErr;
       if (!XrdOucCRC::Ver32C((void *)buffer,wrlen,csvec,XrdSfsPageSize,pgErr))
          {error.setErrInfo(EDOM,"Checksum error.");
           return SFS_ERROR;
          }
      }

// Now just return the result of a plain write
//
   return write(offset, buffer, wrlen);
}

/******************************************************************************/
  
XrdSfsXferSize XrdSfsFile::pgWrite(XrdSfsAio *aioparm, bool verify)
{
   aioparm->Result = this->pgWrite((XrdSfsFileOffset)aioparm->sfsAio.aio_offset,
                                             (char *)aioparm->sfsAio.aio_buf,
                                     (XrdSfsXferSize)aioparm->sfsAio.aio_nbytes,
                                                     aioparm->cksVec, verify);
   aioparm->doneWrite();
   return SFS_OK;
}

/******************************************************************************/
/*                                 r e a d v                                  */
/******************************************************************************/
  
XrdSfsXferSize XrdSfsFile::readv(XrdOucIOVec      *readV,
                                 int               rdvCnt)
{
   XrdSfsXferSize rdsz, totbytes = 0;

   for (int i = 0; i < rdvCnt; i++)
       {rdsz = read(readV[i].offset,
                    readV[i].data, readV[i].size);
        if (rdsz != readV[i].size)
           {if (rdsz < 0) return rdsz;
            error.setErrInfo(ESPIPE,"read past eof");
            return SFS_ERROR;
           }
        totbytes += rdsz;
       }
   return totbytes;
}

/******************************************************************************/
/*                                w r i t e v                                 */
/******************************************************************************/
  
XrdSfsXferSize XrdSfsFile::writev(XrdOucIOVec      *writeV,
                                  int               wdvCnt)
{
    XrdSfsXferSize wrsz, totbytes = 0;

    for (int i = 0; i < wdvCnt; i++)
        {wrsz = write(writeV[i].offset,
                      writeV[i].data, writeV[i].size);
         if (wrsz != writeV[i].size)
            {if (wrsz < 0) return wrsz;
            error.setErrInfo(ESPIPE,"write past eof");
            return SFS_ERROR;
           }
        totbytes += wrsz;
       }
   return totbytes;
}
