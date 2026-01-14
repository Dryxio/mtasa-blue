/*
 * MTA:SA Android - Packet Handler
 *
 * Handles incoming MTA protocol packets and dispatches them
 * to appropriate handlers.
 *
 * Phase 7: Multiplayer Logic
 */

#ifndef CPACKET_HANDLER_ANDROID_H
#define CPACKET_HANDLER_ANDROID_H

#include "CNetAndroid.h"
#include "../multiplayer/CPlayerManager.h"
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace MTA::Android::Network
{

// Forward declarations
class CClientGame;
class CVehicleManager;

//=============================================================================
// RPC Function IDs (subset from Shared/sdk/net/rpc_enums.h)
//=============================================================================

enum class RPCFunction : uint16_t
{
    // Player functions
    SET_PLAYER_MONEY = 0,
    SHOW_PLAYER_HUD_COMPONENT,
    FORCE_PLAYER_MAP,
    SET_PLAYER_NAMETAG_TEXT,
    SET_PLAYER_NAMETAG_COLOR,
    SET_PLAYER_NAMETAG_SHOWING,

    // Ped functions
    SET_PED_ARMOR = 20,
    SET_PED_ROTATION,
    GIVE_PED_JETPACK,
    REMOVE_PED_JETPACK,
    SET_PED_FIGHTING_STYLE,
    SET_PED_GRAVITY,
    SET_PED_CHOKING,
    SET_PED_WEAPON_SLOT,
    WARP_PED_INTO_VEHICLE,
    REMOVE_PED_FROM_VEHICLE,
    SET_PED_DOING_GANG_DRIVEBY,
    SET_PED_ANIMATION,
    SET_PED_ANIMATION_PROGRESS,
    SET_PED_ANIMATION_SPEED,
    SET_PED_ON_FIRE,
    SET_PED_HEADLESS,
    SET_PED_FROZEN,
    RELOAD_PED_WEAPON,
    SET_PED_FOOTBLOOD_ENABLED,

    // Vehicle functions
    FIX_VEHICLE = 50,
    BLOW_VEHICLE,
    SET_VEHICLE_ROTATION,
    SET_VEHICLE_TURNSPEED,
    SET_VEHICLE_COLOR,
    SET_VEHICLE_LOCKED,
    SET_VEHICLE_DOORS_UNDAMAGEABLE,
    SET_VEHICLE_SIRENE_ON,
    SET_VEHICLE_LANDING_GEAR_DOWN,
    SET_VEHICLE_TAXI_LIGHT_ON,
    ADD_VEHICLE_UPGRADE,
    ADD_ALL_VEHICLE_UPGRADES,
    REMOVE_VEHICLE_UPGRADE,
    SET_VEHICLE_DAMAGE_STATE,
    SET_VEHICLE_OVERRIDE_LIGHTS,
    SET_VEHICLE_ENGINE_STATE,
    SET_VEHICLE_DIRT_LEVEL,
    SET_VEHICLE_PAINTJOB,
    SET_VEHICLE_FUEL_TANK_EXPLODABLE,
    SET_VEHICLE_FROZEN,
    SET_VEHICLE_ADJUSTABLE_PROPERTY,
    SET_TRAIN_DERAILED,
    SET_TRAIN_DERAILABLE,
    SET_TRAIN_DIRECTION,
    SET_TRAIN_SPEED,

    // Element functions
    SET_ELEMENT_PARENT = 100,
    SET_ELEMENT_DATA,
    SET_ELEMENT_POSITION,
    SET_ELEMENT_VELOCITY,
    SET_ELEMENT_INTERIOR,
    SET_ELEMENT_DIMENSION,
    ATTACH_ELEMENTS,
    DETACH_ELEMENTS,
    SET_ELEMENT_ALPHA,
    SET_ELEMENT_NAME,
    SET_ELEMENT_HEALTH,
    SET_ELEMENT_MODEL,
    SET_ELEMENT_ATTACHED_OFFSETS,
    SET_ELEMENT_DOUBLESIDED,
    SET_ELEMENT_COLLISIONS_ENABLED,
    SET_ELEMENT_FROZEN,
    SET_ELEMENT_CALL_PROPAGATION_ENABLED,

    // World functions
    SET_TIME = 130,
    SET_WEATHER,
    SET_WEATHER_BLENDED,
    SET_GRAVITY,
    SET_GAME_SPEED,
    SET_WAVE_HEIGHT,
    SET_SKY_GRADIENT,
    RESET_SKY_GRADIENT,
    SET_HEAT_HAZE,
    RESET_HEAT_HAZE,
    SET_BLUR_LEVEL,
    SET_WANTED_LEVEL,
    SET_MINUTE_DURATION,
    SET_GARAGE_OPEN,
    SET_GLITCH_ENABLED,
    SET_CLOUDS_ENABLED,
    SET_TRAFFIC_LIGHT_STATE,
    SET_JETPACK_MAXHEIGHT,
    SET_INTERIOR_SOUNDS_ENABLED,
    SET_RAIN_LEVEL,
    SET_SUN_SIZE,
    SET_SUN_COLOR,
    SET_WIND_VELOCITY,
    SET_FAR_CLIP_DISTANCE,
    SET_FOG_DISTANCE,
    SET_AIRCRAFT_MAXHEIGHT,
    SET_AIRCRAFT_MAXVELOCITY,
    SET_OCCLUSIONS_ENABLED,
    SET_MOON_SIZE,
    SET_FPS_LIMIT,

    // Camera functions
    SET_CAMERA_MATRIX = 170,
    SET_CAMERA_TARGET,
    SET_CAMERA_INTERIOR,
    FADE_CAMERA,
    SET_CAMERA_VIEW_MODE,
    SET_CAMERA_CLIP,

    // GUI/HUD functions
    SHOW_CURSOR = 180,
    SHOW_CHAT,

    // Audio functions
    PLAY_SOUND = 190,
    STOP_SOUND,
    PLAY_MISSION_AUDIO,

    // Blip functions
    DESTROY_ALL_BLIPS = 200,
    SET_BLIP_ICON,
    SET_BLIP_SIZE,
    SET_BLIP_COLOR,
    SET_BLIP_ORDERING,
    SET_BLIP_VISIBLE_DISTANCE,

    // Marker functions
    SET_MARKER_TYPE = 210,
    SET_MARKER_SIZE,
    SET_MARKER_COLOR,
    SET_MARKER_TARGET,
    SET_MARKER_ICON,

    // Object functions
    SET_OBJECT_ROTATION = 220,
    MOVE_OBJECT,
    STOP_OBJECT,
    SET_OBJECT_SCALE,
    SET_OBJECT_STATIC,
    SET_OBJECT_VISIBILITY,

    // Radar area functions
    SET_RADAR_AREA_SIZE = 230,
    SET_RADAR_AREA_COLOR,
    SET_RADAR_AREA_FLASHING,
    IS_RADAR_AREA_FLASHING,

    // Pickup functions
    SET_PICKUP_TYPE = 240,

    // Team functions
    SET_PLAYER_TEAM = 250,
    SET_TEAM_NAME,
    SET_TEAM_COLOR,
    SET_TEAM_FRIENDLY_FIRE,

    // Water functions
    SET_WATER_LEVEL = 260,
    SET_WATER_VERTEX_POSITION,
    SET_WORLD_WATER_LEVEL,
    RESET_WORLD_WATER_LEVEL,
};

//=============================================================================
// Packet Handler Callbacks
//=============================================================================

struct PacketHandlerCallbacks
{
    // Connection events
    std::function<void()> onConnected;
    std::function<void(const std::string& reason)> onDisconnected;
    std::function<void(const std::string& error)> onConnectionFailed;

    // Player events
    std::function<void(uint32_t playerId, const std::string& name)> onPlayerJoin;
    std::function<void(uint32_t playerId)> onPlayerQuit;
    std::function<void(uint32_t playerId, float x, float y, float z)> onPlayerSpawn;

    // Chat events
    std::function<void(const std::string& message, uint8_t r, uint8_t g, uint8_t b)> onChatMessage;

    // Sync events
    std::function<void(uint32_t playerId, float x, float y, float z, float rotation)> onPlayerSync;
};

//=============================================================================
// CPacketHandler
//=============================================================================

class CPacketHandler
{
public:
    CPacketHandler();
    ~CPacketHandler();

    /**
     * Initialize packet handler
     */
    bool Initialize(CNetAndroid* network);

    /**
     * Shutdown packet handler
     */
    void Shutdown();

    /**
     * Set callbacks for packet events
     */
    void SetCallbacks(const PacketHandlerCallbacks& callbacks);

    /**
     * Process a received packet
     * @return true if packet was handled
     */
    bool ProcessPacket(PacketID packetId, NetBitStream& bitStream);

    //=========================================================================
    // Packet Senders
    //=========================================================================

    /**
     * Send player join request to server
     */
    void SendJoinRequest(const std::string& nickname, const std::string& serial,
                         const std::string& password = "");

    /**
     * Send chat message
     */
    void SendChatMessage(const std::string& message);

    /**
     * Send player spawn request
     */
    void SendSpawnRequest(uint16_t skinId);

    /**
     * Send player position sync
     */
    void SendPlayerSync(float x, float y, float z, float rotation,
                        float vx, float vy, float vz, bool onGround);

    /**
     * Send key sync (input state)
     */
    void SendKeySync(uint16_t keys, float aimX, float aimY);

    /**
     * Send vehicle sync (when driving)
     */
    void SendVehicleSync(uint32_t vehicleId, float x, float y, float z,
                         float rotX, float rotY, float rotZ, float health);

    /**
     * Send RPC call
     */
    void SendRPC(RPCFunction function, NetBitStream& data);
    void SendCoreRPC(uint8_t functionId);

private:
    //=========================================================================
    // Packet Handlers
    //=========================================================================

    // Connection packets
    void Packet_ServerJoined(NetBitStream& bitStream);
    void Packet_ServerDisconnected(NetBitStream& bitStream);
    void Packet_ServerJoinComplete(NetBitStream& bitStream);

    // Player packets
    void Packet_PlayerList(NetBitStream& bitStream);
    void Packet_PlayerJoin(NetBitStream& bitStream);
    void Packet_PlayerQuit(NetBitStream& bitStream);
    void Packet_PlayerSpawn(NetBitStream& bitStream);
    void Packet_PlayerWasted(NetBitStream& bitStream);
    void Packet_PlayerChangeNick(NetBitStream& bitStream);

    // Sync packets
    void Packet_PlayerPureSync(NetBitStream& bitStream);
    void Packet_PlayerKeySync(NetBitStream& bitStream);
    void Packet_PlayerVehicleSync(NetBitStream& bitStream);
    void Packet_ReturnSync(NetBitStream& bitStream);
    void Packet_LightSync(NetBitStream& bitStream);

    // Vehicle packets
    void Packet_VehicleSpawn(NetBitStream& bitStream);
    void Packet_VehicleInOut(NetBitStream& bitStream);
    void Packet_VehicleDamageSync(NetBitStream& bitStream);
    void Packet_UnoccupiedVehicleSync(NetBitStream& bitStream);

    // Entity packets
    void Packet_EntityAdd(NetBitStream& bitStream);
    void Packet_EntityRemove(NetBitStream& bitStream);

    // Map packets
    void Packet_MapInfo(NetBitStream& bitStream);
    void Packet_MapStart(NetBitStream& bitStream);

    // Chat/Console
    void Packet_ChatEcho(NetBitStream& bitStream);
    void Packet_ConsoleEcho(NetBitStream& bitStream);
    void Packet_DebugEcho(NetBitStream& bitStream);

    // RPC
    void Packet_RPC(NetBitStream& bitStream);

    // Lua
    void Packet_LuaEvent(NetBitStream& bitStream);

    // Resource
    void Packet_ResourceStart(NetBitStream& bitStream);
    void Packet_ResourceStop(NetBitStream& bitStream);

    // Misc
    void Packet_SyncSettings(NetBitStream& bitStream);

    //=========================================================================
    // RPC Handlers
    //=========================================================================

    void ProcessRPC(RPCFunction function, NetBitStream& bitStream);

private:
    CNetAndroid* m_network = nullptr;
    PacketHandlerCallbacks m_callbacks;

    // Local player info
    uint32_t m_localPlayerId = 0;
    std::string m_localNickname;

    // Sync settings
    uint16_t m_syncIntervalMs = 66;  // ~15 Hz
    uint64_t m_lastSyncTime = 0;
};

} // namespace MTA::Android::Network

#endif // CPACKET_HANDLER_ANDROID_H
