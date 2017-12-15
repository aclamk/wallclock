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
    return read_bytes(&data, sizeof(T));
  }
  template <typename T>
  bool write(const T& data) {
    static_assert(std::is_trivial<T>::value,"");
    return write_bytes(&data, sizeof(T));
  }
  /*
  bool read(uint16_t& data);
  bool write(uint16_t data);
  bool read(uint32_t& data);
  bool write(uint32_t data);
  bool read(uint64_t& data);
  bool write(uint64_t data);
*/
  bool read(std::string& s);
  bool write(const std::string& s);

private:
  int conn_fd{-1};
};




#endif /* UNIX_IO_H_ */
