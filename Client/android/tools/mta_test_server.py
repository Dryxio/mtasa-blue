#!/usr/bin/env python3
"""
MTA:SA Test Server - With RakNet 3.x and RakNet 4 Handshake Support

This test server simulates an MTA server including both RakNet 3.x (MTA protocol)
and RakNet 4 connection handshakes, allowing full testing of the Android client.

Usage:
    python3 mta_test_server.py [port]

Default port: 22003
"""

import socket
import struct
import sys
import time
import random
from datetime import datetime

# RakNet 4 Packet IDs (for backward compatibility with test clients)
ID_OPEN_CONNECTION_REQUEST_1 = 0x05
ID_OPEN_CONNECTION_REPLY_1 = 0x06
ID_OPEN_CONNECTION_REQUEST_2 = 0x07
ID_OPEN_CONNECTION_REPLY_2 = 0x08
ID_CONNECTION_REQUEST = 0x09
ID_CONNECTION_REQUEST_ACCEPTED = 0x10

# MTA RakNet 3.x Packet IDs (from packetenums.h)
MTA_RID_CONNECTION_REQUEST = 0x04          # Client -> Server: connection request
MTA_RID_OPEN_CONNECTION_REQUEST = 0x09     # Client -> Server: initial contact
MTA_RID_OPEN_CONNECTION_REPLY = 0x0A       # Server -> Client: reply to open
MTA_RID_CONNECTION_REQUEST_ACCEPTED = 0x0E # Server -> Client: accepted

# RakNet magic bytes (RakNet 4 only)
RAKNET_MAGIC = bytes([0x00, 0xff, 0xff, 0x00, 0xfe, 0xfe, 0xfe, 0xfe,
                      0xfd, 0xfd, 0xfd, 0xfd, 0x12, 0x34, 0x56, 0x78])

# MTA Packet IDs
PACKET_ID_SERVER_JOIN = 0x00
PACKET_ID_SERVER_JOIN_DATA = 0x01
PACKET_ID_SERVER_JOIN_COMPLETE = 0x02
PACKET_ID_PLAYER_JOINDATA = 0x01
PACKET_ID_SERVER_JOINEDGAME = 0x16  # 22
PACKET_ID_MOD_NAME = 0x1C  # 28

# Server config
DEFAULT_PORT = 22003
BITSTREAM_VERSION = 0x06B  # Latest MTA bitstream version
SERVER_VERSION = "1.6.0"

SERVER_GUID = random.randint(1, 2**63)

def log(msg):
    """Print timestamped log message"""
    timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"[{timestamp}] {msg}")

#=============================================================================
# RakNet Handshake Packets
#=============================================================================

def create_open_connection_reply_1(mtu_size):
    """
    Create OPEN_CONNECTION_REPLY_1 packet
    Format:
        - uint8: packet ID (0x06)
        - bytes[16]: RakNet magic
        - uint64: server GUID
        - uint8: use security (0 = no)
        - uint16: MTU size
    """
    packet = bytearray()
    packet.append(ID_OPEN_CONNECTION_REPLY_1)
    packet.extend(RAKNET_MAGIC)
    packet.extend(struct.pack('>Q', SERVER_GUID))  # big-endian uint64
    packet.append(0)  # No security
    packet.extend(struct.pack('>H', mtu_size))  # big-endian uint16
    return bytes(packet)

def create_open_connection_reply_2(client_addr, mtu_size):
    """
    Create OPEN_CONNECTION_REPLY_2 packet
    Format:
        - uint8: packet ID (0x08)
        - bytes[16]: RakNet magic
        - uint64: server GUID
        - bytes[7]: client address (1 byte family + 4 bytes IP + 2 bytes port)
        - uint16: MTU size
        - uint8: use security (0 = no)
    """
    packet = bytearray()
    packet.append(ID_OPEN_CONNECTION_REPLY_2)
    packet.extend(RAKNET_MAGIC)
    packet.extend(struct.pack('>Q', SERVER_GUID))

    # Write client address (RakNet format: family + inverted IP + port)
    packet.append(4)  # AF_INET
    ip_parts = client_addr[0].split('.')
    for part in ip_parts:
        packet.append(255 - int(part))  # RakNet inverts IP bytes
    packet.extend(struct.pack('>H', client_addr[1]))  # port big-endian

    packet.extend(struct.pack('>H', mtu_size))
    packet.append(0)  # No security
    return bytes(packet)

