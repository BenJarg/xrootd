//------------------------------------------------------------------------------
// Copyright (c) 2012-2013 by European Organization for Nuclear Research (CERN)
// Author: Justin Salmon <jsalmon@cern.ch>
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

#include "PyXRootD.hh"
#include "PyXRootDURL.hh"

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! Is the url valid
  //----------------------------------------------------------------------------
  PyObject* URL::IsValid( URL *self )
  {
    return Py_BuildValue( "O", PyBool_FromLong( self->url->IsValid() ) );
  }

  //----------------------------------------------------------------------------
  //! Get the host part of the URL (user:password\@host:port)
  //----------------------------------------------------------------------------
  PyObject* URL::GetHostId( URL *self, void *closure )
  {
    return Py_BuildValue( "S",
        PyUnicode_FromString( self->url->GetHostId().c_str() ) );
  }

  //----------------------------------------------------------------------------
  //! Get the protocol
  //----------------------------------------------------------------------------
  PyObject* URL::GetProtocol( URL *self, void *closure )
  {
    return Py_BuildValue( "S",
        PyUnicode_FromString( self->url->GetProtocol().c_str() ) );
  }

  //----------------------------------------------------------------------------
  //! Set protocol
  //----------------------------------------------------------------------------
  int URL::SetProtocol( URL *self, PyObject *protocol, void *closure )
  {
    if ( !PyString_Check( protocol ) ) {
      PyErr_SetString( PyExc_TypeError, "protocol must be string" );
      return -1;
    }

    self->url->SetProtocol( std::string ( PyString_AsString( protocol ) ) );
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Get the username
  //----------------------------------------------------------------------------
  PyObject* URL::GetUserName( URL *self, void *closure )
  {
    return Py_BuildValue( "S",
        PyUnicode_FromString( self->url->GetUserName().c_str() ) );
  }

  //----------------------------------------------------------------------------
  //! Set the username
  //----------------------------------------------------------------------------
  int URL::SetUserName( URL *self, PyObject *username, void *closure )
  {
    if ( !PyString_Check( username ) ) {
      PyErr_SetString( PyExc_TypeError, "username must be string" );
      return -1;
    }

    self->url->SetUserName( std::string( PyString_AsString( username ) ) );
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Get the password
  //----------------------------------------------------------------------------
  PyObject* URL::GetPassword( URL *self, void *closure )
  {
    return Py_BuildValue( "S",
        PyUnicode_FromString( self->url->GetPassword().c_str() ) );
  }

  //----------------------------------------------------------------------------
  //! Set the password
  //----------------------------------------------------------------------------
  int URL::SetPassword( URL *self, PyObject *password, void *closure )
  {
    if ( !PyString_Check( password ) ) {
      PyErr_SetString( PyExc_TypeError, "password must be string" );
      return -1;
    }

    self->url->SetPassword( std::string( PyString_AsString( password ) ) );
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Get the name of the target host
  //----------------------------------------------------------------------------
  PyObject* URL::GetHostName( URL *self, void *closure )
  {
    return Py_BuildValue( "S",
        PyUnicode_FromString( self->url->GetHostName().c_str() ) );
  }

  //----------------------------------------------------------------------------
  //! Set the host name
  //----------------------------------------------------------------------------
  int URL::SetHostName( URL *self, PyObject *hostname, void *closure )
  {
    if ( !PyString_Check( hostname ) ) {
      PyErr_SetString( PyExc_TypeError, "hostname must be string" );
      return -1;
    }

    self->url->SetHostName( std::string( PyString_AsString( hostname ) ) );
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Get the target port
  //----------------------------------------------------------------------------
  PyObject* URL::GetPort( URL *self, void *closure )
  {
    return Py_BuildValue( "i", self->url->GetPort() );
  }

  //----------------------------------------------------------------------------
  //! Is the url valid
  //----------------------------------------------------------------------------
  int URL::SetPort( URL *self, PyObject *port, void *closure )
  {
    if ( !PyInt_Check( port ) ) {
      PyErr_SetString( PyExc_TypeError, "port must be int" );
      return -1;
    }

    self->url->SetPort( PyInt_AsLong( port ) );
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Get the path
  //----------------------------------------------------------------------------
  PyObject* URL::GetPath( URL *self, void *closure )
  {
    return Py_BuildValue( "S",
        PyUnicode_FromString( self->url->GetPath().c_str() ) );
  }

  //----------------------------------------------------------------------------
  //! Set the path
  //----------------------------------------------------------------------------
  int URL::SetPath( URL *self, PyObject *path, void *closure )
  {
    if ( !PyString_Check( path ) ) {
      PyErr_SetString( PyExc_TypeError, "path must be string" );
      return -1;
    }

    self->url->SetPath( std::string( PyString_AsString( path ) ) );
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Get the path with params
  //----------------------------------------------------------------------------
  PyObject* URL::GetPathWithParams( URL *self, void *closure )
  {
    return Py_BuildValue( "S",
        PyUnicode_FromString( self->url->GetPathWithParams().c_str() ) );
  }

  //----------------------------------------------------------------------------
  //! Clear the url
  //----------------------------------------------------------------------------
  PyObject* URL::Clear( URL *self )
  {
    self->url->Clear();
    Py_RETURN_NONE ;
  }
}
