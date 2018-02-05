/*
 * unix_io.h
 *
 *  Created on: Dec 14, 2017
 *      Author: adam
 */

#ifndef UNIX_IO_H_
#define UNIX_IO_H_

#include <string>
#include <type_traits>

class UnixIO
{
public:
  int server(uint64_t some_id);
  int accept(int serv_fd);
  int connect(uint64_t some_id);

  int wait_read();
  bool read_bytes(void* ptr, size_t size);
  bool write_bytes(const void* ptr, size_t size);
  template <typename T>
  bool read(T& data) {
    static_assert(std::is_trivial<T>::value,"");
    bool res = read_bytes(&data, sizeof(T));
    //printf("read res=%d size=%d %ld\n",res, sizeof(T), (long long)data);
    return res;
  }
  template <typename T>
  bool write(const T& data) {
    static_assert(std::is_trivial<T>::value,"");
    bool res = write_bytes(&data, sizeof(T));
    //printf("write res=%d size=%d %ld\n",res, sizeof(T), (long long)data);
    return res;
  }
  bool read(std::string& s);
  bool write(const std::string& s);

private:
  int conn_fd{-1};
};




#endif /* UNIX_IO_H_ */
