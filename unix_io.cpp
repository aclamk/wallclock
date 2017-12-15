#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <fcntl.h>
#include <poll.h>
#include "unix_io.h"

int UnixIO::server(uint64_t some_id)
{
  int serv_fd, cfd;
  struct sockaddr_un serv_addr;

  std::string unix_name{"@/wallclock/"};
  char hex[8*2+1];
  sprintf(hex, "%16.16lx", some_id);
  unix_name.append(hex);

  serv_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (serv_fd != -1)
  {
    memset(&serv_addr, 0, sizeof(struct sockaddr_un));
    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, unix_name.c_str(), sizeof(serv_addr.sun_path) - 1);
    serv_addr.sun_path[0] = '\0';

    if (bind(serv_fd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr_un)) != -1)
    {
      if (listen(serv_fd, 1) == 0)
      {
        return serv_fd;
      }
    }
    else
      close(serv_fd);
  }
  return -1;
}

int UnixIO::accept(int serv_fd)
{
  int flags;
  int conn_fd = -1;
  flags = fcntl(serv_fd, F_GETFL);
  if (fcntl(serv_fd, F_SETFL, flags | O_NONBLOCK) == 0)
  {
    struct sockaddr_un peer_addr;
    socklen_t peer_addr_size = sizeof(struct sockaddr_un);
    int i=0;
    do
    {
      conn_fd = ::accept(serv_fd, (struct sockaddr *) &peer_addr, &peer_addr_size);
      if (conn_fd == -1)
      {
        printf("errno=%d\n",errno);
        if (errno != EWOULDBLOCK)
          break;
        else
          usleep(100*1000);
      }
      i++;
    } while ((conn_fd == -1) && (i < 10));
  }

  fcntl(serv_fd, F_SETFL, flags);
  this->conn_fd = conn_fd;
  return conn_fd;
}

int UnixIO::connect(uint64_t some_id)
{
  int conn_fd;
  struct sockaddr_un conn_addr;

  std::string unix_name{"@/wallclock/"};
  char hex[8*2+1];
  sprintf(hex, "%16.16lx", some_id);
  unix_name.append(hex);
  conn_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (conn_fd != -1)
  {
    memset(&conn_addr, 0, sizeof(struct sockaddr_un));
    conn_addr.sun_family = AF_UNIX;
    strncpy(conn_addr.sun_path, unix_name.c_str(), sizeof(conn_addr.sun_path) - 1);
    conn_addr.sun_path[0] = '\0';
    if (::connect(conn_fd, (struct sockaddr *) &conn_addr, sizeof(struct sockaddr_un)) == 0)
    {
      this->conn_fd = conn_fd;
      return conn_fd;
    }
    else
      close(conn_fd);
  }
  return -1;
}

int UnixIO::wait_read()
{
  struct pollfd fds;
  fds.fd = conn_fd;
  fds.events = POLLIN;
  int r;
  r = poll(&fds, 1, 100);
  if (r == -1)
    r = -errno;
  return r;
}

bool UnixIO::read_bytes(void* ptr, size_t size)
{
  int res;
  res = ::read(conn_fd, ptr, size);
  return res == size;
}

bool UnixIO::write_bytes(const void* ptr, size_t size)
{
  int res;
  res = ::write(conn_fd, ptr, size);
  return res == size;
}

bool UnixIO::read(std::string& s)
{
  bool res = false;
  uint16_t size;
  if (read(size)) {
    char str[size];
    if (read_bytes(str, size)) {
      s = std::string(str, size);
      res = true;
    }
  }
  return res;
}

bool UnixIO::write(const std::string& s)
{
  bool res = false;
  uint16_t size = s.size();
  if (write(size)) {
    if (write_bytes(s.c_str(), size))
    {
      res = true;
    }
  }
  return res;
}




