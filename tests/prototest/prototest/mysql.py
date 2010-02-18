#!/usr/bin/env python
#
# Drizzle Client & Protocol Library
# 
# Copyright (C) 2008 Eric Day (eday@oddments.org)
# All rights reserved.
# 
# Use and distribution licensed under the BSD license.  See
# the COPYING file in this directory for full text.
#
'''
MySQL Protocol Objects

Objects in this module can be initialized by passing in either a raw
buffer to parse or keywords. This allows you to use these objects
for either sending or receiving.
'''

import struct
import unittest
import bitfield

class Capabilities(bitfield.BitField):
  _fields = [
    'LONG_PASSWORD',
    'FOUND_ROWS',
    'LONG_FLAG',
    'CONNECT_WITH_DB',
    'NO_SCHEMA',
    'COMPRESS',
    'ODBC',
    'LOCAL_FILES',
    'IGNORE_SPACE',
    'PROTOCOL_41',
    'INTERACTIVE',
    'SSL',
    'IGNORE_SIGPIPE',
    'TRANSACTIONS',
    'RESERVED',
    'SECURE_CONNECTION',
    'MULTI_STATEMENTS',
    'MULTI_RESULTS',
    None,
    None,
    None,
    None,
    None,
    None,
    None,
    None,
    None,
    None,
    None,
    None,
    'SSL_VERIFY_SERVER_CERT',
    'REMEMBER_OPTIONS'
  ]

class Status(bitfield.BitField):
  _fields = [
    'IN_TRANS',
    'AUTOCOMMIT',
    'MORE_RESULTS_EXISTS',
    'QUERY_NO_GOOD_INDEX_USED',
    'QUERY_NO_INDEX_USED',
    'CURSOR_EXISTS',
    'LAST_ROW_SENT',
    'DB_DROPPED',
    'NO_BACKSLASH_ESCAPES',
    'QUERY_WAS_SLOW'
  ]

class CommandID(object):
  SLEEP = 0
  QUIT = 1
  INIT_DB = 2
  QUERY = 3
  FIELD_LIST = 4
  CREATE_DB = 5
  DROP_DB = 6
  REFRESH = 7
  SHUTDOWN = 8
  STATISTICS = 9
  PROCESS_INFO = 10
  CONNECT = 11
  PROCESS_KILL = 12
  DEBUG = 13
  PING = 14
  TIME = 15
  DELAYED_INSERT = 16
  CHANGE_USER = 17
  BINLOG_DUMP = 18
  TABLE_DUMP = 19
  CONNECT_OUT = 20
  REGISTER_SLAVE = 21
  STMT_PREPARE = 22
  STMT_EXECUTE = 23
  STMT_SEND_LONG_DATA = 24
  STMT_CLOSE = 25
  STMT_RESET = 26
  SET_OPTION = 27
  STMT_FETCH = 28
  DAEMON = 29
  END = 30

class PacketException(Exception):
  pass

class Packet(object):
  '''This class represents a packet header.'''

  def __init__(self, packed=None, size=0, sequence=0):
    if packed is None:
      self.size = size
      self.sequence = sequence
    else:
      data = struct.unpack('4B', packed)
      self.size = data[0] | (data[1] << 8) | (data[2] << 16)
      self.sequence = data[3]

    self.verify()

  def pack(self):
    self.verify()
    return struct.pack('4B',
                       self.size & 0xFF,
                       (self.size >> 8) & 0xFF,
                       (self.size >> 16) & 0xFF,
                       self.sequence % 256)

  def verify(self):
    if self.size >= 16777216:
      raise PacketException('Packet size cannot exceed 16777215 bytes (%d)' %
                            self.size)

  def __str__(self):
    return '''Packet
  size = %s
  sequence = %s
''' % (self.size, self.sequence)

class TestPacket(unittest.TestCase):

  def testDefaultInit(self):
    packet = Packet()
    self.assertEqual(packet.size, 0)
    self.assertEqual(packet.sequence, 0)

  def testKeywordInit(self):
    packet = Packet(size=1234, sequence=5)
    self.assertEqual(packet.size, 1234)
    self.assertEqual(packet.sequence, 5)

  def testUnpackInit(self):
    packet = Packet(struct.pack('4B', 210, 4, 0, 5))
    self.assertEqual(packet.size, 1234)
    self.assertEqual(packet.sequence, 5)

  def testPack(self):
    packet = Packet(Packet(size=1234, sequence=5).pack())
    self.assertEqual(packet.size, 1234)

  def testPackRange(self):
    for x in range(0, 300):
      packet = Packet(Packet(size=x, sequence=x).pack())
      self.assertEqual(packet.size, x)
      self.assertEqual(packet.sequence, x % 256)

    # 997 is a random prime number so we hit various increments
    for x in range(300, 16777216, 997):
      packet = Packet(Packet(size=x, sequence=x).pack())
      self.assertEqual(packet.size, x)
      self.assertEqual(packet.sequence, x % 256)

    packet = Packet(Packet(size=16777215).pack())
    self.assertEqual(packet.size, 16777215)
    self.assertEqual(packet.sequence, 0)

    self.assertRaises(PacketException, Packet, size=16777216)
    self.assertRaises(PacketException, Packet, size=16777217)
    self.assertRaises(PacketException, Packet, size=4294967295)
    self.assertRaises(PacketException, Packet, size=4294967296)
    self.assertRaises(PacketException, Packet, size=4294967297)

