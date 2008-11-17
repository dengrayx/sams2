/***************************************************************************
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

using namespace std;

#include "urlgroup.h"
#include "url.h"
#include "proxy.h"

#include "debug.h"

UrlGroup::UrlGroup (const long &id, const UrlGroup::accessType &access)
{
  DEBUG (DEBUG7, "[" << this << "->" << __FUNCTION__ << "(" << id << ")]");

  _id = id;
  _type = access;
}

UrlGroup::~UrlGroup ()
{
  DEBUG (DEBUG7, "[" << this << "->" << __FUNCTION__ << "]");
  _list.clear ();

#ifdef USE_PCRECPP
  vector<pcrecpp::RE*>::iterator it;
  for (it = _patterns.begin (); it != _patterns.end (); it++)
    {
      delete (*it);
    }
  _patterns.clear ();
#else
#  ifdef USE_PCRE
  _patterns.clear ();
#  endif
#endif
}

long UrlGroup::getId ()
{
  DEBUG (DEBUG8, "[" << this << "->" << __FUNCTION__ << "] = " << _id);
  return _id;
}

UrlGroup::accessType UrlGroup::getAccessType ()
{
//  DEBUG (DEBUG8, "[" << this << "->" << __FUNCTION__ << "] = ...");
  return _type;
}

void UrlGroup::addUrl (const string & url)
{
  DEBUG (DEBUG8, "[" << this << "->" << __FUNCTION__ << "(" << url << ")]");

  if (_type == UrlGroup::ACC_REGEXP || _type == UrlGroup::ACC_REPLACE || _type == UrlGroup::ACC_REDIR)
    {
#ifdef USE_PCRECPP
      pcrecpp::RE *re;

      re = new pcrecpp::RE(url);
      if (!re->error().empty ())
        {
          WARNING ("Mailformed regexp pattern " << url << " " << re->error ());
          delete re;
        }
      else
        {
          _patterns.push_back (re);
          /// @TODO Убрать использование переменной _list если _type=ACC_REGEXP, но для этого подправить asString()
          _list.push_back (url);
        }
#else
#  ifdef USE_PCRE
      int erroroffset;
      const char *error;
      pcre *re;

      re = pcre_compile(url.c_str (), 0, &error, &erroroffset, NULL);
      if (re == NULL)
        {
          WARNING ("Mailformed regexp pattern " << url);
        }
      else
        {
          _patterns.push_back (re);
          _list.push_back (url);
        }
#  endif
#endif
    }
  else
    _list.push_back (url);
}

bool UrlGroup::hasUrl (const string & url) const
{
  if (_type == UrlGroup::ACC_REGEXP || _type == UrlGroup::ACC_REPLACE || _type == UrlGroup::ACC_REDIR)
    {
      uint idx;
#ifndef USE_PCRECPP
#  ifdef USE_PCRE
      int ovector[300];
#  endif
#endif
      for (idx = 0; idx < _patterns.size (); idx++)
        {
#ifdef USE_PCRECPP
          if (_patterns[idx]->FullMatch (url))
            {
              DEBUG (DEBUG4, "[" << this << "] Found rule " << _patterns[idx]->pattern () << " for " << url);
              return true;
            }
#else
#  ifdef USE_PCRE
          if (pcre_exec (_patterns[idx], NULL, url.c_str (), url.size (), 0, 0, ovector, 300) >= 0)
            {
              DEBUG (DEBUG4, "[" << this << "] Found rule " << _list[idx] << " for " << url);
              return true;
            }
#  endif
#endif
        }
      return false;
    }
  else
    {
      Url u;
      u.setUrl (url);

      string domain = u.getAddress ();

      vector<string>::const_iterator it;
      for (it = _list.begin (); it != _list.end (); it++)
        {
          if (_type == UrlGroup::ACC_ALLOW || _type == UrlGroup::ACC_DENY)
            {
              // Если сеть определена как www.mail.ru, то mail.ru никак не может быть хостом в этой сети
              if ((*it).size () > domain.size ())
                continue;

              if (domain.compare (domain.size ()-(*it).size (), (*it).size (), (*it)) == 0)
                {
                  DEBUG (DEBUG4, "[" << this << "] Host " << domain << " is part of net " << *it);
                  return true;
                }
            }
        }
      return false;
    }
}

void UrlGroup::setReplacement (const string & dest)
{
  DEBUG (DEBUG8, "[" << this << "->" << __FUNCTION__ << "(" << dest << ")]");
  _destination = dest;
}

string UrlGroup::modifyUrl (const string & url) const
{
  DEBUG (DEBUG8, "[" << this << "->" << __FUNCTION__ << "(" << url << ")]");

  string res = "";
  if (hasUrl (url))
    {
      switch (_type)
        {
          case ACC_DENY:
          case ACC_ALLOW:
          case ACC_REGEXP:
            break;
          case ACC_REDIR:
            res = Proxy::getRedirectAddr ();
            break;
          case ACC_REPLACE:
            res = "301:" + _destination;
            break;
        }
    }
  return res;
}

string UrlGroup::asString () const
{
  string res = "";
  vector<string>::const_iterator it;

  for (it = _list.begin (); it != _list.end (); it++)
    {
      if ( ! res.empty () )
        res += " ";
      res += (*it);
    }

  return res;
}

ostream & operator<< (ostream & out, const UrlGroup & grp)
{
  out << grp.asString();
  return out;
}
