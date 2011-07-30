/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


#pragma once

#include <sys/socket.h>
#include <cerrno>

namespace drizzle_plugin {

/**
 *@brief Virtual I/O layer, only used with TCP/IP sockets at the moment.
 */
class Vio
{
public:
  /**
   * Constructor. 
   * @param[in] sd Descriptor to use.
   */
  Vio(int sd);
  ~Vio();

  /**
   *Close the connection.
   *@returns 0 on success.
   */
  int close();

  /**
   * Read some data from the remote end.
   *@param[out] buf A buffer to write the new data to.
   *@param[in] size The size of the buffer
   *@returns The number of bytes read.
   */
  size_t read(unsigned char* buf, size_t size);
  
  /**
   * Write some data to the remote end.
   *@param[in] buf A buffer that contains the data to send.
   *@param[in] size The size of the buffer
   *@returns The number of bytes written.
   */
  size_t write(const unsigned char* buf, size_t size);

  /**
   * Set device blocking mode.
   *@param[in] set_blocking_mode Whether the device should block. true sets blocking mode, false clears it.
   *@param[out] old_mode This will be set to the previous blocking mode.
   *@returns 0 on success.
   */
  int blocking(bool set_blocking_mode, bool *old_mode);

  /**
   * Enables fast sending.
   * Setting this sets the TCP_NODELAY socket option.
   *@returns 0 on succcess.
   */
  int fastsend();

  /**
   * Sets or clears the keepalive option.
   *@param[in] set_keep_alive Whether to set or clear the flag. True Sets keepalive, false clears it.
   *@returns 0 on success.
   */
  int32_t keepalive(bool set_keep_alive);

  /**
   *@returns true if the caller should retry the last operation.
   */
  bool should_retry() const;

  /**
   *@returns true if the last operation was interrupted.
   */
  bool was_interrupted() const;

  /**
   *Gets the address details of the peer.
   @param[out] buf Buffer that will recieve the peer address.
   @param[out] port Port of remote end.
   @param[in] buflen Size of buf.
   @returns True on success, false otherwise.
   */
  bool peer_addr(char *buf, size_t buflen, uint16_t& port) const;

  /**
   * Sets either the send, or recieve timeouts for the socket.
   *@param[in] is_sndtimeo Set to true to change the send timeout, false to change the recieve timeout.
   *@param[in] timeout The new timeout to set, in seconds.
   */
  void timeout(bool is_sndtimeo, int32_t timeout);

  /**
   * Returns the last error code.
   *@returns the last error code, as described in errno.h
   */
  int get_errno() const;

  /**
   * Get the underlying descriptor this class is using.
   *@returns The descriptor passed in to the constructor of this class.
   */
  int get_fd() const;

private:
  int sd;
  int fcntl_mode; /* Buffered fcntl(sd,F_GETFL) */
};

} /* namespace drizzle_plugin */

