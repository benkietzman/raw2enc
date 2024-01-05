// vim: syntax=cpp
// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// raw2enc
// -------------------------------------
// file       : passthru.cpp
// author     : Ben Kietzman
// begin      : 2024-01-04
// copyright  : kietzman.org
// email      : ben@kietzman.org
///////////////////////////////////////////

/**************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
**************************************************************************/
// {{{ includes
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <locale.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
using namespace std;
#include <Central>
#include <SignalHandling>
#include <StringManip>
#include <Utility>
using namespace common;
// }}}
// {{{ defines
#ifdef VERSION
#undef VERSION
#endif
/*! \def VERSION
* \brief Contains the application version number.
*/
#define VERSION "0.1"
/*! \def mUSAGE(A)
* \brief Prints the usage statement.
*/
#define mUSAGE(A) cout << endl << "Usage:  "<< A << " [options]"  << endl << endl << " -d DATA, --data=DATA" << endl << "     Sets the data directory." << endl << endl << " -e EMAIL, --email=EMAIL" << endl << "     Provides the email address for default notifications." << endl << endl << " -h, --help" << endl << "     Displays this usage screen." << endl << endl << " -l PORT, --listening-port=PORT" << endl << "     Provides the listening port." << endl << endl << " -p PORT, --port=PORT" << endl << "     Provides the outbound port." << endl << endl << " -s SERVER, --server=SERVER" << endl << "     Provides the outbound server." << endl << endl << "     Displays the current version of this software." << endl << endl
/*! \def mVER_USAGE(A,B)
* \brief Prints the version number.
*/
#define mVER_USAGE(A,B) cout << endl << A << " Version: " << B << endl << endl
// }}}
// {{{ structs
struct connection
{
  bool bCloseIn;
  bool bCloseOut;
  bool bRetry;
  int fdIn;
  int fdOut;
  SSL *ssl;
  string strBuffers[2];
};
// }}}
// {{{ global variables
bool gbShutdown = false;
Central *gpCentral;
// }}}
// {{{ prototypes
void sighandle(const int nSignal);
// }}}
// {{{ main()
int main(int argc, char *argv[])
{
  // {{{ prep work
  string strData = "/data/raw2enc", strEmail, strError, strListeningPort = "7376", strPort = "443", strPrefix = "main()", strServer = "localhost";
  stringstream ssMessage;
  StringManip manip;
  Utility utility(strError);
  setlocale(LC_ALL, "");
  gpCentral = new Central(strError);
  // }}}
  // {{{ set signal handling
  sethandles(sighandle);
  signal(SIGBUS, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);
  signal(SIGCONT, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGSEGV, SIG_IGN);
  signal(SIGWINCH, SIG_IGN);
  // }}}
  // {{{ command line arguments
  for (int i = 1; i < argc; i++)
  {
    string strArg = argv[i];
    if (strArg == "-d" || (strArg.size() > 7 && strArg.substr(0, 7) == "--data="))
    {
      if (strArg == "-d" && i + 1 < argc && argv[i+1][0] != '-')
      {
        strData = argv[++i];
      }
      else
      {
        strData = strArg.substr(7, strArg.size() - 7);
      }
      manip.purgeChar(strData, strData, "'");
      manip.purgeChar(strData, strData, "\"");
    }
    else if (strArg == "-e" || (strArg.size() > 8 && strArg.substr(0, 8) == "--email="))
    {
      if (strArg == "-e" && i + 1 < argc && argv[i+1][0] != '-')
      {
        strEmail = argv[++i];
      }
      else
      {
        strEmail = strArg.substr(8, strArg.size() - 8);
      }
      manip.purgeChar(strEmail, strEmail, "'");
      manip.purgeChar(strEmail, strEmail, "\"");
    }
    else if (strArg == "-h" || strArg == "--help")
    {
      mUSAGE(argv[0]);
      return 0;
    }
    else if (strArg == "-l" || (strArg.size() > 17 && strArg.substr(0, 17) == "--listening-port="))
    {
      if (strArg == "-l" && i + 1 < argc && argv[i+1][0] != '-')
      {
        strListeningPort = argv[++i];
      }
      else
      {
        strListeningPort = strArg.substr(17, strArg.size() - 17);
      }
      manip.purgeChar(strListeningPort, strListeningPort, "'");
      manip.purgeChar(strListeningPort, strListeningPort, "\"");
    }
    else if (strArg == "-p" || (strArg.size() > 7 && strArg.substr(0, 7) == "--port="))
    {
      if (strArg == "-p" && i + 1 < argc && argv[i+1][0] != '-')
      {
        strPort = argv[++i];
      }
      else
      {
        strPort = strArg.substr(7, strArg.size() - 7);
      }
      manip.purgeChar(strPort, strPort, "'");
      manip.purgeChar(strPort, strPort, "\"");
    }
    else if (strArg == "-s" || (strArg.size() > 9 && strArg.substr(0, 9) == "--server="))
    {
      if (strArg == "-s" && i + 1 < argc && argv[i+1][0] != '-')
      {
        strServer = argv[++i];
      }
      else
      {
        strServer = strArg.substr(9, strArg.size() - 9);
      }
      manip.purgeChar(strServer, strPort, "'");
      manip.purgeChar(strServer, strPort, "\"");
    }
    else if (strArg == "-v" || strArg == "--version")
    {
      mVER_USAGE(argv[0], VERSION);
      return 0;
    }
    else
    {
      cout << endl << "Illegal option, '" << strArg << "'." << endl;
      mUSAGE(argv[0]);
      return 0;
    }
  }
  // }}}
  // {{{ normal run
  if (!strData.empty() && !strEmail.empty() && !strListeningPort.empty() && !strPort.empty() && !strServer.empty())
  {
    // {{{ prep work
    SSL_CTX *ctx;
    gpCentral->setEmail(strEmail);
    gpCentral->setLog(strData, "passhthru_", "monthly", true, true);
    // }}}
    if ((ctx = utility.sslInitClient(strError)) != NULL)
    {
      // {{{ prep work
      addrinfo hints, *result;
      bool bBound[3] = {false, false, false};
      int fdSocket, nReturn;
      memset(&hints, 0, sizeof(addrinfo));
      hints.ai_family = AF_INET6;
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_flags = AI_PASSIVE;
      if ((nReturn = getaddrinfo(NULL, strListeningPort.c_str(), &hints, &result)) == 0)
      {
        addrinfo *rp;
        bBound[0] = true;
        for (rp = result; !bBound[2] && rp != NULL; rp = rp->ai_next)
        {
          bBound[1] = false;
          if ((fdSocket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) >= 0)
          {
            int nOn = 1;
            bBound[1] = true;
            setsockopt(fdSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&nOn, sizeof(nOn));
            if (bind(fdSocket, rp->ai_addr, rp->ai_addrlen) == 0)
            {
              bBound[2] = true;
            }
            else
            {
              close(fdSocket);
            }
          }
        }
        freeaddrinfo(result);
      }
      // }}}
      if (bBound[2])
      {
        // {{{ prep work
        ssMessage.str("");
        ssMessage << strPrefix << "->bind():  Bound incoming socket.";
        gpCentral->log(ssMessage.str());
        // }}}
        if (listen(fdSocket, SOMAXCONN) == 0)
        {
          // {{{ prep work
          bool bExit = false, bIn = false;
          list<connection *> conns;
          list<connection *>::iterator connIter;
          list<list<connection *>::iterator> removals;
          pollfd *fds;
          size_t unIndex;
          ssMessage.str("");
          ssMessage << strPrefix << "->listen():  Listening to incoming socket.";
          gpCentral->log(ssMessage.str());
          // }}}
          while (!gbShutdown && !bExit)
          {
            // {{{ prep work
            fds = new pollfd[conns.size()*2+1];
            unIndex = 0;
            fds[unIndex].fd = fdSocket;
            fds[unIndex].events = POLLIN;
            unIndex++;
            for (auto &conn : conns)
            {
              fds[unIndex].fd = conn->fdIn;
              fds[unIndex].events = POLLIN;
              if (!conn->strBuffers[0].empty())
              {
                fds[unIndex].events |= POLLOUT;
              }
              unIndex++;
              fds[unIndex].fd = conn->fdOut;
              fds[unIndex].events = POLLIN;
              if (!conn->strBuffers[1].empty())
              {
                fds[unIndex].events |= POLLOUT;
              }
              unIndex++;
              if (!conn->bCloseOut && (conn->ssl == NULL || conn->bRetry))
              {
                if (conn->ssl == NULL)
                {
                  if ((conn->ssl = utility.sslConnect(ctx, conn->fdOut, conn->bRetry, strError)) == NULL)
                  {
                    conn->bCloseOut = true;
                  }
                }
                else if ((nReturn = SSL_connect(conn->ssl)) == 1)
                {
                  conn->bRetry = false;
                }
                else
                {
                  utility.sslstrerror(conn->ssl, nReturn, conn->bRetry);
                  if (!conn->bRetry)
                  {
                    conn->bCloseOut = true;
                  }
                }
              }
            }
            // }}}
            if ((nReturn = poll(fds, unIndex, 2000)) > 0)
            {
              // {{{ accept
              if (fds[0].revents & POLLIN)
              {
                int fdClient;
                sockaddr_in cli_addr;
                socklen_t clilen = sizeof(cli_addr);
                if ((fdClient = accept(fds[0].fd, (sockaddr *)&cli_addr, &clilen)) >= 0)
                {
                  connection *ptConn = new connection;
                  ptConn->bCloseIn = false;
                  ptConn->bCloseOut = false;
                  ptConn->bRetry = false;
                  ptConn->fdIn = fdClient;
                  ptConn->fdOut = -1;
                  ptConn->ssl = NULL;
                  conns.push_back(ptConn);
                }
                else
                {
                  bExit = true;
                  ssMessage.str("");
                  ssMessage << strPrefix << "->accept(" << errno << ") error [" << fds[0].fd << "]:  " << strerror(errno);
                  gpCentral->log(ssMessage.str());
                }
              }
              // }}}
              // {{{ connections
              for (size_t i = 1; i < unIndex; i++)
              {
                // {{{ prep work
                bIn = false;
                connIter = conns.end();
                for (auto j = conns.begin(); connIter == conns.end() && j != conns.end(); j++)
                {
                  if (fds[i].fd == (*j)->fdIn || fds[i].fd == (*j)->fdOut)
                  {
                    connIter = j;
                    if (fds[i].fd == (*j)->fdIn)
                    {
                      bIn = true;
                    }
                  }
                }
                // }}}
                if (connIter != conns.end())
                {
                  // {{{ inbound socket
                  if (bIn)
                  {
                    // {{{ read
                    if (fds[i].revents & POLLIN)
                    {
                      if (!utility.fdRead(fds[i].fd, (*connIter)->strBuffers[1], nReturn))
                      {
                        (*connIter)->bCloseIn = true;
                      }
                    }
                    // }}}
                    // {{{ write
                    if (fds[i].revents & POLLOUT)
                    {
                      if (utility.fdWrite(fds[i].fd, (*connIter)->strBuffers[0], nReturn))
                      {
                        if ((*connIter)->strBuffers[0].empty() && (*connIter)->bCloseOut)
                        {
                          (*connIter)->bCloseIn = true;
                        }
                      }
                      else
                      {
                        (*connIter)->bCloseIn = true;
                      }
                    }
                    // }}}
                  }
                  // }}}
                  // {{{ outbound socket
                  else
                  {
                    // {{{ read
                    if (fds[i].revents & POLLIN)
                    {
                      if (!utility.sslRead((*connIter)->ssl, (*connIter)->strBuffers[0], nReturn))
                      {
                        (*connIter)->bCloseOut = true;
                      }
                    }
                    // }}}
                    // {{{ write
                    if ((*connIter)->fdOut != -1 && fds[i].revents & POLLOUT)
                    {
                      if (utility.sslWrite((*connIter)->ssl, (*connIter)->strBuffers[1], nReturn))
                      {
                        if ((*connIter)->strBuffers[1].empty() && (*connIter)->bCloseIn)
                        {
                          (*connIter)->bCloseOut = true;
                        }
                      }
                      else
                      {
                        (*connIter)->bCloseOut = true;
                      }
                    }
                    // }}}
                  }
                  // }}}
                }
              }
              // }}}
            }
            else if (nReturn < 0 && errno != EINTR)
            {
              bExit = true;
              ssMessage.str("");
              ssMessage << strPrefix << "->poll(" << errno << ") error:  " << strerror(errno);
            }
            // {{{ post work
            delete[] fds;
            for (auto i = conns.begin(); i != conns.end(); i++)
            {
              if ((*i)->bCloseIn && (*i)->fdIn != -1)
              {
                close((*i)->fdIn);
                (*i)->fdIn = -1;
              }
              if ((*i)->bCloseOut && (*i)->fdOut != -1)
              {
                if ((*i)->ssl != NULL)
                {
                  SSL_shutdown((*i)->ssl);
                  SSL_free((*i)->ssl);
                  (*i)->ssl = NULL;
                }
                close((*i)->fdOut);
                (*i)->fdOut = -1;
              }
              if ((*i)->bCloseIn && (*i)->fdIn == -1 && (*i)->bCloseOut && (*i)->fdOut == -1)
              {
                removals.push_back(i);
              }
            }
            while (!removals.empty())
            {
              delete (*removals.front());
              conns.erase(removals.front());
              removals.pop_front();
            }
            // }}}
          }
          // {{{ post work
          while (!conns.empty())
          {
            if (conns.front()->fdIn != -1)
            {
              close(conns.front()->fdIn);
            }
            if (conns.front()->fdOut != -1)
            {
              if (conns.front()->ssl != NULL)
              {
                SSL_shutdown(conns.front()->ssl);
                SSL_free(conns.front()->ssl);
              }
              close(conns.front()->fdOut);
            }
            delete conns.front();
            conns.pop_front();
          }
          // }}}
        }
        else
        {
          ssMessage.str("");
          ssMessage << strPrefix << "->listen(" << errno << ") error:  " << strerror(errno);
          gpCentral->log(ssMessage.str());
        }
        // {{{ post work
        close(fdSocket);
        ssMessage.str("");
        ssMessage << strPrefix << "->close():  Closed incoming socket.";
        gpCentral->log(ssMessage.str());
        // }}}
      }
      else
      {
        ssMessage.str("");
        ssMessage << strPrefix << "->";
        if (!bBound[0])
        {
          ssMessage << "getaddrinfo(" << nReturn << ") error:  " << gai_strerror(nReturn);
        }
        else
        {
          ssMessage << ((!bBound[1])?"socket":"bind") << "(" << errno << ") error:  " << strerror(errno);
        }
        gpCentral->log(ssMessage.str());
      }
    }
    else
    {
      ssMessage.str("");
      ssMessage << strPrefix << "->Utility::sslInitClient() error:  " << strError;
      gpCentral->log(ssMessage.str());
    }
    // {{{ post work
    utility.sslDeinit();
    delete gpCentral;
    // }}}
  }
  // }}}
  // {{{ usage statement
  else
  {
    mUSAGE(argv[0]);
  }
  // }}}

  return 0;
}
// }}}
// {{{ sighandle()
void sighandle(const int nSignal)
{
  string strSignal;
  stringstream ssMessage;

  sethandles(sigdummy);
  gbShutdown = true;
  ssMessage << "sighandle(" << nSignal << "):  " << sigstring(strSignal, nSignal);
  gpCentral->log(ssMessage.str());
}
// }}}