def create_connection_request_accepted(client_addr, client_guid):
    """
    Create CONNECTION_REQUEST_ACCEPTED packet
    Format:
        - uint8: packet ID (0x10)
        - bytes[7]: client address
        - uint16: system index
        - bytes[7 * 10]: internal addresses (10 addresses)
        - uint64: request time (from client)
        - uint64: reply time
    """
    packet = bytearray()
    packet.append(ID_CONNECTION_REQUEST_ACCEPTED)

    # Client address
    packet.append(4)  # AF_INET
    ip_parts = client_addr[0].split('.')
    for part in ip_parts:
        packet.append(255 - int(part))
    packet.extend(struct.pack('>H', client_addr[1]))

    # System index (0)
    packet.extend(struct.pack('>H', 0))

    # Internal addresses (10 addresses, use 127.0.0.1 for all)
    for _ in range(10):
        packet.append(4)  # AF_INET
        packet.extend([128, 255, 255, 254])  # 127.0.0.1 inverted
        packet.extend(struct.pack('>H', 0))  # port 0

    # Request time (we'll use current time as placeholder)
    current_time = int(time.time() * 1000) & 0xFFFFFFFFFFFFFFFF
    packet.extend(struct.pack('>Q', current_time))

    # Reply time
    packet.extend(struct.pack('>Q', current_time))

    return bytes(packet)

#=============================================================================
# MTA RakNet 3.x Packets
#=============================================================================

def create_mta_open_connection_reply(cookie):
    """
    Create MTA RakNet 3.x OPEN_CONNECTION_REPLY packet
    Format:
        - uint8: packet ID (0x0A)
        - uint32: cookie (little-endian, echo what client sent)
    """
    packet = bytearray()
    packet.append(MTA_RID_OPEN_CONNECTION_REPLY)
    packet.extend(struct.pack('<I', cookie))  # little-endian uint32
    return bytes(packet)

def create_mta_connection_request_accepted(client_addr):
    """
    Create MTA RakNet 3.x CONNECTION_REQUEST_ACCEPTED packet
    Format:
        - uint8: packet ID (0x0E)
        - bytes[7]: client address
        - uint16: system index
        - bytes[70]: internal addresses (10 * 7 bytes)
        - uint64: request time
        - uint64: reply time
    """
    packet = bytearray()
    packet.append(MTA_RID_CONNECTION_REQUEST_ACCEPTED)

    # Client address (RakNet format)
    packet.append(4)  # AF_INET
    ip_parts = client_addr[0].split('.')
    for part in ip_parts:
        packet.append(255 - int(part))
    packet.extend(struct.pack('>H', client_addr[1]))

    # System index
    packet.extend(struct.pack('>H', 0))

    # Internal addresses (10 addresses)
    for _ in range(10):
        packet.append(4)
        packet.extend([128, 255, 255, 254])  # 127.0.0.1 inverted
        packet.extend(struct.pack('>H', 0))

    # Timestamps
    current_time = int(time.time() * 1000) & 0xFFFFFFFFFFFFFFFF
    packet.extend(struct.pack('>Q', current_time))
    packet.extend(struct.pack('>Q', current_time))

    return bytes(packet)

def parse_mta_open_connection_request(data):
    """Parse MTA RakNet 3.x OPEN_CONNECTION_REQUEST and return cookie"""
    if len(data) < 5:
        return None
    # Format: ID (1) + cookie (4, little-endian)
    cookie = struct.unpack('<I', data[1:5])[0]
    log(f"   MTA cookie: 0x{cookie:08x}")
    return cookie

def parse_mta_connection_request(data):
    """Parse MTA RakNet 3.x CONNECTION_REQUEST and return info"""
    if len(data) < 18:
        return None
    # Format: ID (1) + GUID (8) + timestamp (8) + has_security (1)
    client_guid = struct.unpack('>Q', data[1:9])[0]
    timestamp = struct.unpack('>Q', data[9:17])[0]
    log(f"   MTA client GUID: 0x{client_guid:016x}, timestamp: {timestamp}")
    return {'guid': client_guid, 'timestamp': timestamp}