class ServerHandshake(object):
  '''This class represents the initial handshake sent from server to client.'''

  def __init__(self, packed=None, protocol_version=10, server_version='',
               thread_id=0, scramble=tuple([0] * 20), null1=0, capabilities=0,
               charset=0, status=0, unused=tuple([0] * 13), null2=0):
    if packed is None:
      self.protocol_version = protocol_version
      self.server_version = server_version
      self.thread_id = thread_id
      self.scramble = scramble
      self.null1 = null1
      self.capabilities = Capabilities(capabilities)
      self.charset = charset
      self.status = Status(status)
      self.unused = unused
      self.null2 = null2
    else:
      self.protocol_version = struct.unpack('B', packed[:1])[0]
      server_version_length = packed[1:].index('\x00')
      self.server_version = packed[1:1+server_version_length]
      data = struct.unpack('<I8BB2BB2B13B12BB', packed[2+server_version_length:])
      self.thread_id = data[0]
      self.scramble = data[1:9] + data[28:40]
      self.null1 = data[9]
      self.capabilities = Capabilities(data[10] | (data[11] << 8))
      self.charset = data[12]
      self.status = Status(data[13] | (data[14] << 8))
      self.unused = data[15:28]
      self.null2 = data[40]

  def pack(self):
    data = struct.pack('B', self.protocol_version)
    data += self.server_version + '\x00'
    data += struct.pack('<I', self.thread_id)
    data += ''.join(map(chr, self.scramble[:8]))
    data += struct.pack('B2BB2B',
                       self.null1,
                       self.capabilities.value() & 0xFF,
                       (self.capabilities.value() >> 8) & 0xFF,
                       self.charset,
                       self.status.value() & 0xFF,
                       (self.status.value() >> 8) & 0xFF)
    data += ''.join(map(chr, self.unused))
    data += ''.join(map(chr, self.scramble[8:]))
    data += struct.pack('B', self.null2)
    return data

  def __str__(self):
    return '''ServerHandshake
  protocol_version = %s
  server_version = %s
  thread_id = %s
  scramble = %s
  null1 = %s
  capabilities = %s
  charset = %s
  status = %s
  unused = %s
  null2 = %s
''' % (self.protocol_version, self.server_version, self.thread_id,
       self.scramble, self.null1, self.capabilities, self.charset,
       self.status, self.unused, self.null2)

class TestServerHandshake(unittest.TestCase):

  def testDefaultInit(self):
    handshake = ServerHandshake()
    self.verifyDefault(handshake)

  def testKeywordInit(self):
    handshake = ServerHandshake(protocol_version=11,
                                server_version='test',
                                thread_id=1234,
                                scramble=tuple([5] * 20),
                                null1=1,
                                capabilities=65279,
                                charset=253,
                                status=64508,
                                unused=tuple([6] * 13),
                                null2=2)
    self.verifyCustom(handshake)

  def testUnpackInit(self):
    data = struct.pack('B', 11)
    data += 'test\x00'
    data += struct.pack('<I', 1234)
    data += ''.join([chr(5)] * 8)
    data += struct.pack('B2BB2B', 1, 255, 254, 253, 252, 251)
    data += ''.join([chr(6)] * 13)
    data += ''.join([chr(5)] * 12)
    data += struct.pack('B', 2)

    handshake = ServerHandshake(data)
    self.verifyCustom(handshake)

  def testPack(self):
    handshake = ServerHandshake(ServerHandshake().pack())
    self.verifyDefault(handshake)

  def verifyDefault(self, handshake):
    self.assertEqual(handshake.protocol_version, 10)
    self.assertEqual(handshake.server_version, '')
    self.assertEqual(handshake.thread_id, 0)
    self.assertEqual(handshake.scramble, tuple([0] * 20))
    self.assertEqual(handshake.null1, 0)
    self.assertEqual(handshake.capabilities.value(), 0)
    self.assertEqual(handshake.charset, 0)
    self.assertEqual(handshake.status.value(), 0)
    self.assertEqual(handshake.unused, tuple([0] * 13))
    self.assertEqual(handshake.null2, 0)

  def verifyCustom(self, handshake):
    self.assertEqual(handshake.protocol_version, 11)
    self.assertEqual(handshake.server_version, 'test')
    self.assertEqual(handshake.thread_id, 1234)
    self.assertEqual(handshake.scramble, tuple([5] * 20))
    self.assertEqual(handshake.null1, 1)
    self.assertEqual(handshake.capabilities.value(), 65279)
    self.assertEqual(handshake.charset, 253)
    self.assertEqual(handshake.status.value(), 64508)
    self.assertEqual(handshake.unused, tuple([6] * 13))
    self.assertEqual(handshake.null2, 2)

