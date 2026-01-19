/*
 * MTA:SA Android - Packet Handler Implementation
 *
 * Phase 7: Multiplayer Logic
 */

#include "CPacketHandler.h"
#include "SyncStructures.h"

#include <android/log.h>
#include <algorithm>
#include <chrono>
#include <cstring>

#define LOG_TAG "MTA-PacketHandler"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace MTA::Android::Network
{
static uint64_t GetCurrentTimeMs()
{
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

CPacketHandler::CPacketHandler() = default;
CPacketHandler::~CPacketHandler() = default;

bool CPacketHandler::Initialize(CNetAndroid* network)
{
    if (!network)
    {
        LOGE("CPacketHandler: Invalid network pointer");
        return false;
    }

    m_network = network;

    // Register ourselves as the packet handler
    m_network->RegisterPacketHandler([this](PacketID id, NetBitStream& bitStream) {
        return ProcessPacket(id, bitStream);
    });

    LOGI("CPacketHandler: Initialized");
    return true;
}

void CPacketHandler::Shutdown()
{
    m_network = nullptr;
    LOGI("CPacketHandler: Shutdown");
}

void CPacketHandler::SetCallbacks(const PacketHandlerCallbacks& callbacks)
{
    m_callbacks = callbacks;
}

bool CPacketHandler::ProcessPacket(PacketID packetId, NetBitStream& bitStream)
{
    LOGD("CPacketHandler: Processing packet ID %d", static_cast<int>(packetId));

    switch (packetId)
    {
        // Connection packets
        case PacketID::SERVER_JOIN_COMPLETE:
            Packet_ServerJoinComplete(bitStream);
            return true;

        case PacketID::SERVER_JOINEDGAME:
            Packet_ServerJoined(bitStream);
            return true;

        case PacketID::SERVER_DISCONNECTED:
            Packet_ServerDisconnected(bitStream);
            return true;

        // Player packets
        case PacketID::PLAYER_LIST:
            Packet_PlayerList(bitStream);
            return true;

        case PacketID::PLAYER_JOIN:
            Packet_PlayerJoin(bitStream);
            return true;

        case PacketID::PLAYER_QUIT:
            Packet_PlayerQuit(bitStream);
            return true;

        case PacketID::PLAYER_SPAWN:
            Packet_PlayerSpawn(bitStream);
            return true;

        case PacketID::PLAYER_WASTED:
            Packet_PlayerWasted(bitStream);
            return true;

        case PacketID::PLAYER_CHANGE_NICK:
            Packet_PlayerChangeNick(bitStream);
            return true;

        // Sync packets
        case PacketID::PLAYER_PURESYNC:
            Packet_PlayerPureSync(bitStream);
            return true;

        case PacketID::PLAYER_KEYSYNC:
            Packet_PlayerKeySync(bitStream);
            return true;

        case PacketID::PLAYER_VEHICLE_PURESYNC:
            Packet_PlayerVehicleSync(bitStream);
            return true;

        case PacketID::RETURN_SYNC:
            Packet_ReturnSync(bitStream);
            return true;

        case PacketID::LIGHTSYNC:
            Packet_LightSync(bitStream);
            return true;

        // Vehicle packets
        case PacketID::VEHICLE_SPAWN:
            Packet_VehicleSpawn(bitStream);
            return true;

        case PacketID::VEHICLE_INOUT:
            Packet_VehicleInOut(bitStream);
            return true;

        case PacketID::VEHICLE_DAMAGE_SYNC:
            Packet_VehicleDamageSync(bitStream);
            return true;

        case PacketID::UNOCCUPIED_VEHICLE_SYNC:
            Packet_UnoccupiedVehicleSync(bitStream);
            return true;

        // Entity packets
        case PacketID::ENTITY_ADD:
            Packet_EntityAdd(bitStream);
            return true;

        case PacketID::ENTITY_REMOVE:
            Packet_EntityRemove(bitStream);
            return true;

        // Map packets
        case PacketID::MAP_INFO:
            Packet_MapInfo(bitStream);
            return true;

        case PacketID::MAP_START:
            Packet_MapStart(bitStream);
            return true;

        // Chat/Console
        case PacketID::CHAT_ECHO:
            Packet_ChatEcho(bitStream);
            return true;

        case PacketID::CONSOLE_ECHO:
            Packet_ConsoleEcho(bitStream);
            return true;

        case PacketID::DEBUG_ECHO:
            Packet_DebugEcho(bitStream);
            return true;

        // RPC
        case PacketID::RPC:
            Packet_RPC(bitStream);
            return true;

        // Lua
        case PacketID::LUA_EVENT:
            Packet_LuaEvent(bitStream);
            return true;

        // Resource
        case PacketID::RESOURCE_START:
            Packet_ResourceStart(bitStream);
            return true;

        case PacketID::RESOURCE_STOP:
            Packet_ResourceStop(bitStream);
            return true;

        // Sync settings
        case PacketID::SYNC_SETTINGS:
            Packet_SyncSettings(bitStream);
            return true;

        default:
            LOGW("CPacketHandler: Unhandled packet ID %d", static_cast<int>(packetId));
            return false;
    }
}

//=============================================================================
// Packet Senders
//=============================================================================

void CPacketHandler::SendJoinRequest(const std::string& nickname, const std::string& serial,
                                      const std::string& password)
{
    if (!m_network) return;

    auto bitStream = m_network->AllocateBitStream();

    // Protocol version
    bitStream->Write(static_cast<uint16_t>(Protocol::PROTOCOL_VERSION));

    // Player nickname
    bitStream->Write(nickname);

    // Serial
    bitStream->Write(serial);

    // Password (empty if none)
    bitStream->Write(password);

    // MTA version info
    bitStream->Write(std::string("1.6.0-android"));

    // Send join packet
    m_network->SendPacket(PacketID::SERVER_JOIN, *bitStream,
                          Protocol::PRIORITY_HIGH, Protocol::RELIABILITY_RELIABLE);

    m_localNickname = nickname;
    LOGI("CPacketHandler: Sent join request for '%s'", nickname.c_str());
}

void CPacketHandler::SendChatMessage(const std::string& message)
{
    if (!m_network) return;

    auto bitStream = m_network->AllocateBitStream();
    bitStream->Write(message);

    m_network->SendPacket(PacketID::COMMAND, *bitStream,
                          Protocol::PRIORITY_HIGH, Protocol::RELIABILITY_RELIABLE);

    LOGD("CPacketHandler: Sent chat message: %s", message.c_str());
}

void CPacketHandler::SendSpawnRequest(uint16_t skinId)
{
    if (!m_network) return;

    auto bitStream = m_network->AllocateBitStream();
    bitStream->Write(skinId);

    m_network->SendPacket(PacketID::PLAYER_SPAWN, *bitStream,
                          Protocol::PRIORITY_HIGH, Protocol::RELIABILITY_RELIABLE);

    LOGI("CPacketHandler: Sent spawn request with skin %d", skinId);
}

void CPacketHandler::SendPlayerSync(float x, float y, float z, float rotation,
                                     float vx, float vy, float vz, bool onGround)
{
    if (!m_network) return;

    // Rate limiting
    uint64_t now = GetCurrentTimeMs();
    if (now - m_lastSyncTime < m_syncIntervalMs)
    {
        return;
    }
    m_lastSyncTime = now;

    auto bitStream = m_network->AllocateBitStream();

    // Time context (for ordering)
    bitStream->Write(static_cast<uint16_t>(now & 0xFFFF));

    // Flags
    uint16_t flags = 0;
    if (onGround) flags |= 0x0002;
    bitStream->Write(flags);

    // Position (compressed)
    bitStream->Write(x);
    bitStream->Write(y);
    bitStream->Write(z);

    // Rotation
    bitStream->Write(rotation);

    // Velocity (if moving)
    bool hasVelocity = (vx != 0.0f || vy != 0.0f || vz != 0.0f);
    bitStream->WriteBit(hasVelocity);
    if (hasVelocity)
    {
        bitStream->Write(vx);
        bitStream->Write(vy);
        bitStream->Write(vz);
    }

    // Health and armor
    bitStream->Write(static_cast<uint8_t>(100));  // Health (0-255)
    bitStream->Write(static_cast<uint8_t>(0));    // Armor (0-255)

    // Current weapon
    bitStream->Write(static_cast<uint8_t>(0));    // Weapon ID
    bitStream->Write(static_cast<uint16_t>(0));   // Ammo

    m_network->SendPacket(PacketID::PLAYER_PURESYNC, *bitStream,
                          Protocol::PRIORITY_MEDIUM, Protocol::RELIABILITY_UNRELIABLE_SEQUENCED);
}

void CPacketHandler::SendKeySync(uint16_t keys, float aimX, float aimY)
{
    if (!m_network) return;

    auto bitStream = m_network->AllocateBitStream();

    // Time context
    bitStream->Write(static_cast<uint16_t>(GetCurrentTimeMs() & 0xFFFF));

    // Key states
    bitStream->Write(keys);

    // Aim direction
    bitStream->Write(aimX);
    bitStream->Write(aimY);

    m_network->SendPacket(PacketID::PLAYER_KEYSYNC, *bitStream,
                          Protocol::PRIORITY_MEDIUM, Protocol::RELIABILITY_UNRELIABLE_SEQUENCED);
}

void CPacketHandler::SendVehicleSync(uint32_t vehicleId, float x, float y, float z,
                                      float rotX, float rotY, float rotZ, float health)
{
    if (!m_network) return;

    auto bitStream = m_network->AllocateBitStream();

    // Time context
    bitStream->Write(static_cast<uint16_t>(GetCurrentTimeMs() & 0xFFFF));

    // Vehicle ID (element ID format)
    bitStream->Write(vehicleId);

    // Position
    bitStream->Write(x);
    bitStream->Write(y);
    bitStream->Write(z);

    // Rotation (Euler angles)
    bitStream->Write(rotX);
    bitStream->Write(rotY);
    bitStream->Write(rotZ);

    // Velocity (placeholder)
    bitStream->Write(0.0f);
    bitStream->Write(0.0f);
    bitStream->Write(0.0f);

    // Turn speed (placeholder)
    bitStream->Write(0.0f);
    bitStream->Write(0.0f);
    bitStream->Write(0.0f);

    // Health
    bitStream->Write(health);

    m_network->SendPacket(PacketID::PLAYER_VEHICLE_PURESYNC, *bitStream,
                          Protocol::PRIORITY_MEDIUM, Protocol::RELIABILITY_UNRELIABLE_SEQUENCED);
}

void CPacketHandler::SendRPC(RPCFunction function, NetBitStream& data)
{
    if (!m_network) return;

    auto bitStream = m_network->AllocateBitStream();

    // RPC function ID
    bitStream->Write(static_cast<uint8_t>(function));

    // Copy RPC data
    int bytesToCopy = data.GetBytesUsed();
    if (bytesToCopy > 0)
    {
        bitStream->Write(reinterpret_cast<const char*>(data.GetData()), bytesToCopy);
    }

    m_network->SendPacket(PacketID::RPC, *bitStream,
                          Protocol::PRIORITY_HIGH, Protocol::RELIABILITY_RELIABLE);
}

void CPacketHandler::SendCoreRPC(uint8_t functionId)
{
    if (!m_network) return;

    auto bitStream = m_network->AllocateBitStream();
    bitStream->Write(functionId);

    m_network->SendPacket(PacketID::RPC, *bitStream,
                          Protocol::PRIORITY_HIGH, Protocol::RELIABILITY_RELIABLE);
}

//=============================================================================
// Packet Handlers - Connection
//=============================================================================

void CPacketHandler::Packet_ServerJoinComplete(NetBitStream& bitStream)
{
    LOGI("CPacketHandler: Join complete - waiting for game join");
}

void CPacketHandler::Packet_ServerJoined(NetBitStream& bitStream)
{
    uint16_t playerId = 0;
    uint8_t playerCount = 0;
    uint16_t rootElementId = 0;

    if (!bitStream.Read(playerId) || !bitStream.Read(playerCount) || !bitStream.Read(rootElementId))
    {
        LOGE("CPacketHandler: Failed to read JOINED_GAME payload");
        return;
    }

    m_localPlayerId = playerId;

    LOGI("CPacketHandler: Joined game with player ID %u (count=%u root=%u)",
         m_localPlayerId, playerCount, rootElementId);

    // Set local player ID in player manager
    auto& playerMgr = Multiplayer::CPlayerManager::GetInstance();
    playerMgr.SetLocalPlayerId(m_localPlayerId);

    // Request initial data stream (matches PC client behavior)
    // RPC function IDs for core network: 0=PLAYER_INGAME_NOTICE, 1=INITIAL_DATA_STREAM
    SendCoreRPC(1);
    LOGI("CPacketHandler: Sent INITIAL_DATA_STREAM RPC");

    if (m_callbacks.onConnected)
    {
        m_callbacks.onConnected();
    }
}

void CPacketHandler::Packet_ServerDisconnected(NetBitStream& bitStream)
{
    std::string reason;
    bitStream.Read(reason, 256);

    LOGI("CPacketHandler: Disconnected from server: %s", reason.c_str());

    if (m_callbacks.onDisconnected)
    {
        m_callbacks.onDisconnected(reason);
    }
}

//=============================================================================
// Packet Handlers - Players
//=============================================================================

void CPacketHandler::Packet_PlayerList(NetBitStream& bitStream)
{
    // Server packet layout does NOT include a player count; it starts with a flag bit and
    // streams entries until EOF. Parsing the count here desyncs the stream.
    bool showInChat = bitStream.ReadBit();
    (void)showInChat;

    auto& playerMgr = Multiplayer::CPlayerManager::GetInstance();
    int parsedPlayers = 0;

    while (bitStream.GetUnreadBits() >= static_cast<int>(ELEMENT_ID_BITS + 16))
    {
        bool parseOk = true;
        uint32_t playerId = 0;
        if (!ReadElementId(bitStream, playerId))
            break;

        uint8_t syncTimeContext = 0;
        if (!bitStream.Read(syncTimeContext))
            break;

        uint8_t nicknameLength = 0;
        if (!bitStream.Read(nicknameLength))
            break;
        if (nicknameLength > 64)
        {
            LOGW("CPacketHandler: Invalid nickname length %u", nicknameLength);
            break;
        }

        std::string nickname;
        nickname.resize(nicknameLength);
        if (nicknameLength > 0 && !bitStream.Read(&nickname[0], nicknameLength))
            break;

        uint16_t bitStreamVersion = 0;
        uint32_t buildNumber = 0;
        if (!bitStream.Read(bitStreamVersion)) break;
        if (!bitStream.Read(buildNumber)) break;

        bool isDead = bitStream.ReadBit();
        bool isSpawned = bitStream.ReadBit();
        bool isInVehicle = bitStream.ReadBit();
        bool hasJetpack = bitStream.ReadBit();
        bool nametagShowing = bitStream.ReadBit();
        bool nametagColorOverridden = bitStream.ReadBit();
        bool isHeadless = bitStream.ReadBit();
        bool isFrozen = bitStream.ReadBit();
        (void)isDead;
        (void)hasJetpack;
        (void)nametagShowing;
        (void)isHeadless;
        (void)isFrozen;

        uint8_t nametagTextLength = 0;
        if (!bitStream.Read(nametagTextLength)) break;
        if (nametagTextLength > 0)
        {
            std::string nametagText;
            nametagText.resize(nametagTextLength);
            if (!bitStream.Read(&nametagText[0], nametagTextLength))
                break;
        }

        if (nametagColorOverridden)
        {
            uint8_t r = 0, g = 0, b = 0;
            if (!bitStream.Read(r)) break;
            if (!bitStream.Read(g)) break;
            if (!bitStream.Read(b)) break;
        }

        uint8_t moveAnim = 0;
        if (!bitStream.Read(moveAnim)) break;
        (void)moveAnim;

        Multiplayer::RemoteSyncData initialSync;
        initialSync.syncTimeContext = syncTimeContext;
        initialSync.lastSyncTime = GetCurrentTimeMs();

        if (isSpawned)
        {
            uint16_t modelId = 0;
            if (!bitStream.ReadCompressed(modelId)) break;

            bool hasTeam = bitStream.ReadBit();
            if (hasTeam)
            {
                uint32_t teamId = 0;
                if (!ReadElementId(bitStream, teamId)) break;
            }

            if (isInVehicle)
            {
                uint32_t vehicleId = 0;
                if (!ReadElementId(bitStream, vehicleId)) break;

                uint8_t seatBits = 0;
                if (!bitStream.ReadBits(&seatBits, 4)) break;

                initialSync.isInVehicle = true;
                initialSync.vehicleId = vehicleId;
                initialSync.vehicleSeat = seatBits & 0x0F;
            }
            else
            {
                float x = 0.0f;
                float y = 0.0f;
                float z = 0.0f;
                if (!ReadFixedPoint(bitStream, 14, 10, x)) break;
                if (!ReadFixedPoint(bitStream, 14, 10, y)) break;
                if (!bitStream.Read(z)) break;

                float rotation = ReadFloatAsBits(bitStream, 16, -PI, PI);

                initialSync.position = Multiplayer::CVector3D(x, y, z);
                initialSync.targetPosition = initialSync.position;
                initialSync.rotation = rotation;
                initialSync.targetRotation = rotation;
            }

            uint16_t dimension = 0;
            if (!bitStream.ReadCompressed(dimension)) break;

            uint8_t fightingStyle = 0;
            if (!bitStream.Read(fightingStyle)) break;
            (void)fightingStyle;

            uint16_t alphaCompressed = 0;
            if (!bitStream.ReadCompressed(alphaCompressed)) break;

            uint8_t interior = 0;
            if (!bitStream.Read(interior)) break;
            (void)interior;

            for (unsigned char i = 0; i < 16; ++i)
            {
                bool hasWeapon = bitStream.ReadBit();
                if (hasWeapon)
                {
                    uint8_t weaponBits = 0;
                    if (!bitStream.ReadBits(&weaponBits, 6))
                    {
                        parseOk = false;
                        break;
                    }
                }
            }
            if (!parseOk)
                break;

            bool animRunning = bitStream.ReadBit();
            if (animRunning)
            {
                std::string blockName;
                std::string animName;
                if (!bitStream.Read(blockName, 256)) break;
                if (!bitStream.Read(animName, 256)) break;

                float animTime = 0.0f;
                if (!bitStream.Read(animTime)) break;

                bool loop = bitStream.ReadBit();
                bool updatePosition = bitStream.ReadBit();
                bool interruptable = bitStream.ReadBit();
                bool freezeLastFrame = bitStream.ReadBit();
                (void)loop;
                (void)updatePosition;
                (void)interruptable;
                (void)freezeLastFrame;

                float blendTime = 0.0f;
                if (!bitStream.Read(blendTime)) break;

                bool taskRestore = bitStream.ReadBit();
                (void)taskRestore;

                double startTime = 0.0;
                if (!bitStream.Read(startTime)) break;

                float speed = 0.0f;
                if (!bitStream.Read(speed)) break;
            }
        }

        // Add/update player entry
        playerMgr.AddPlayer(playerId, nickname);
        if (isSpawned && !isInVehicle)
        {
            playerMgr.UpdatePlayerSync(playerId, initialSync);
        }

        if (m_callbacks.onPlayerJoin)
        {
            m_callbacks.onPlayerJoin(playerId, nickname);
        }

        ++parsedPlayers;
    }

    LOGI("CPacketHandler: Parsed player list entries: %d", parsedPlayers);
}

void CPacketHandler::Packet_PlayerJoin(NetBitStream& bitStream)
{
    uint32_t playerId;
    std::string nickname;

    static int s_debugJoinCount = 0;
    const bool debug = (s_debugJoinCount < 5);
    auto logPos = [&](const char* label)
    {
        if (!debug)
            return;
        LOGD("PLAYER_JOIN %s: read=%d/%d unread=%d",
             label,
             bitStream.GetReadOffsetBits(),
             bitStream.GetBitsUsed(),
             bitStream.GetUnreadBits());
    };
    auto logHex = [&](const char* label)
    {
        if (!debug)
            return;
        const uint8_t* data = bitStream.GetData();
        const int bytesUsed = bitStream.GetBytesUsed();
        const int dumpBytes = std::min(bytesUsed, 32);
        char buffer[256];
        int offset = 0;
        offset += std::snprintf(buffer + offset, sizeof(buffer) - offset, "%s bytes=", label);
        for (int i = 0; i < dumpBytes && offset < static_cast<int>(sizeof(buffer)); ++i)
        {
            offset += std::snprintf(buffer + offset, sizeof(buffer) - offset, "%02X ", data[i]);
        }
        LOGD("PLAYER_JOIN %s", buffer);
    };
    auto fail = [&](const char* label)
    {
        if (debug)
        {
            LOGD("PLAYER_JOIN failed %s", label);
            logPos("fail");
            logHex("fail");
            ++s_debugJoinCount;
        }
    };

    logPos("start");
    logHex("start");

    // Server sends raw uint16_t player ID (not 17-bit ElementID)
    uint16_t rawPlayerId = 0;
    if (!bitStream.Read(rawPlayerId))
    {
        fail("playerId");
        return;
    }
    playerId = rawPlayerId;
    logPos("after playerId");

    // Server sends uint8_t nickname length (not uint16_t)
    uint8_t nicknameLength = 0;
    if (!bitStream.Read(nicknameLength))
    {
        fail("nicknameLength");
        return;
    }
    if (nicknameLength > 64)
    {
        LOGD("PLAYER_JOIN invalid nickname length %u", nicknameLength);
        fail("nicknameLength > 64");
        return;
    }
    nickname.resize(nicknameLength);
    if (nicknameLength > 0 && !bitStream.Read(&nickname[0], nicknameLength))
    {
        fail("nickname");
        return;
    }
    logPos("after nickname");

    if (debug)
    {
        LOGD("PLAYER_JOIN parsed id=%u nickLen=%u nick='%s'", playerId, nicknameLength, nickname.c_str());
        ++s_debugJoinCount;
    }

    LOGI("CPacketHandler: Player joined: %s (ID: %u)", nickname.c_str(), playerId);

    // Add to player manager
    auto& playerMgr = Multiplayer::CPlayerManager::GetInstance();
    playerMgr.AddPlayer(playerId, nickname);

    if (m_callbacks.onPlayerJoin)
    {
        m_callbacks.onPlayerJoin(playerId, nickname);
    }
}

void CPacketHandler::Packet_PlayerQuit(NetBitStream& bitStream)
{
    uint32_t playerId;
    uint8_t quitReason;

    if (!ReadElementId(bitStream, playerId)) return;
    if (!bitStream.Read(quitReason)) return;

    LOGI("CPacketHandler: Player quit: ID %u (reason: %d)", playerId, quitReason);

    // Remove from player manager
    auto& playerMgr = Multiplayer::CPlayerManager::GetInstance();
    playerMgr.RemovePlayer(playerId);

    if (m_callbacks.onPlayerQuit)
    {
        m_callbacks.onPlayerQuit(playerId);
    }
}

void CPacketHandler::Packet_PlayerSpawn(NetBitStream& bitStream)
{
    // Server sends non-standard raw format (not MTA PC bitstream format):
    // uint16_t playerId, uint16_t modelId, uint16_t teamId,
    // float x, float y, float z, float rotation,
    // float health, float armor, uint8_t nicknameLength, char[] nickname

    uint32_t playerId;
    float x, y, z;
    float rotation;
    uint16_t skinId;

    static int s_debugSpawnCount = 0;
    const bool debug = (s_debugSpawnCount < 5);
    auto logPos = [&](const char* label)
    {
        if (!debug)
            return;
        LOGD("PLAYER_SPAWN %s: read=%d/%d unread=%d",
             label,
             bitStream.GetReadOffsetBits(),
             bitStream.GetBitsUsed(),
             bitStream.GetUnreadBits());
    };
    auto logHex = [&](const char* label)
    {
        if (!debug)
            return;
        const uint8_t* data = bitStream.GetData();
        const int bytesUsed = bitStream.GetBytesUsed();
        const int dumpBytes = std::min(bytesUsed, 48);
        char buffer[256];
        int offset = 0;
        offset += std::snprintf(buffer + offset, sizeof(buffer) - offset, "%s bytes=", label);
        for (int i = 0; i < dumpBytes && offset < static_cast<int>(sizeof(buffer)); ++i)
        {
            offset += std::snprintf(buffer + offset, sizeof(buffer) - offset, "%02X ", data[i]);
        }
        LOGD("PLAYER_SPAWN %s", buffer);
    };
    auto fail = [&](const char* label)
    {
        if (debug)
        {
            LOGD("PLAYER_SPAWN failed %s", label);
            logPos("fail");
            logHex("fail");
            ++s_debugSpawnCount;
        }
    };

    logPos("start");
    logHex("start");

    // Server sends raw uint16_t player ID (not 17-bit ElementID)
    uint16_t rawPlayerId = 0;
    if (!bitStream.Read(rawPlayerId))
    {
        fail("playerId");
        return;
    }
    playerId = rawPlayerId;
    logPos("after playerId");

    // Model ID (skin)
    uint16_t modelId = 0;
    if (!bitStream.Read(modelId))
    {
        fail("modelId");
        return;
    }
    skinId = modelId;
    logPos("after modelId");

    // Team ID (unused but must read)
    uint16_t teamId = 0;
    if (!bitStream.Read(teamId))
    {
        fail("teamId");
        return;
    }
    logPos("after teamId");

    // Position
    if (!bitStream.Read(x))
    {
        fail("x");
        return;
    }
    if (!bitStream.Read(y))
    {
        fail("y");
        return;
    }
    if (!bitStream.Read(z))
    {
        fail("z");
        return;
    }
    logPos("after position");

    // Rotation
    if (!bitStream.Read(rotation))
    {
        fail("rotation");
        return;
    }
    logPos("after rotation");

    // Health (non-standard, not in MTA PC format)
    float health = 0.0f;
    if (!bitStream.Read(health))
    {
        fail("health");
        return;
    }
    logPos("after health");

    // Armor (non-standard, not in MTA PC format)
    float armor = 0.0f;
    if (!bitStream.Read(armor))
    {
        fail("armor");
        return;
    }
    logPos("after armor");

    // Nickname (non-standard, not in MTA PC format)
    uint8_t nicknameLength = 0;
    std::string nickname;
    if (!bitStream.Read(nicknameLength))
    {
        fail("nicknameLength");
        return;
    }
    if (nicknameLength > 0 && nicknameLength <= 64)
    {
        nickname.resize(nicknameLength);
        if (!bitStream.Read(&nickname[0], nicknameLength))
        {
            fail("nickname");
            return;
        }
    }
    logPos("after nickname");

    LOGI("CPacketHandler: Player %u ('%s') spawned at (%.1f, %.1f, %.1f) skin=%u health=%.0f",
         playerId, nickname.c_str(), x, y, z, skinId, health);

    if (debug)
    {
        LOGD("PLAYER_SPAWN parsed id=%u model=%u team=%u pos=(%.3f,%.3f,%.3f) rot=%.3f health=%.1f armor=%.1f nick='%s'",
             playerId,
             modelId,
             teamId,
             x, y, z,
             rotation,
             health,
             armor,
             nickname.c_str());
        ++s_debugSpawnCount;
    }

    // Spawn in player manager
    auto& playerMgr = Multiplayer::CPlayerManager::GetInstance();
    Multiplayer::CVector3D pos(x, y, z);
    playerMgr.SpawnPlayer(playerId, pos, rotation, skinId);

    if (m_callbacks.onPlayerSpawn)
    {
        m_callbacks.onPlayerSpawn(playerId, x, y, z);
    }
}

void CPacketHandler::Packet_PlayerWasted(NetBitStream& bitStream)
{
    uint32_t playerId;
    uint32_t killerId;
    uint8_t weaponId;
    uint8_t bodypart;

    if (!ReadElementId(bitStream, playerId)) return;
    if (!ReadElementId(bitStream, killerId)) return;
    bitStream.Read(weaponId);
    bitStream.Read(bodypart);

    LOGI("CPacketHandler: Player %u died", playerId);
}

void CPacketHandler::Packet_PlayerChangeNick(NetBitStream& bitStream)
{
    uint32_t playerId;
    std::string newNickname;

    if (!ReadElementId(bitStream, playerId)) return;
    if (!bitStream.Read(newNickname, 256)) return;

    LOGI("CPacketHandler: Player %u changed nick to: %s", playerId, newNickname.c_str());
}

//=============================================================================
// Packet Handlers - Sync
//=============================================================================

void CPacketHandler::Packet_PlayerPureSync(NetBitStream& bitStream)
{
    uint32_t playerId;
    uint8_t syncTimeContext = 0;
    uint16_t latency = 0;
    static int s_debugSyncCount = 0;
    const bool debug = (s_debugSyncCount < 10);
    auto logPosNoId = [&](const char* label)
    {
        if (!debug)
            return;
        LOGD("PURESYNC %s: read=%d/%d unread=%d",
             label,
             bitStream.GetReadOffsetBits(),
             bitStream.GetBitsUsed(),
             bitStream.GetUnreadBits());
    };
    auto logHexNoId = [&](const char* label)
    {
        if (!debug)
            return;
        const uint8_t* data = bitStream.GetData();
        const int bytesUsed = bitStream.GetBytesUsed();
        const int dumpBytes = std::min(bytesUsed, 8);
        char buffer[128];
        int offset = 0;
        offset += std::snprintf(buffer + offset, sizeof(buffer) - offset, "%s bytesUsed=%d bitsUsed=%d bytes=",
                                label,
                                bytesUsed,
                                bitStream.GetBitsUsed());
        for (int i = 0; i < dumpBytes && offset < static_cast<int>(sizeof(buffer)); ++i)
        {
            offset += std::snprintf(buffer + offset, sizeof(buffer) - offset, "%02X ", data[i]);
        }
        LOGD("PURESYNC %s", buffer);
    };
    auto isLikelyAligned = [&](int offsetBits) -> bool
    {
        NetBitStream probe(bitStream.GetData(), bitStream.GetBytesUsed());
        if (offsetBits > 0)
        {
            probe.SetReadOffsetBits(offsetBits);
        }

        uint32_t probePlayerId = 0;
        if (!ReadElementId(probe, probePlayerId)) return false;

        uint8_t probeTimeContext = 0;
        if (!probe.Read(probeTimeContext)) return false;

        uint16_t probeLatency = 0;
        if (!probe.ReadCompressed(probeLatency)) return false;

        SControllerState probeController{};
        if (!ReadFullKeysync(probeController, probe)) return false;

        uint32_t rawFlags = 0;
        if (!ReadBitsToUInt(probe, rawFlags, SPlayerPuresyncFlags::BITCOUNT)) return false;

        if (rawFlags & (1u << 5))
        {
            uint32_t contactId = 0;
            if (!ReadElementId(probe, contactId)) return false;
        }

        SPcPositionSync position(false);
        if (!position.Read(probe)) return false;

        return !(position.x == 0.0f && position.y == 0.0f && position.z == 0.0f);
    };

    int startOffsetBits = 0;
    for (int offset = 0; offset <= 8; ++offset)
    {
        if (isLikelyAligned(offset))
        {
            startOffsetBits = offset;
            if (debug && offset != 0)
            {
                LOGD("PURESYNC: applying %d-bit framing offset", offset);
            }
            break;
        }
    }
    if (startOffsetBits > 0)
    {
        bitStream.SetReadOffsetBits(startOffsetBits);
    }

    auto logPos = [&](const char* label)
    {
        if (!debug)
            return;
        LOGD("PURESYNC[%u] %s: read=%d/%d unread=%d",
             playerId,
             label,
             bitStream.GetReadOffsetBits(),
             bitStream.GetBitsUsed(),
             bitStream.GetUnreadBits());
    };
    auto logHex = [&](const char* label)
    {
        if (!debug)
            return;
        const uint8_t* data = bitStream.GetData();
        const int bytesUsed = bitStream.GetBytesUsed();
        const int dumpBytes = std::min(bytesUsed, 8);
        char buffer[128];
        int offset = 0;
        offset += std::snprintf(buffer + offset, sizeof(buffer) - offset, "%s bytes=", label);
        for (int i = 0; i < dumpBytes && offset < static_cast<int>(sizeof(buffer)); ++i)
        {
            offset += std::snprintf(buffer + offset, sizeof(buffer) - offset, "%02X ", data[i]);
        }
        LOGD("PURESYNC[%u] %s", playerId, buffer);
    };
    auto debugParseWithPlayerIdBits = [&](int playerIdBits)
    {
        if (!debug)
            return;
        const int savedOffset = bitStream.GetReadOffsetBits();
        bitStream.SetReadOffsetBits(startOffsetBits);
        uint32_t pid = 0;
        bool okPid = ReadBitsToUInt(bitStream, pid, static_cast<size_t>(playerIdBits));
        uint8_t timeContext = 0;
        bool okTime = bitStream.Read(timeContext);
        uint16_t latencyTmp = 0;
        bool okLatency = bitStream.ReadCompressed(latencyTmp);
        SControllerState tmpController{};
        SFullKeysyncDebug ksDebug{};
        bool okKeys = ReadFullKeysync(tmpController, bitStream, &ksDebug);
        const int offsetAfter = bitStream.GetReadOffsetBits();
        LOGD("PURESYNC[%u] probe pidBits=%d ok=%d/%d/%d/%d pid=%u time=%u lat=%u keys=0x%02X sq=%d cr=%d lx=%d ly=%d off=%d",
             playerId,
             playerIdBits,
             okPid ? 1 : 0,
             okTime ? 1 : 0,
             okLatency ? 1 : 0,
             okKeys ? 1 : 0,
             pid,
             timeContext,
             latencyTmp,
             ksDebug.keyBits,
             ksDebug.hasButtonSquare ? 1 : 0,
             ksDebug.hasButtonCross ? 1 : 0,
             ksDebug.leftX,
             ksDebug.leftY,
             offsetAfter);
        bitStream.SetReadOffsetBits(savedOffset);
    };
    auto debugProbeOffset = [&](int offsetBits, const char* label)
    {
        if (!debug)
            return;
        NetBitStream probe(bitStream.GetData(), bitStream.GetBytesUsed());
        if (offsetBits > 0)
        {
            probe.SetReadOffsetBits(offsetBits);
        }
        uint32_t pid = 0;
        bool okPid = ReadElementId(probe, pid);
        uint8_t timeContext = 0;
        bool okTime = probe.Read(timeContext);
        uint16_t latencyTmp = 0;
        bool okLatency = probe.ReadCompressed(latencyTmp);
        SControllerState tmpController{};
        SFullKeysyncDebug ksDebug{};
        bool okKeys = ReadFullKeysync(tmpController, probe, &ksDebug);
        LOGD("PURESYNC probe %s off=%d ok=%d/%d/%d/%d pid=%u time=%u lat=%u keys=0x%02X lx=%d ly=%d read=%d",
             label,
             offsetBits,
             okPid ? 1 : 0,
             okTime ? 1 : 0,
             okLatency ? 1 : 0,
             okKeys ? 1 : 0,
             pid,
             timeContext,
             latencyTmp,
             ksDebug.keyBits,
             ksDebug.leftX,
             ksDebug.leftY,
             probe.GetReadOffsetBits());
    };

    logHexNoId("start");
    logPosNoId("start");
    debugProbeOffset(0, "offset0");
    debugProbeOffset(8, "offset8");

    // Read player ID first (added by server relay)
    logPosNoId("before playerId");
    if (!ReadElementId(bitStream, playerId))
    {
        LOGD("PURESYNC: Failed to read playerId: read=%d/%d unread=%d",
             bitStream.GetReadOffsetBits(),
             bitStream.GetBitsUsed(),
             bitStream.GetUnreadBits());
        return;
    }
    logHex("packet");
    logPos("after playerId");
    debugParseWithPlayerIdBits(16);
    debugParseWithPlayerIdBits(17);
    debugParseWithPlayerIdBits(18);

    // Skip if this is our own sync (server echoing)
    if (playerId == m_localPlayerId)
        return;

    logPos("before timeContext");
    if (!bitStream.Read(syncTimeContext))
    {
        LOGD("PURESYNC[%u] Failed to read timeContext", playerId);
        logPos("timeContext fail");
        return;
    }
    logPos("after timeContext");
    if (debug)
    {
        const int savedOffset = bitStream.GetReadOffsetBits();
        const bool isByte = bitStream.ReadBit();
        bitStream.SetReadOffsetBits(savedOffset);
        LOGD("PURESYNC[%u] latency selector bit=%d", playerId, isByte ? 1 : 0);
    }
    logPos("before latency");
    if (!bitStream.ReadCompressed(latency))
    {
        LOGD("PURESYNC[%u] Failed to read latency", playerId);
        logPos("latency fail");
        return;
    }
    if (debug)
    {
        LOGD("PURESYNC[%u] latency=%u", playerId, latency);
    }
    logPos("after latency");

    SControllerState controllerState;
    SFullKeysyncDebug keysyncDebug;
    logPos("before keysync");
    if (!ReadFullKeysync(controllerState, bitStream, debug ? &keysyncDebug : nullptr))
    {
        LOGD("PURESYNC[%u] Failed to read full keysync", playerId);
        logPos("keysync fail");
        return;
    }
    logPos("after keysync");
    if (debug)
    {
        LOGD("PURESYNC[%u] keysync bits=0x%02X sq=%d(%u) cr=%d(%u) lx=%d ly=%d",
             playerId,
             keysyncDebug.keyBits,
             keysyncDebug.hasButtonSquare ? 1 : 0,
             keysyncDebug.buttonSquare,
             keysyncDebug.hasButtonCross ? 1 : 0,
             keysyncDebug.buttonCross,
             keysyncDebug.leftX,
             keysyncDebug.leftY);
    }

    auto peekBits = [&](int offsetBits, size_t bitCount, bool msbFirst, bool msbSignificance) -> uint32_t
    {
        uint32_t value = 0;
        const uint8_t* data = bitStream.GetData();
        const int totalBits = bitStream.GetBitsUsed();
        for (size_t i = 0; i < bitCount; ++i)
        {
            int bitIndex = offsetBits + static_cast<int>(i);
            if (bitIndex >= totalBits)
                break;
            int byteIndex = bitIndex / 8;
            int bitInByte = bitIndex % 8;
            int shift = msbFirst ? (7 - bitInByte) : bitInByte;
            bool bit = (data[byteIndex] >> shift) & 1;
            if (bit)
            {
                const size_t dst = msbSignificance ? (bitCount - 1 - i) : i;
                if (dst < 32)
                    value |= (1u << dst);
            }
        }
        return value;
    };

    SPlayerPuresyncFlags flags;
    uint32_t rawFlags = 0;
    const int flagsOffset = bitStream.GetReadOffsetBits();
    logPos("before flags");
    if (!ReadBitsToUInt(bitStream, rawFlags, SPlayerPuresyncFlags::BITCOUNT))
    {
        LOGD("PURESYNC[%u] Failed to read flags", playerId);
        logPos("flags fail");
        return;
    }
    logPos("after flags");
    flags.data.bIsInWater = (rawFlags & (1u << 0)) != 0;
    flags.data.bIsOnGround = (rawFlags & (1u << 1)) != 0;
    flags.data.bHasJetPack = (rawFlags & (1u << 2)) != 0;
    flags.data.bIsDucked = (rawFlags & (1u << 3)) != 0;
    flags.data.bWearsGoogles = (rawFlags & (1u << 4)) != 0;
    flags.data.bHasContact = (rawFlags & (1u << 5)) != 0;
    flags.data.bIsChoking = (rawFlags & (1u << 6)) != 0;
    flags.data.bAkimboTargetUp = (rawFlags & (1u << 7)) != 0;
    flags.data.bIsOnFire = (rawFlags & (1u << 8)) != 0;
    flags.data.bHasAWeapon = (rawFlags & (1u << 9)) != 0;
    flags.data.bSyncingVelocity = (rawFlags & (1u << 10)) != 0;
    flags.data.bStealthAiming = (rawFlags & (1u << 11)) != 0;
    flags.data.isReloadingWeapon = (rawFlags & (1u << 12)) != 0;
    flags.data.animInterrupted = (rawFlags & (1u << 13)) != 0;
    flags.data.hangingDuringClimb = (rawFlags & (1u << 14)) != 0;
    if (debug)
    {
        uint32_t rawFlagsMsb = peekBits(flagsOffset, SPlayerPuresyncFlags::BITCOUNT, true, false);
        uint32_t rawFlagsMsbSig = peekBits(flagsOffset, SPlayerPuresyncFlags::BITCOUNT, true, true);
        LOGD("PURESYNC[%u] flags raw=0x%04X msb=0x%04X msbSig=0x%04X ground=%d water=%d ducked=%d contact=%d weapon=%d vel=%d",
             playerId,
             rawFlags,
             rawFlagsMsb,
             rawFlagsMsbSig,
             flags.data.bIsOnGround,
             flags.data.bIsInWater,
             flags.data.bIsDucked,
             flags.data.bHasContact,
             flags.data.bHasAWeapon,
             flags.data.bSyncingVelocity);
    }

    if (flags.data.bHasContact)
    {
        uint32_t contactId = 0;
        logPos("before contact");
        if (!ReadElementId(bitStream, contactId))
        {
            LOGD("PURESYNC[%u] Failed to read contactId", playerId);
            logPos("contact fail");
            return;
        }
        logPos("after contact");
    }

    SPcPositionSync position(false);
    const int positionOffset = bitStream.GetReadOffsetBits();
    logPos("before position");
    if (!position.Read(bitStream))
    {
        LOGD("PURESYNC[%u] Failed to read position", playerId);
        LOGD("PURESYNC[%u] position values: x=%.3f y=%.3f z=%.3f",
             playerId,
             position.x,
             position.y,
             position.z);
        if (debug)
        {
            const int originalOffset = bitStream.GetReadOffsetBits();
            bitStream.SetReadOffsetBits(positionOffset);
            SFloatSync<14, 10> sx;
            SFloatSync<14, 10> sy;
            const bool okX = sx.Read(bitStream);
            const bool okY = sy.Read(bitStream);
            bitStream.SetReadOffsetBits(originalOffset);
            LOGD("PURESYNC[%u] syncXY: ok=%d x=%.3f y=%.3f",
                 playerId,
                 (okX && okY) ? 1 : 0,
                 sx.data.fValue,
                 sy.data.fValue);

            uint32_t rawXMsb = peekBits(positionOffset, 24, true, false);
            uint32_t rawYMsb = peekBits(positionOffset + 24, 24, true, false);
            uint32_t rawXMsbSig = peekBits(positionOffset, 24, true, true);
            uint32_t rawYMsbSig = peekBits(positionOffset + 24, 24, true, true);
            auto decodeFixed = [](uint32_t raw, int totalBits, int fracBits) -> float
            {
                const uint32_t signBit = 1u << (totalBits - 1);
                int32_t signedValue = 0;
                if (raw & signBit)
                {
                    uint32_t mask = ~((1u << totalBits) - 1u);
                    signedValue = static_cast<int32_t>(raw | mask);
                }
                else
                {
                    signedValue = static_cast<int32_t>(raw);
                }
                return static_cast<float>(signedValue) / static_cast<float>(1 << fracBits);
            };
            LOGD("PURESYNC[%u] syncXY-msb: x=%.3f y=%.3f",
                 playerId,
                 decodeFixed(rawXMsb, 24, 10),
                 decodeFixed(rawYMsb, 24, 10));
            LOGD("PURESYNC[%u] syncXY-msbSig: x=%.3f y=%.3f",
                 playerId,
                 decodeFixed(rawXMsbSig, 24, 10),
                 decodeFixed(rawYMsbSig, 24, 10));

            const struct
            {
                int bits;
                const char* label;
            } probes[] = {
                {17, "probe+contact"},
                {8, "probe+weaponType"},
                {4, "probe+weaponSlot"},
                {25, "probe+contact+weaponType"},
                {21, "probe+contact+weaponSlot"},
                {29, "probe+contact+weaponType+weaponSlot"},
            };
            for (const auto& probe : probes)
            {
                bitStream.SetReadOffsetBits(positionOffset + probe.bits);
                SPcPositionSync probePos(false);
                const bool ok = probePos.Read(bitStream);
                LOGD("PURESYNC[%u] %s: ok=%d pos=(%.1f,%.1f,%.1f)",
                     playerId,
                     probe.label,
                     ok ? 1 : 0,
                     probePos.x,
                     probePos.y,
                     probePos.z);
            }
            bitStream.SetReadOffsetBits(originalOffset);
        }
        logPos("position fail");
        return;
    }
    logPos("after position");

    SPcPedRotationSync rotationSync;
    logPos("before rotation");
    if (!rotationSync.Read(bitStream))
    {
        LOGD("PURESYNC[%u] Failed to read rotation", playerId);
        logPos("rotation fail");
        return;
    }
    logPos("after rotation");

    SPcVelocitySync velocitySync;
    if (flags.data.bSyncingVelocity)
    {
        logPos("before velocity");
        if (!velocitySync.Read(bitStream))
        {
            LOGD("PURESYNC[%u] Failed to read velocity", playerId);
            logPos("velocity fail");
            return;
        }
        logPos("after velocity");
    }

    float healthValue = ReadFloatAsBits(bitStream, 8, 0.0f, 255.0f);
    float armorValue = ReadFloatAsBits(bitStream, 8, 0.0f, 127.5f);
    uint8_t health = static_cast<uint8_t>(std::clamp(healthValue, 0.0f, 255.0f));
    uint8_t armor = static_cast<uint8_t>(std::clamp(armorValue, 0.0f, 255.0f));
    logPos("after health/armor");

    SPcCameraRotationSync camRotation;
    logPos("before camera rotation");
    if (!camRotation.Read(bitStream))
    {
        LOGD("PURESYNC[%u] Failed to read camera rotation", playerId);
        logPos("camera rot fail");
        return;
    }
    logPos("after camera rotation");

    uint8_t weaponSlot = 0;
    uint16_t ammoInClip = 0;
    if (flags.data.bHasAWeapon)
    {
        SPcWeaponSlotSync slot;
        logPos("before weapon slot");
        if (!slot.Read(bitStream))
        {
            LOGD("PURESYNC[%u] Failed to read weapon slot", playerId);
            logPos("weapon slot fail");
            return;
        }
        weaponSlot = slot.slot;
        logPos("after weapon slot");

        auto doesSlotHaveAmmo = [](uint8_t slotId) -> bool {
            switch (slotId)
            {
                case 0:
                case 1:
                case 10:
                case 11:
                case 12:
                    return false;
                default:
                    return true;
            }
        };

        if (doesSlotHaveAmmo(weaponSlot))
        {
            SWeaponAmmoSync ammo;
            logPos("before ammo");
            if (!ammo.Read(bitStream, false, true))
            {
                LOGD("PURESYNC[%u] Failed to read ammo", playerId);
                logPos("ammo fail");
                return;
            }
            ammoInClip = ammo.ammoInClip;
            logPos("after ammo");

            const bool aimFull = (controllerState.RightShoulder1 != 0) || (controllerState.ButtonCircle != 0);
            SWeaponAimSync aim(0.0f, aimFull);
            if (!aim.Read(bitStream))
            {
                LOGD("PURESYNC[%u] Failed to read aim", playerId);
                logPos("aim fail");
                return;
            }
            logPos("after aim");
        }
    }

    // Convert controller state to pad input (0-255 range, center=128)
    uint8_t leftStickX = static_cast<uint8_t>(std::clamp(128 + controllerState.LeftStickX, 0, 255));
    uint8_t leftStickY = static_cast<uint8_t>(std::clamp(128 + controllerState.LeftStickY, 0, 255));

    uint16_t keyFlags = 0;
    if (controllerState.ButtonTriangle) keyFlags |= 0x0001;   // KEY_ACTION
    if (controllerState.ShockButtonL)   keyFlags |= 0x0002;   // KEY_CROUCH
    if (controllerState.ButtonCircle)  keyFlags |= 0x0004;   // KEY_FIRE
    if (controllerState.ButtonCross)   keyFlags |= 0x0008;   // KEY_SPRINT
    if (controllerState.LeftShoulder1) keyFlags |= 0x0010;   // KEY_SECONDARY_ATTACK
    if (controllerState.ButtonSquare)  keyFlags |= 0x0020;   // KEY_JUMP
    if (controllerState.RightShoulder1) keyFlags |= 0x0080;  // KEY_HANDBRAKE/AIM
    if (controllerState.m_bPedWalk)    keyFlags |= 0x0400;   // KEY_WALK

    static int s_camLogCount = 0;
    if (s_camLogCount < 200)
    {
        LOGD("PURESYNC[%u] camRot=%.3f pedRot=%.3f pos=(%.1f,%.1f) stick=(%u,%u) keyFlags=0x%04X",
             playerId,
             camRotation.rotation,
             rotationSync.rotation,
             position.x,
             position.y,
             leftStickX,
             leftStickY,
             keyFlags);
        ++s_camLogCount;
    }

    // Build sync data for player manager
    Multiplayer::RemoteSyncData syncData;
    syncData.position = Multiplayer::CVector3D(position.x, position.y, position.z);
    syncData.velocity = Multiplayer::CVector3D(velocitySync.x, velocitySync.y, velocitySync.z);
    syncData.rotation = rotationSync.rotation;
    syncData.cameraRotation = camRotation.rotation;
    syncData.health = health;
    syncData.armor = armor;
    syncData.weaponSlot = weaponSlot;
    syncData.ammo = ammoInClip;
    syncData.syncTimeContext = syncTimeContext;
    syncData.isOnGround = flags.data.bIsOnGround;
    syncData.isInWater = flags.data.bIsInWater;
    syncData.isDucked = flags.data.bIsDucked;
    syncData.controllerLeftStickX = leftStickX;
    syncData.controllerLeftStickY = leftStickY;
    syncData.keyFlags = keyFlags;

    // Update player manager
    auto& playerMgr = Multiplayer::CPlayerManager::GetInstance();
    playerMgr.UpdatePlayerSync(playerId, syncData);

    // Legacy callback
    if (m_callbacks.onPlayerSync)
    {
        m_callbacks.onPlayerSync(playerId, position.x, position.y, position.z, rotationSync.rotation);
    }

    // Debug log (occasionally)
    static int syncLogCount = 0;
    if (++syncLogCount >= 100)
    {
        LOGD("PURESYNC from player %u: pos=(%.1f,%.1f,%.1f) rot=%.2f health=%d",
             playerId, position.x, position.y, position.z, rotationSync.rotation, health);
        syncLogCount = 0;
    }

    if (debug)
    {
        logPos("end");
        ++s_debugSyncCount;
    }
}

void CPacketHandler::Packet_PlayerKeySync(NetBitStream& bitStream)
{
    uint32_t playerId;
    uint16_t timeContext;
    uint16_t keys;

    if (!ReadElementId(bitStream, playerId)) return;
    if (!bitStream.Read(timeContext)) return;
    if (!bitStream.Read(keys)) return;

    // Process key states for other players
    LOGD("CPacketHandler: Key sync for player %u: keys=0x%04x", playerId, keys);
}

void CPacketHandler::Packet_PlayerVehicleSync(NetBitStream& bitStream)
{
    uint32_t playerId;
    uint16_t timeContext;
    uint32_t vehicleId;
    float x, y, z;

    if (!ReadElementId(bitStream, playerId)) return;
    if (!bitStream.Read(timeContext)) return;
    if (!ReadElementId(bitStream, vehicleId)) return;
    if (!bitStream.Read(x)) return;
    if (!bitStream.Read(y)) return;
    if (!bitStream.Read(z)) return;

    LOGD("CPacketHandler: Vehicle sync for player %u in vehicle %u", playerId, vehicleId);
}

void CPacketHandler::Packet_ReturnSync(NetBitStream& bitStream)
{
    // Server is correcting our position
    float x, y, z;

    if (!bitStream.Read(x)) return;
    if (!bitStream.Read(y)) return;
    if (!bitStream.Read(z)) return;

    LOGI("CPacketHandler: Position correction to (%.1f, %.1f, %.1f)", x, y, z);
}

void CPacketHandler::Packet_LightSync(NetBitStream& bitStream)
{
    // Lightweight sync for distant players
    // Just process silently
}

//=============================================================================
// Packet Handlers - Vehicles
//=============================================================================

void CPacketHandler::Packet_VehicleSpawn(NetBitStream& bitStream)
{
    uint32_t vehicleId;
    uint16_t model;
    float x, y, z;
    float rotation;

    if (!ReadElementId(bitStream, vehicleId)) return;
    if (!bitStream.Read(model)) return;
    if (!bitStream.Read(x)) return;
    if (!bitStream.Read(y)) return;
    if (!bitStream.Read(z)) return;
    if (!bitStream.Read(rotation)) return;

    LOGI("CPacketHandler: Vehicle %u (model %d) spawned at (%.1f, %.1f, %.1f)",
         vehicleId, model, x, y, z);
}

void CPacketHandler::Packet_VehicleInOut(NetBitStream& bitStream)
{
    uint32_t playerId;
    uint32_t vehicleId;
    uint8_t action;  // 0=getting in, 1=getting out

    if (!ReadElementId(bitStream, playerId)) return;
    if (!ReadElementId(bitStream, vehicleId)) return;
    if (!bitStream.Read(action)) return;

    LOGI("CPacketHandler: Player %u %s vehicle %u",
         playerId, action == 0 ? "entering" : "exiting", vehicleId);
}

void CPacketHandler::Packet_VehicleDamageSync(NetBitStream& bitStream)
{
    uint32_t vehicleId;
    // Damage data...
    LOGD("CPacketHandler: Vehicle damage sync");
}

void CPacketHandler::Packet_UnoccupiedVehicleSync(NetBitStream& bitStream)
{
    // Sync for vehicles with no driver
    LOGD("CPacketHandler: Unoccupied vehicle sync");
}

//=============================================================================
// Packet Handlers - Entities
//=============================================================================

void CPacketHandler::Packet_EntityAdd(NetBitStream& bitStream)
{
    // Server is adding an entity to our world
    uint8_t entityType;
    uint32_t entityId;

    if (!bitStream.Read(entityType)) return;
    if (!bitStream.Read(entityId)) return;

    LOGD("CPacketHandler: Entity add - type %d, ID %u", entityType, entityId);
}

void CPacketHandler::Packet_EntityRemove(NetBitStream& bitStream)
{
    uint32_t entityId;

    if (!bitStream.Read(entityId)) return;

    LOGD("CPacketHandler: Entity remove - ID %u", entityId);
}

//=============================================================================
// Packet Handlers - Map
//=============================================================================

void CPacketHandler::Packet_MapInfo(NetBitStream& bitStream)
{
    // Server is sending map information
    LOGI("CPacketHandler: Received map info");
}

void CPacketHandler::Packet_MapStart(NetBitStream& bitStream)
{
    std::string mapName;
    if (!bitStream.Read(mapName, 256)) return;

    LOGI("CPacketHandler: Map started: %s", mapName.c_str());
}

//=============================================================================
// Packet Handlers - Chat/Console
//=============================================================================

void CPacketHandler::Packet_ChatEcho(NetBitStream& bitStream)
{
    uint8_t r, g, b;
    std::string message;

    if (!bitStream.Read(r)) return;
    if (!bitStream.Read(g)) return;
    if (!bitStream.Read(b)) return;
    if (!bitStream.Read(message, 1024)) return;

    LOGI("CPacketHandler: Chat: %s", message.c_str());

    if (m_callbacks.onChatMessage)
    {
        m_callbacks.onChatMessage(message, r, g, b);
    }
}

void CPacketHandler::Packet_ConsoleEcho(NetBitStream& bitStream)
{
    std::string message;
    if (!bitStream.Read(message, 1024)) return;

    LOGI("CPacketHandler: Console: %s", message.c_str());
}

void CPacketHandler::Packet_DebugEcho(NetBitStream& bitStream)
{
    uint8_t level;
    std::string message;

    if (!bitStream.Read(level)) return;
    if (!bitStream.Read(message, 1024)) return;

    LOGD("CPacketHandler: Debug[%d]: %s", level, message.c_str());
}

//=============================================================================
// Packet Handlers - RPC
//=============================================================================

void CPacketHandler::Packet_RPC(NetBitStream& bitStream)
{
    uint16_t functionId;
    if (!bitStream.Read(functionId)) return;

    ProcessRPC(static_cast<RPCFunction>(functionId), bitStream);
}

void CPacketHandler::ProcessRPC(RPCFunction function, NetBitStream& bitStream)
{
    LOGD("CPacketHandler: Processing RPC function %d", static_cast<int>(function));

    switch (function)
    {
        case RPCFunction::SET_PLAYER_MONEY:
        {
            int32_t money;
            if (bitStream.Read(money))
            {
                LOGI("CPacketHandler: Set player money to $%d", money);
            }
            break;
        }

        case RPCFunction::SET_TIME:
        {
            uint8_t hour, minute;
            if (bitStream.Read(hour) && bitStream.Read(minute))
            {
                LOGI("CPacketHandler: Set time to %02d:%02d", hour, minute);
            }
            break;
        }

        case RPCFunction::SET_WEATHER:
        {
            uint8_t weatherId;
            if (bitStream.Read(weatherId))
            {
                LOGI("CPacketHandler: Set weather to %d", weatherId);
            }
            break;
        }

        case RPCFunction::SET_ELEMENT_POSITION:
        {
            uint32_t elementId;
            float x, y, z;
            if (bitStream.Read(elementId) && bitStream.Read(x) && bitStream.Read(y) && bitStream.Read(z))
            {
                LOGD("CPacketHandler: Set element %u position to (%.1f, %.1f, %.1f)", elementId, x, y, z);
            }
            break;
        }

        case RPCFunction::SHOW_CHAT:
        {
            bool show;
            if (bitStream.ReadBit())
            {
                LOGI("CPacketHandler: Show chat: true");
            }
            break;
        }

        default:
            LOGD("CPacketHandler: Unhandled RPC function %d", static_cast<int>(function));
            break;
    }
}

//=============================================================================
// Packet Handlers - Lua/Resources
//=============================================================================

void CPacketHandler::Packet_LuaEvent(NetBitStream& bitStream)
{
    std::string eventName;
    uint32_t sourceElement;

    if (!bitStream.Read(eventName, 256)) return;
    if (!bitStream.Read(sourceElement)) return;

    LOGD("CPacketHandler: Lua event: %s (source: %u)", eventName.c_str(), sourceElement);
}

void CPacketHandler::Packet_ResourceStart(NetBitStream& bitStream)
{
    uint16_t resourceId;
    std::string resourceName;

    if (!bitStream.Read(resourceId)) return;
    if (!bitStream.Read(resourceName, 256)) return;

    LOGI("CPacketHandler: Resource started: %s (ID: %d)", resourceName.c_str(), resourceId);
}

void CPacketHandler::Packet_ResourceStop(NetBitStream& bitStream)
{
    uint16_t resourceId;
    if (!bitStream.Read(resourceId)) return;

    LOGI("CPacketHandler: Resource stopped: ID %d", resourceId);
}

//=============================================================================
// Packet Handlers - Misc
//=============================================================================

void CPacketHandler::Packet_SyncSettings(NetBitStream& bitStream)
{
    uint16_t syncRate;
    if (!bitStream.Read(syncRate))
    {
        syncRate = 66;  // Default ~15 Hz
    }

    m_syncIntervalMs = syncRate;
    LOGI("CPacketHandler: Sync interval set to %d ms", syncRate);
}

} // namespace MTA::Android::Network
