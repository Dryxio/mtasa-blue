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

//=============================================================================
// Packet Handlers - Connection
//=============================================================================

void CPacketHandler::Packet_ServerJoinComplete(NetBitStream& bitStream)
{
    LOGI("CPacketHandler: Join complete - waiting for game join");
}

void CPacketHandler::Packet_ServerJoined(NetBitStream& bitStream)
{
    // Read player ID
    bitStream.Read(m_localPlayerId);

    LOGI("CPacketHandler: Joined game with player ID %u", m_localPlayerId);

    // Set local player ID in player manager
    auto& playerMgr = Multiplayer::CPlayerManager::GetInstance();
    playerMgr.SetLocalPlayerId(m_localPlayerId);

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
    uint16_t playerCount;
    if (!bitStream.Read(playerCount))
    {
        LOGE("CPacketHandler: Failed to read player count");
        return;
    }

    LOGI("CPacketHandler: Received player list with %d players", playerCount);

    auto& playerMgr = Multiplayer::CPlayerManager::GetInstance();

    for (uint16_t i = 0; i < playerCount; ++i)
    {
        uint32_t playerId;
        std::string nickname;
        uint16_t nicknameLength;

        if (!ReadElementId(bitStream, playerId)) break;
        if (!bitStream.Read(nicknameLength)) break;
        if (nicknameLength > 256) break;

        nickname.resize(nicknameLength);
        if (!bitStream.Read(&nickname[0], nicknameLength)) break;

        LOGD("CPacketHandler: Player %d: %s", playerId, nickname.c_str());

        // Add to player manager
        playerMgr.AddPlayer(playerId, nickname);

        if (m_callbacks.onPlayerJoin)
        {
            m_callbacks.onPlayerJoin(playerId, nickname);
        }
    }
}

void CPacketHandler::Packet_PlayerJoin(NetBitStream& bitStream)
{
    uint32_t playerId;
    std::string nickname;

    if (!ReadElementId(bitStream, playerId)) return;
    if (!bitStream.Read(nickname, 256)) return;

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
    uint32_t playerId;
    float x, y, z;
    float rotation;
    uint16_t skinId;

    if (!ReadElementId(bitStream, playerId)) return;
    if (!bitStream.Read(x)) return;
    if (!bitStream.Read(y)) return;
    if (!bitStream.Read(z)) return;
    if (!bitStream.Read(rotation)) return;
    if (!bitStream.Read(skinId)) return;

    LOGI("CPacketHandler: Player %u spawned at (%.1f, %.1f, %.1f)", playerId, x, y, z);

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

    // Read player ID first (added by server relay)
    if (!ReadElementId(bitStream, playerId)) return;

    // Skip if this is our own sync (server echoing)
    if (playerId == m_localPlayerId)
        return;

    if (!bitStream.Read(syncTimeContext)) return;
    if (!bitStream.ReadCompressed(latency)) return;

    SControllerState controllerState;
    if (!ReadFullKeysync(controllerState, bitStream)) return;

    SPlayerPuresyncFlags flags;
    if (!flags.Read(bitStream)) return;

    if (flags.hasContact)
    {
        uint32_t contactId = 0;
        if (!ReadElementId(bitStream, contactId)) return;
    }

    SPcPositionSync position(false);
    if (!position.Read(bitStream)) return;

    SPcPedRotationSync rotationSync;
    if (!rotationSync.Read(bitStream)) return;

    SPcVelocitySync velocitySync;
    if (flags.syncingVelocity)
    {
        if (!velocitySync.Read(bitStream)) return;
    }

    uint8_t health = 100;
    uint8_t armor = 0;
    if (!bitStream.Read(health)) return;
    if (!bitStream.Read(armor)) return;

    SPcCameraRotationSync camRotation;
    if (!camRotation.Read(bitStream)) return;

    uint8_t weaponSlot = 0;
    uint16_t ammoInClip = 0;
    if (flags.hasAWeapon)
    {
        SPcWeaponSlotSync slot;
        if (!slot.Read(bitStream)) return;
        weaponSlot = slot.slot;

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
            if (!ammo.Read(bitStream, true, true)) return;
            ammoInClip = ammo.ammoInClip;

            const bool aimFull = (controllerState.RightShoulder1 != 0) || (controllerState.ButtonCircle != 0);
            SWeaponAimSync aim(0.0f, aimFull);
            if (!aim.Read(bitStream)) return;
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

    // Build sync data for player manager
    Multiplayer::RemoteSyncData syncData;
    syncData.position = Multiplayer::CVector3D(position.x, position.y, position.z);
    syncData.velocity = Multiplayer::CVector3D(velocitySync.x, velocitySync.y, velocitySync.z);
    syncData.rotation = rotationSync.rotation;
    syncData.health = health;
    syncData.armor = armor;
    syncData.weaponSlot = weaponSlot;
    syncData.ammo = ammoInClip;
    syncData.syncTimeContext = syncTimeContext;
    syncData.isOnGround = flags.isOnGround;
    syncData.isInWater = flags.isInWater;
    syncData.isDucked = flags.isDucked;
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