class ClientHandshake(object):
  '''This class represents the client handshake sent back to the server.'''

  def __init__(self, packed=None, capabilities=0, max_packet_size=0, charset=0,
               unused=tuple([0] * 23), user='', scramble_size=0,
               scramble=None, db=''):
    if packed is None:
      self.capabilities = Capabilities(capabilities)
      self.max_packet_size = max_packet_size
      self.charset = charset
      self.unused = unused
      self.user = user
      self.scramble_size = scramble_size
      self.scramble = scramble
      self.db = db
    else:
      data = struct.unpack('<IIB23B', packed[:32])
      self.capabilities = Capabilities(data[0])
      self.max_packet_size = data[1]
      self.charset = data[2]
      self.unused = data[3:]
      packed = packed[32:]
      user_length = packed.index('\x00')
      self.user = packed[:user_length]
      packed = packed[1+user_length:]
      self.scramble_size = ord(packed[0])
      if self.scramble_size == 0:
        self.scramble = None
      else:
        self.scramble = tuple(map(ord, packed[1:21]))
      if packed[-1:] == '\x00':
        self.db = packed[21:-1]
      else:
        self.db = packed[21:]

  def pack(self):
    data = struct.pack('<IIB', 
                       self.capabilities.value(),
                       self.max_packet_size,
                       self.charset)
    data += ''.join(map(chr, self.unused))
    data += self.user + '\x00'
    data += chr(self.scramble_size)
    if self.scramble_size != 0:
      data += ''.join(map(chr, self.scramble))
    data += self.db + '\x00'
    return data

  def __str__(self):
    return '''ClientHandshake
  capabilities = %s
  max_packet_size = %s
  charset = %s
  unused = %s
  user = %s
  scramble_size = %s
  scramble = %s
  db = %s
''' % (self.capabilities, self.max_packet_size, self.charset, self.unused,
       self.user, self.scramble_size, self.scramble, self.db)

class TestClientHandshake(unittest.TestCase):

  def testDefaultInit(self):
    handshake = ClientHandshake()
    self.verifyDefault(handshake)

  def testKeywordInit(self):
    handshake = ClientHandshake(capabilities=65279,
                                max_packet_size=64508,
                                charset=253,
                                unused=tuple([6] * 23),
                                user='user',
                                scramble_size=20,
                                scramble=tuple([5] * 20),
                                db='db')
    self.verifyCustom(handshake)

  def testUnpackInit(self):
    data = struct.pack('<IIB', 65279, 64508, 253)
    data += ''.join([chr(6)] * 23)
    data += 'user\x00'
    data += chr(20)
    data += ''.join([chr(5)] * 20)
    data += 'db\x00'

    handshake = ClientHandshake(data)
    self.verifyCustom(handshake)

  def testPack(self):
    handshake = ClientHandshake(ClientHandshake().pack())
    self.verifyDefault(handshake)

  def verifyDefault(self, handshake):
    self.assertEqual(handshake.capabilities.value(), 0)
    self.assertEqual(handshake.max_packet_size, 0)
    self.assertEqual(handshake.charset, 0)
    self.assertEqual(handshake.unused, tuple([0] * 23))
    self.assertEqual(handshake.user, '')
    self.assertEqual(handshake.scramble_size, 0)
    self.assertEqual(handshake.scramble, None)
    self.assertEqual(handshake.db, '')

  def verifyCustom(self, handshake):
    self.assertEqual(handshake.capabilities.value(), 65279)
    self.assertEqual(handshake.max_packet_size, 64508)
    self.assertEqual(handshake.charset, 253)
    self.assertEqual(handshake.unused, tuple([6] * 23))
    self.assertEqual(handshake.user, 'user')
    self.assertEqual(handshake.scramble_size, 20)
    self.assertEqual(handshake.scramble, tuple([5] * 20))
    self.assertEqual(handshake.db, 'db')