def is_mta_raknet3_packet(data):
    """Check if packet is MTA RakNet 3.x (no magic bytes after ID)"""
    if len(data) < 2:
        return False
    # RakNet 4 packets have magic bytes at offset 1
    # MTA RakNet 3.x packets don't have magic
    if len(data) >= 17 and data[1:17] == RAKNET_MAGIC:
        return False
    # MTA OPEN_CONNECTION_REQUEST is ID 0x09 with a 4-byte cookie
    if data[0] == MTA_RID_OPEN_CONNECTION_REQUEST and len(data) == 5:
        return True
    # MTA CONNECTION_REQUEST is ID 0x04
    if data[0] == MTA_RID_CONNECTION_REQUEST:
        return True
    return False

#=============================================================================
# RakNet 4 Packet Parsing
#=============================================================================

def parse_open_connection_request_1(data):
    """Parse OPEN_CONNECTION_REQUEST_1 and return MTU size"""
    # Packet format: ID (1) + MAGIC (16) + PROTOCOL_VERSION (1) + padding to MTU
    if len(data) < 18:
        return None

    # Check magic
    if data[1:17] != RAKNET_MAGIC:
        log(f"   Invalid magic in REQUEST_1")
        return None

    protocol_version = data[17]
    mtu_size = len(data)  # MTU is the packet size

    log(f"   Protocol version: {protocol_version}, MTU: {mtu_size}")
    return mtu_size

def parse_open_connection_request_2(data):
    """Parse OPEN_CONNECTION_REQUEST_2 and return info"""
    if len(data) < 34:
        return None, None

    # Check magic
    if data[1:17] != RAKNET_MAGIC:
        return None, None

    # Skip to client GUID and MTU
    # Format: ID (1) + MAGIC (16) + server addr (7) + MTU (2) + client GUID (8)
    offset = 17
    # Skip server address (7 bytes)
    offset += 7
    # MTU
    mtu = struct.unpack('>H', data[offset:offset+2])[0]
    offset += 2
    # Client GUID
    client_guid = struct.unpack('>Q', data[offset:offset+8])[0]

    return mtu, client_guid

def create_mod_name_packet():
    """
    Create MOD_NAME packet (0x1C)
    Format:
        - uint8: packet ID (0x1C)
        - uint16: bitstream version
        - uint16: string length
        - chars: module name ("deathmatch")
    """
    packet = bytearray()
    packet.append(PACKET_ID_MOD_NAME)

    # Bitstream version (little-endian uint16)
    packet.extend(struct.pack('<H', BITSTREAM_VERSION))

    # Module name as length-prefixed string (MTA format)
    module_name = b"deathmatch"
    packet.extend(struct.pack('<H', len(module_name)))  # Length prefix
    packet.extend(module_name)

    return bytes(packet)

def create_join_complete_packet():
    """
    Create SERVER_JOIN_COMPLETE packet (0x02)
    Format:
        - uint8: packet ID
        - uint16: version string length
        - chars: version string
        - string: full version (length-prefixed or fixed)
    """
    packet = bytearray()
    packet.append(PACKET_ID_SERVER_JOIN_COMPLETE)

    # Version string
    version = SERVER_VERSION.encode('utf-8')
    packet.extend(struct.pack('<H', len(version)))
    packet.extend(version)

    # Full version info (simplified)
    full_version = f"MTA:SA Server v{SERVER_VERSION} (Test)".encode('utf-8')
    packet.extend(struct.pack('<H', len(full_version)))
    packet.extend(full_version)

    return bytes(packet)

def create_joined_game_packet(player_id=1):
    """
    Create SERVER_JOINEDGAME packet (0x16)
    Format:
        - uint8: packet ID
        - uint16: player ID
        - uint8: player count
        - uint16: root element ID
    """
    packet = bytearray()
    packet.append(PACKET_ID_SERVER_JOINEDGAME)

    # Player ID (little-endian uint16)
    packet.extend(struct.pack('<H', player_id))

    # Player count
    packet.append(1)

    # Root element ID
    packet.extend(struct.pack('<H', 1))

    return bytes(packet)

