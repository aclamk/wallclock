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
  struct sockaddr_un serv_addr = {0};

  char unix_name[sizeof(serv_addr.sun_path)];
  memset(unix_name, 0, sizeof(unix_name));
  sprintf(unix_name, "@/wallclock/%d", some_id);

  serv_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (serv_fd != -1)
  {
    memset(&serv_addr, 0, sizeof(struct sockaddr_un));
    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, unix_name, sizeof(serv_addr.sun_path) - 1);
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
        if (errno != EWOULDBLOCK)
          break;
        else
          usleep(100*1000);
      }
      //i++;
    } while ((conn_fd == -1) && (i < 100));
  }

  fcntl(serv_fd, F_SETFL, flags);
  this->conn_fd = conn_fd;
  return conn_fd;
}

int UnixIO::connect(uint64_t some_id)
{
  int conn_fd;
  struct sockaddr_un conn_addr = {0};

  char unix_name[sizeof(conn_addr.sun_path)];
  memset(unix_name, 0, sizeof(unix_name));
  sprintf(unix_name, "@/wallclock/%d", some_id);
  conn_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (conn_fd != -1)
  {
    memset(&conn_addr, 0, sizeof(struct sockaddr_un));
    conn_addr.sun_family = AF_UNIX;
    strncpy(conn_addr.sun_path, unix_name, sizeof(conn_addr.sun_path) - 1);
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
  do {
    r = poll(&fds, 1, 100);
  } while (r == -1 && errno == EINTR);
  if (r == -1) {
    r = -errno;
  }
  return r;
}

bool UnixIO::read_bytes(void* ptr, size_t size)
{
  size_t cnt = 0;
  int res;
  do {
    res = ::read(conn_fd, (char*)ptr + cnt, size - cnt);
    if (res == -1) {
      printf("read res=%d errno=%d\n",res,errno);
      if (errno == EINTR) continue;
      return false;
    }
    if (res == 0)
      return false;
    cnt = cnt + res;
  } while (res != size);
  return true;
}

bool UnixIO::write_bytes(const void* ptr, size_t size)
{
  size_t cnt = 0;
  int res;
  do {
    res = ::write(conn_fd, (char*)ptr + cnt, size - cnt);
    if (res == -1) {
      printf("write res=%d errno=%d\n",res,errno);
      if (errno == EINTR) continue;
      return false;
    }
    if (res == 0)
      return false;
    cnt = cnt + res;
  } while (res != size);
  return true;
}

bool UnixIO::read(std::string& s)
{
  bool res = false;
  uint16_t size;
  if (read(size)) {
    if(size > 0) {
      char str[size];
      if (read_bytes(str, size)) {
        s = std::string(str, size);
        res = true;
      }
    } else {
      s.clear();
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
    if(size>0)
    {
      if (write_bytes(s.c_str(), size))
      {
        res = true;
      }
    }
    else
      res = true;
  }
  return res;
}