class Result(object):
  '''This class represents a result packet sent from the server.'''

  def __init__(self, packed=None, field_count=0, affected_rows=0, insert_id=0,
               status=0, warning_count=0, message='', version_40=False):
    if packed is None:
      self.field_count = field_count
      self.affected_rows = affected_rows
      self.insert_id = insert_id
      self.status = status
      self.warning_count = warning_count
      self.message = message
      self.version_40 = version_40
    else:
      if version_40 is True:
        self.field_count = ord(packed[0])
        if self.field_count == 0:
          self.affected_rows = ord(packed[1])
          self.insert_id = ord(packed[2])
          if len(packed) == 3:
            self.status = 0
          else:
            data = struct.unpack('<H', packed[2:])
            self.status = data[0]
        elif self.field_count == 255:
          data = struct.unpack('<H', packed[1:3])
          self.error_code = data[0]
          self.message = packed[3:]
        else:
          self.affected_rows = ord(packed[1])
          self.insert_id = ord(packed[2])
          data = struct.unpack('<HH', packed[3:7])
          self.status = data[0]
          self.warning_count = data[1]
          self.message = packed[7:]
      else:
        self.field_count = ord(packed[0])
        if self.field_count == 255:
          data = struct.unpack('<H', packed[1:3])
          self.error_code = data[0]
          self.sqlstate_marker = packed[3]
          self.sqlstate = packed[4:9]
          self.message = packed[9:]
        else:
          self.affected_rows = ord(packed[1])
          self.insert_id = ord(packed[2])
          data = struct.unpack('<HH', packed[3:7])
          self.status = data[0]
          self.warning_count = data[1]
          self.message = packed[7:]

      self.version_40 = version_40

  def __str__(self):
    if self.version_40 is True:
      if self.field_count == 255:
        return '''Result
  field_count = %s
  error_code = %s
  message = %s
  version_40 = %s
''' % (self.field_count, self.error_code, self.message, self.version_40)
      else:
        return '''Result
  field_count = %s
  affected_rows = %s
  insert_id = %s
  status = %s
  version_40 = %s
''' % (self.field_count, self.affected_rows, self.insert_id, self.status,
       self.version_40)
    else:
      if self.field_count == 255:
        return '''Result
  field_count = %s
  error_code = %s
  sqlstate_marker = %s
  sqlstate = %s
  message = %s
  version_40 = %s
''' % (self.field_count, self.error_code, self.sqlstate_marker, sqlstate,
       self.message, self.version_40)
      else:
        return '''Result
  field_count = %s
  affected_rows = %s
  insert_id = %s
  status = %s
  warning_count = %s
  message = %s
  version_40 = %s
''' % (self.field_count, self.affected_rows, self.insert_id, self.status,
       self.warning_count, self.message, self.version_40)

# TODO testResult

class Command(object):
  '''This class represents a command packet sent from the client.'''

  def __init__(self, packed=None, command=CommandID.SLEEP, payload=''):
    if packed is None:
      self.command = command
      self.payload = payload
    else:
      self.command = ord(packed[0])
      self.payload = packed[1:]

  def pack(self):
    return chr(self.command) + self.payload

  def __str__(self):
    return '''Command
  command = %s
  payload = %s
''' % (self.command, self.payload)

class TestCommand(unittest.TestCase):

  def testDefaultInit(self):
    command = Command()
    self.assertEqual(command.command, CommandID.SLEEP)
    self.assertEqual(command.payload, '')

  def testKeywordInit(self):
    command = Command(command=CommandID.QUERY, payload='abc')
    self.assertEqual(command.command, CommandID.QUERY)
    self.assertEqual(command.payload, 'abc')

  def testUnpackInit(self):
    command = Command('\x03abc')
    self.assertEqual(command.command, CommandID.QUERY)
    self.assertEqual(command.payload, 'abc')

  def testPack(self):
    command = Command(Command(command=CommandID.QUERY, payload='abc').pack())
    self.assertEqual(command.command, CommandID.QUERY)
    self.assertEqual(command.payload, 'abc')

class QueryCommand(Command):
  def __init__(self, packed=None, query=''):
    super(QueryCommand, self).__init__(packed=packed, command=CommandID.QUERY,
                                       payload=query)

  def __str__(self):
    return '''Command
  command = %s
  query = %s
''' % (self.command, self.payload)

class TestQueryCommand(unittest.TestCase):

  def testDefaultInit(self):
    query = QueryCommand()
    self.assertEqual(query.command, CommandID.QUERY)
    self.assertEqual(query.payload, '')

  def testKeywordInit(self):
    query = QueryCommand(query='abc')
    self.assertEqual(query.command, CommandID.QUERY)
    self.assertEqual(query.payload, 'abc')

  def testUnpackInit(self):
    query = QueryCommand('\x03abc')
    self.assertEqual(query.command, CommandID.QUERY)
    self.assertEqual(query.payload, 'abc')

  def testPack(self):
    query = QueryCommand(QueryCommand(query='abc').pack())
    self.assertEqual(query.command, CommandID.QUERY)
    self.assertEqual(query.payload, 'abc')

if __name__ == '__main__':
  unittest.main()