def parse_join_data(data):
    """Parse PLAYER_JOINDATA packet and extract info"""
    if len(data) < 10:
        return None

    try:
        offset = 1  # Skip packet ID

        # Netcode version (uint16)
        netcode_version = struct.unpack_from('<H', data, offset)[0]
        offset += 2

        # MTA version (uint16)
        mta_version = struct.unpack_from('<H', data, offset)[0]
        offset += 2

        # Bitstream version (uint16)
        bitstream_version = struct.unpack_from('<H', data, offset)[0]
        offset += 2

        # Version string (length-prefixed)
        # MTA uses a custom string format - try to read it
        # For simplicity, skip to nickname which is at a known offset

        # Skip version string and other fields to get nickname
        # The exact offset depends on the version string length
        # For testing, we'll just acknowledge the packet

        return {
            'netcode_version': hex(netcode_version),
            'mta_version': hex(mta_version),
            'bitstream_version': hex(bitstream_version),
        }
    except Exception as e:
        log(f"Error parsing join data: {e}")
        return None

def dump_packet(data, direction="<-"):
    """Hex dump a packet for debugging"""
    hex_str = data.hex()
    # Format as groups of 2 (bytes)
    hex_formatted = ' '.join(hex_str[i:i+2] for i in range(0, min(len(hex_str), 64), 2))
    if len(hex_str) > 64:
        hex_formatted += " ..."
    return hex_formatted

def run_server(port):
    """Run the test server with RakNet handshake support"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    try:
        sock.bind(('0.0.0.0', port))
    except OSError as e:
        log(f"Failed to bind to port {port}: {e}")
        sys.exit(1)

    log(f"=" * 60)
    log(f"MTA:SA Test Server (With RakNet 3.x + RakNet 4 Handshake)")
    log(f"Listening on UDP port {port}")
    log(f"Bitstream version: {hex(BITSTREAM_VERSION)}")
    log(f"Server GUID: {SERVER_GUID}")
    log(f"Supports: MTA RakNet 3.x (0x09->0x0A->0x04->0x0E)")
    log(f"          RakNet 4 (0x05->0x06->0x07->0x08->0x09->0x10)")
    log(f"=" * 60)
    log(f"Waiting for Android client connection...")
    log("")

    clients = {}  # Track client states
    next_player_id = 1

    while True:
        try:
            data, addr = sock.recvfrom(4096)
            client_key = f"{addr[0]}:{addr[1]}"

            if not data:
                continue

            log(f"<- [{client_key}] Received {len(data)} bytes: {dump_packet(data)}")

            # Get or create client state
            if client_key not in clients:
                clients[client_key] = {
                    'state': 'init',
                    'protocol': None,  # 'mta3' or 'rn4'
                    'player_id': next_player_id,
                    'packets_received': 0,
                    'mtu': 1200,
                    'client_guid': 0,
                    'cookie': 0
                }
                next_player_id += 1

            client = clients[client_key]
            client['packets_received'] += 1
            packet_id = data[0] if data else 0

            #=================================================================
            # MTA RakNet 3.x Handshake
            #=================================================================

            # MTA OPEN_CONNECTION_REQUEST (0x09 with 4-byte cookie, no magic)
            if is_mta_raknet3_packet(data) and packet_id == MTA_RID_OPEN_CONNECTION_REQUEST:
                cookie = parse_mta_open_connection_request(data)
                if cookie is not None:
                    client['protocol'] = 'mta3'
                    client['cookie'] = cookie
                    reply = create_mta_open_connection_reply(cookie)
                    sock.sendto(reply, addr)
                    log(f"-> [{client_key}] Sent MTA OPEN_CONNECTION_REPLY (cookie: 0x{cookie:08x})")
                    client['state'] = 'mta_wait_conn_req'
                continue

            # MTA CONNECTION_REQUEST (0x04)
            if client.get('protocol') == 'mta3' and packet_id == MTA_RID_CONNECTION_REQUEST:
                conn_info = parse_mta_connection_request(data)
                if conn_info:
                    client['client_guid'] = conn_info['guid']
                    reply = create_mta_connection_request_accepted(addr)
                    sock.sendto(reply, addr)
                    log(f"-> [{client_key}] Sent MTA CONNECTION_REQUEST_ACCEPTED")
                    client['state'] = 'mta_connected'

                    # After RakNet handshake, send MOD_NAME
                    time.sleep(0.05)
                    mod_name = create_mod_name_packet()
                    sock.sendto(mod_name, addr)
                    log(f"-> [{client_key}] Sent MOD_NAME")
                    client['state'] = 'wait_join'
                continue

            #=================================================================
            # RakNet 4 Handshake States
            #=================================================================

            # OPEN_CONNECTION_REQUEST_1 (0x05 with magic)
            if packet_id == ID_OPEN_CONNECTION_REQUEST_1:
                mtu_size = parse_open_connection_request_1(data)
                if mtu_size:
                    client['protocol'] = 'rn4'
                    client['mtu'] = min(mtu_size, 1492)  # Cap MTU
                    reply = create_open_connection_reply_1(client['mtu'])
                    sock.sendto(reply, addr)
                    log(f"-> [{client_key}] Sent OPEN_CONNECTION_REPLY_1 (MTU: {client['mtu']})")
                    client['state'] = 'raknet_2'
                continue

            # OPEN_CONNECTION_REQUEST_2 (0x07)
            elif packet_id == ID_OPEN_CONNECTION_REQUEST_2:
                mtu, client_guid = parse_open_connection_request_2(data)
                if mtu:
                    client['mtu'] = mtu
                    client['client_guid'] = client_guid
                    reply = create_open_connection_reply_2(addr, mtu)
                    sock.sendto(reply, addr)
                    log(f"-> [{client_key}] Sent OPEN_CONNECTION_REPLY_2 (client GUID: {client_guid})")
                    client['state'] = 'raknet_3'
                continue

            # CONNECTION_REQUEST (0x09)
            elif packet_id == ID_CONNECTION_REQUEST:
                log(f"   Received CONNECTION_REQUEST")
                reply = create_connection_request_accepted(addr, client.get('client_guid', 0))
                sock.sendto(reply, addr)
                log(f"-> [{client_key}] Sent CONNECTION_REQUEST_ACCEPTED")
                client['state'] = 'raknet_connected'

                # After RakNet handshake, send MOD_NAME
                time.sleep(0.05)
                mod_name = create_mod_name_packet()
                sock.sendto(mod_name, addr)
                log(f"-> [{client_key}] Sent MOD_NAME")
                client['state'] = 'wait_join'
                continue

            #=================================================================
            # MTA Protocol States
            #=================================================================

            # Handle legacy direct connection (no RakNet)
            if client['state'] == 'raknet_1' and packet_id not in [ID_OPEN_CONNECTION_REQUEST_1]:
                # Legacy client - skip RakNet handshake
                log(f"   Legacy client (no RakNet), sending MOD_NAME...")
                mod_name = create_mod_name_packet()
                sock.sendto(mod_name, addr)
                log(f"-> [{client_key}] Sent MOD_NAME ({len(mod_name)} bytes)")
                client['state'] = 'wait_join'
                continue

            # Waiting for PLAYER_JOINDATA
            if client['state'] == 'wait_join':
                if packet_id == PACKET_ID_PLAYER_JOINDATA:
                    log(f"   Received PLAYER_JOINDATA")
                    join_info = parse_join_data(data)
                    if join_info:
                        log(f"   Client info: {join_info}")

                    # Send JOIN_COMPLETE
                    time.sleep(0.05)
                    join_complete = create_join_complete_packet()
                    sock.sendto(join_complete, addr)
                    log(f"-> [{client_key}] Sent JOIN_COMPLETE")

                    # Send JOINED_GAME
                    time.sleep(0.05)
                    joined_game = create_joined_game_packet(client['player_id'])
                    sock.sendto(joined_game, addr)
                    log(f"-> [{client_key}] Sent JOINED_GAME (player ID: {client['player_id']})")

                    client['state'] = 'connected'
                    log(f"")
                    log(f"   *** CLIENT CONNECTED SUCCESSFULLY ***")
                    log(f"")
                else:
                    # Unexpected packet, resend MOD_NAME
                    log(f"   Unexpected packet {hex(packet_id)} in wait_join, resending MOD_NAME...")
                    mod_name = create_mod_name_packet()
                    sock.sendto(mod_name, addr)
                continue

            # Client is fully connected
            if client['state'] == 'connected':
                log(f"   Connected client sent packet {hex(packet_id)}")
                # Could handle sync packets, chat, etc. here

        except KeyboardInterrupt:
            log("")
            log("Server shutting down...")
            break
        except Exception as e:
            log(f"Error: {e}")
            import traceback
            traceback.print_exc()
            continue

    sock.close()

if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_PORT
    run_server(port)
