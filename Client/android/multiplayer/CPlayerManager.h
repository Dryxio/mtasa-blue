/*
 * MTA:SA Android - Player Manager
 *
 * Manages all remote players in the game world.
 * Handles player creation, destruction, and sync updates.
 *
 * Phase 7e: Player Synchronization
 */

#ifndef CPLAYER_MANAGER_H
#define CPLAYER_MANAGER_H

#include "CRemotePlayer.h"
#include "CWorldPlayers.h"
#include "CPadHooks.h"
#include "../game_sa/CGameBypass.h"
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <functional>
#include <bitset>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace MTA::Android::Multiplayer
{

#define PMGR_LOG_TAG "MTA-PlayerMgr"
#define PMGR_LOGI(...) __android_log_print(ANDROID_LOG_INFO, PMGR_LOG_TAG, __VA_ARGS__)
#define PMGR_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, PMGR_LOG_TAG, __VA_ARGS__)
#define PMGR_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, PMGR_LOG_TAG, __VA_ARGS__)
#define PMGR_LOGW(...) __android_log_print(ANDROID_LOG_WARN, PMGR_LOG_TAG, __VA_ARGS__)

//=============================================================================
// Player Manager Callbacks
//=============================================================================

struct PlayerManagerCallbacks
{
    std::function<void(uint32_t playerId, const std::string& nickname)> onPlayerAdded;
    std::function<void(uint32_t playerId)> onPlayerRemoved;
    std::function<void(uint32_t playerId, const CVector3D& position)> onPlayerSpawned;
};

//=============================================================================
// CPlayerManager - Singleton
//=============================================================================

class CPlayerManager
{
public:
    /**
     * Get singleton instance
     */
    static CPlayerManager& GetInstance()
    {
        static CPlayerManager instance;
        return instance;
    }

    // Delete copy/move constructors
    CPlayerManager(const CPlayerManager&) = delete;
    CPlayerManager& operator=(const CPlayerManager&) = delete;

    //=========================================================================
    // Initialization
    //=========================================================================

    /**
     * Initialize the player manager
     * @param gameBase Base address of libGTASA.so
     */
    bool Initialize(uintptr_t gameBase);

    /**
     * Shutdown and cleanup
     */
    void Shutdown();

    /**
     * Check if initialized
     */
    bool IsInitialized() const { return m_initialized; }

    //=========================================================================
    // Callbacks
    //=========================================================================

    /**
     * Set callbacks for player events
     */
    void SetCallbacks(const PlayerManagerCallbacks& callbacks);

    //=========================================================================
    // Local Player
    //=========================================================================

    /**
     * Set local player ID (received from server)
     */
    void SetLocalPlayerId(uint32_t playerId);

    /**
     * Get local player ID
     */
    uint32_t GetLocalPlayerId() const { return m_localPlayerId; }

    //=========================================================================
    // Remote Player Management
    //=========================================================================

    /**
     * Add a new remote player
     * @param playerId Player's network ID
     * @param nickname Player's nickname
     * @return Pointer to the created player, or nullptr on failure
     */
    CRemotePlayer* AddPlayer(uint32_t playerId, const std::string& nickname);

    /**
     * Remove a remote player
     * @param playerId Player to remove
     */
    void RemovePlayer(uint32_t playerId);

    /**
     * Get a remote player by ID
     * @return Player pointer or nullptr if not found
     */
    CRemotePlayer* GetPlayer(uint32_t playerId);

    /**
     * Get const player pointer
     */
    const CRemotePlayer* GetPlayer(uint32_t playerId) const;

    /**
     * Check if player exists
     */
    bool HasPlayer(uint32_t playerId) const;

    /**
     * Get number of remote players
     */
    size_t GetPlayerCount() const;

    /**
     * Get all player IDs
     */
    std::vector<uint32_t> GetAllPlayerIds() const;

    //=========================================================================
    // Sync Updates
    //=========================================================================

    /**
     * Update player sync data (called from packet handler)
     * @param playerId Player to update
     * @param data New sync data
     */
    void UpdatePlayerSync(uint32_t playerId, const RemoteSyncData& data);

    /**
     * Spawn a remote player
     */
    void SpawnPlayer(uint32_t playerId, const CVector3D& position,
                     float rotation, uint16_t skinId);

    /**
     * Despawn a remote player
     */
    void DespawnPlayer(uint32_t playerId);

    //=========================================================================
    // Per-Frame Processing
    //=========================================================================

    /**
     * Process all players (update interpolation, apply to game)
     * Should be called every frame
     */
    void Process();

    /**
     * Called when game state changes
     */
    void OnGameStateChange(int newState);

private:
    CPlayerManager();
    ~CPlayerManager();

    //=========================================================================
    // Internal Methods
    //=========================================================================

    /**
     * Remove stale players (not synced recently)
     */
    void RemoveStalePlayers();

    /**
     * Get current time in ms
     */
    static uint64_t GetTimeMs();

private:
    // Initialization state
    bool m_initialized;
    uintptr_t m_gameBase;

    // Local player
    uint32_t m_localPlayerId;

    // Remote players (thread-safe)
    std::map<uint32_t, std::unique_ptr<CRemotePlayer>> m_players;
    mutable std::mutex m_playersMutex;

    // Callbacks
    PlayerManagerCallbacks m_callbacks;

    // Processing state
    uint64_t m_lastProcessTime;
    uint64_t m_lastStaleCheckTime;

    // Game state (9 = GS_PLAYING_GAME = ready for ped creation)
    int m_gameState;

    // Timing for deferred remote ped creation
    // We wait a few seconds after local player spawns before creating remote peds
    // This ensures the game is fully initialized and models are loaded
    uint64_t m_localPlayerFirstSeenTime;
    bool m_localPlayerWasEverSeen;

    // Constants
    static constexpr int STALE_CHECK_INTERVAL_MS = 1000;  // Check for stale players every 1s
    static constexpr int STALE_TIMEOUT_MS = 10000;        // Remove after 10s of no sync
    static constexpr int REMOTE_PED_SPAWN_DELAY_MS = 3000;  // Wait 3s after local player spawns

    // Slot management for CPad hooks
    // Slots: 0=local, 1=unused, 2-31=remote players (30 remote players max)
    static constexpr int MIN_REMOTE_SLOT = 2;
    static constexpr int MAX_REMOTE_SLOT = 31;
    static constexpr int MAX_SLOTS = 32;

    std::bitset<MAX_SLOTS> m_usedSlots;           // Which slots are in use
    std::map<uint32_t, uint8_t> m_playerToSlot;   // PlayerId -> slot number
    std::map<uint8_t, uint32_t> m_slotToPlayer;   // Slot number -> PlayerId

    /**
     * Allocate a slot for a new remote player
     * @return Slot number (2-31) or 0 if no slots available
     */
    uint8_t AllocateSlot()
    {
        for (int i = MIN_REMOTE_SLOT; i <= MAX_REMOTE_SLOT; ++i)
        {
            if (!m_usedSlots[i])
            {
                m_usedSlots[i] = true;
                return static_cast<uint8_t>(i);
            }
        }
        return 0;  // No slots available
    }

    /**
     * Free a slot when player is removed
     */
    void FreeSlot(uint8_t slot)
    {
        if (slot >= MIN_REMOTE_SLOT && slot <= MAX_REMOTE_SLOT)
        {
            m_usedSlots[slot] = false;
        }
    }

public:
    /**
     * Get the slot number for a player
     * @return Slot (2-31) or 0 if not found
     */
    uint8_t GetPlayerSlot(uint32_t playerId) const
    {
        auto it = m_playerToSlot.find(playerId);
        if (it != m_playerToSlot.end())
            return it->second;
        return 0;
    }
};

//=============================================================================
// Implementation
//=============================================================================

inline uint64_t CPlayerManager::GetTimeMs()
{
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

inline CPlayerManager::CPlayerManager()
    : m_initialized(false)
    , m_gameBase(0)
    , m_localPlayerId(0)
    , m_lastProcessTime(0)
    , m_lastStaleCheckTime(0)
    , m_gameState(0)
    , m_localPlayerFirstSeenTime(0)
    , m_localPlayerWasEverSeen(false)
{
}

inline CPlayerManager::~CPlayerManager()
{
    Shutdown();
}

inline bool CPlayerManager::Initialize(uintptr_t gameBase)
{
    if (m_initialized)
    {
        PMGR_LOGW("Already initialized");
        return true;
    }

    if (gameBase == 0)
    {
        PMGR_LOGE("Invalid game base address");
        return false;
    }

    m_gameBase = gameBase;
    m_initialized = true;
    m_lastProcessTime = GetTimeMs();
    m_lastStaleCheckTime = GetTimeMs();

    auto& padHooks = CPadHooks::GetInstance();
    if (!padHooks.Initialize(gameBase))
    {
        PMGR_LOGW("CPadHooks initialization failed - continuing without pad hooks");
    }
    else
    {
        PMGR_LOGI("CPadHooks enabled");
    }

    PMGR_LOGI("Player manager initialized, game base: 0x%lx", (unsigned long)gameBase);
    return true;
}

inline void CPlayerManager::Shutdown()
{
    if (!m_initialized)
        return;

    // Remove all players
    {
        std::lock_guard<std::mutex> lock(m_playersMutex);
        m_players.clear();
    }

    m_initialized = false;
    PMGR_LOGI("Player manager shutdown");
}

inline void CPlayerManager::SetCallbacks(const PlayerManagerCallbacks& callbacks)
{
    m_callbacks = callbacks;
}

inline void CPlayerManager::SetLocalPlayerId(uint32_t playerId)
{
    m_localPlayerId = playerId;
    PMGR_LOGI("Local player ID set to %u", playerId);
}

inline CRemotePlayer* CPlayerManager::AddPlayer(uint32_t playerId, const std::string& nickname)
{
    // Don't add local player as remote
    if (playerId == m_localPlayerId)
    {
        PMGR_LOGD("Ignoring local player %u", playerId);
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_playersMutex);

    // Check if already exists
    auto it = m_players.find(playerId);
    if (it != m_players.end())
    {
        PMGR_LOGD("Player %u already exists, updating nickname", playerId);
        it->second->SetNickname(nickname);
        return it->second.get();
    }

    // Allocate a slot for CPad hooks
    uint8_t slot = AllocateSlot();
    if (slot == 0)
    {
        PMGR_LOGE("No slots available for player %u (max %d remote players)",
                  playerId, MAX_REMOTE_SLOT - MIN_REMOTE_SLOT + 1);
        return nullptr;
    }

    // Create new player
    auto player = std::make_unique<CRemotePlayer>(playerId, nickname);
    CRemotePlayer* playerPtr = player.get();

    m_players[playerId] = std::move(player);
    m_playerToSlot[playerId] = slot;
    m_slotToPlayer[slot] = playerId;

    PMGR_LOGI("Added remote player %u: %s (slot: %d, total: %zu)",
              playerId, nickname.c_str(), slot, m_players.size());

    // Notify callback
    if (m_callbacks.onPlayerAdded)
    {
        m_callbacks.onPlayerAdded(playerId, nickname);
    }

    return playerPtr;
}

inline void CPlayerManager::RemovePlayer(uint32_t playerId)
{
    std::lock_guard<std::mutex> lock(m_playersMutex);

    auto it = m_players.find(playerId);
    if (it == m_players.end())
    {
        PMGR_LOGD("Player %u not found for removal", playerId);
        return;
    }

    std::string nickname = it->second->GetNickname();

    // Free the slot
    auto slotIt = m_playerToSlot.find(playerId);
    if (slotIt != m_playerToSlot.end())
    {
        uint8_t slot = slotIt->second;
        FreeSlot(slot);
        m_slotToPlayer.erase(slot);
        m_playerToSlot.erase(slotIt);
        PMGR_LOGD("Freed slot %d for player %u", slot, playerId);

        auto& padHooks = CPadHooks::GetInstance();
        if (padHooks.IsInitialized())
        {
            padHooks.SetRemotePlayerPed(slot, 0);
            padHooks.SetRemotePlayerKeys(slot, 128, 128, 0);
        }
    }

    m_players.erase(it);

    PMGR_LOGI("Removed remote player %u: %s (remaining: %zu)",
              playerId, nickname.c_str(), m_players.size());

    // Notify callback (outside lock would be safer, but keeping simple)
    if (m_callbacks.onPlayerRemoved)
    {
        m_callbacks.onPlayerRemoved(playerId);
    }
}

inline CRemotePlayer* CPlayerManager::GetPlayer(uint32_t playerId)
{
    std::lock_guard<std::mutex> lock(m_playersMutex);

    auto it = m_players.find(playerId);
    if (it == m_players.end())
        return nullptr;

    return it->second.get();
}

inline const CRemotePlayer* CPlayerManager::GetPlayer(uint32_t playerId) const
{
    std::lock_guard<std::mutex> lock(m_playersMutex);

    auto it = m_players.find(playerId);
    if (it == m_players.end())
        return nullptr;

    return it->second.get();
}

inline bool CPlayerManager::HasPlayer(uint32_t playerId) const
{
    std::lock_guard<std::mutex> lock(m_playersMutex);
    return m_players.find(playerId) != m_players.end();
}

inline size_t CPlayerManager::GetPlayerCount() const
{
    std::lock_guard<std::mutex> lock(m_playersMutex);
    return m_players.size();
}

inline std::vector<uint32_t> CPlayerManager::GetAllPlayerIds() const
{
    std::lock_guard<std::mutex> lock(m_playersMutex);

    std::vector<uint32_t> ids;
    ids.reserve(m_players.size());

    for (const auto& pair : m_players)
    {
        ids.push_back(pair.first);
    }

    return ids;
}

inline void CPlayerManager::UpdatePlayerSync(uint32_t playerId, const RemoteSyncData& data)
{
    // DEBUG: Verify this function is being called
    static int debugCounter = 0;
    if (++debugCounter % 50 == 1)
    {
        PMGR_LOGI(">>> UpdatePlayerSync called for player %u (localId=%u, gameState=%d)",
                  playerId, m_localPlayerId, m_gameState);
    }

    // Don't process sync for local player
    if (playerId == m_localPlayerId)
        return;

    // Validate position data - don't spawn at origin (0,0,0)
    bool hasValidPosition = (data.position.x != 0.0f || data.position.y != 0.0f || data.position.z != 0.0f);

    // Check if game is ready for ped creation (state 9 = GS_PLAYING_GAME)
    // ALSO check that local player exists - we can't create remote peds before local player spawns
    bool gameReady = (m_gameState == 9);

    // Check if local player is ACTUALLY spawned using CGameBypass flag
    // This is more reliable than FindPlayerPed(0) which might return remote peds
    bool localPlayerExists = MTA::Android::Game::CGameBypass::GetInstance().IsLocalPlayerSpawned();

    if (!localPlayerExists)
    {
        PMGR_LOGD("Local player not spawned yet - deferring remote player spawn");
    }

    // Track when local player first appeared for timing-based spawn delay
    uint64_t now = GetTimeMs();
    if (localPlayerExists && !m_localPlayerWasEverSeen)
    {
        m_localPlayerFirstSeenTime = now;
        m_localPlayerWasEverSeen = true;
        PMGR_LOGI("Local player first seen at time %llu - starting %d ms spawn delay",
                  (unsigned long long)now, REMOTE_PED_SPAWN_DELAY_MS);
    }

    // Calculate if spawn delay has passed (ensures game/rendering fully initialized)
    bool spawnDelayPassed = false;
    if (m_localPlayerWasEverSeen)
    {
        uint64_t elapsed = now - m_localPlayerFirstSeenTime;
        spawnDelayPassed = (elapsed >= REMOTE_PED_SPAWN_DELAY_MS);
        if (!spawnDelayPassed)
        {
            PMGR_LOGD("Spawn delay: %llu/%d ms elapsed", (unsigned long long)elapsed, REMOTE_PED_SPAWN_DELAY_MS);
        }
    }

    // Must have game ready AND local player exists AND spawn delay passed
    bool canSpawnRemote = gameReady && localPlayerExists && spawnDelayPassed;

    std::lock_guard<std::mutex> lock(m_playersMutex);

    auto it = m_players.find(playerId);
    if (it == m_players.end())
    {
        // Auto-create player on first sync reception
        PMGR_LOGI("Auto-creating player %u from sync data at pos=(%.1f,%.1f,%.1f) valid=%d gameReady=%d",
                  playerId, data.position.x, data.position.y, data.position.z, hasValidPosition, gameReady);
        std::string autoNickname = "Player" + std::to_string(playerId);
        auto player = std::make_unique<CRemotePlayer>(playerId, autoNickname);

        // Allocate a slot for CPad hooks
        uint8_t slot = AllocateSlot();
        if (slot == 0)
        {
            PMGR_LOGE("No slots available for auto-created player %u (max %d remote players)",
                      playerId, MAX_REMOTE_SLOT - MIN_REMOTE_SLOT + 1);
        }
        else
        {
            m_playerToSlot[playerId] = slot;
            m_slotToPlayer[slot] = playerId;
            PMGR_LOGI("Auto-created player %u assigned slot %d", playerId, slot);
        }

        // Only spawn if we have valid position AND game is ready AND local player exists
        if (hasValidPosition && canSpawnRemote)
        {
            CVector3D spawnPos(data.position.x, data.position.y, data.position.z);
            PMGR_LOGI("Auto-spawning player %u at (%.1f,%.1f,%.1f) with skin 0",
                      playerId, spawnPos.x, spawnPos.y, spawnPos.z);
            player->Spawn(spawnPos, data.rotation, 0);
        }
        else if (!hasValidPosition)
        {
            PMGR_LOGW("Deferring spawn for player %u - waiting for valid position", playerId);
        }
        else if (!localPlayerExists)
        {
            PMGR_LOGW("Deferring spawn for player %u - waiting for local player to spawn", playerId);
        }
        else if (!spawnDelayPassed)
        {
            PMGR_LOGW("Deferring spawn for player %u - waiting for %d ms spawn delay", playerId, REMOTE_PED_SPAWN_DELAY_MS);
        }
        else
        {
            PMGR_LOGW("Deferring spawn for player %u - waiting for game ready (state=%d)", playerId, m_gameState);
        }

        m_players[playerId] = std::move(player);
        it = m_players.find(playerId);

        // Notify callback
        if (m_callbacks.onPlayerAdded)
        {
            m_callbacks.onPlayerAdded(playerId, autoNickname);
        }
    }
    else if (!it->second->IsSpawned() && hasValidPosition && canSpawnRemote)
    {
        // Player exists but not spawned yet - try spawning now with valid position, game ready, and local player exists
        PMGR_LOGI("Deferred spawn for player %u at (%.1f,%.1f,%.1f)",
                  playerId, data.position.x, data.position.y, data.position.z);
        CVector3D spawnPos(data.position.x, data.position.y, data.position.z);
        it->second->Spawn(spawnPos, data.rotation, 0);
    }

    it->second->UpdateSyncData(data);

    // Update CPad hooks with key data for animations
    auto slotIt = m_playerToSlot.find(playerId);
    if (slotIt != m_playerToSlot.end())
    {
        uint8_t slot = slotIt->second;
        auto& padHooks = CPadHooks::GetInstance();
        if (padHooks.IsInitialized())
        {
            const RemoteSyncData& padData = it->second->GetSyncData();
            padHooks.SetRemotePlayerKeys(
                slot,
                padData.controllerLeftStickX,
                padData.controllerLeftStickY,
                padData.keyFlags);
        }
    }
}

inline void CPlayerManager::SpawnPlayer(uint32_t playerId, const CVector3D& position,
                                         float rotation, uint16_t skinId)
{
    std::lock_guard<std::mutex> lock(m_playersMutex);

    auto it = m_players.find(playerId);
    if (it == m_players.end())
    {
        PMGR_LOGW("Cannot spawn unknown player %u", playerId);
        return;
    }

    it->second->Spawn(position, rotation, skinId);

    PMGR_LOGI("Spawned player %u at (%.1f, %.1f, %.1f)",
              playerId, position.x, position.y, position.z);

    // Notify callback
    if (m_callbacks.onPlayerSpawned)
    {
        m_callbacks.onPlayerSpawned(playerId, position);
    }
}

inline void CPlayerManager::DespawnPlayer(uint32_t playerId)
{
    std::lock_guard<std::mutex> lock(m_playersMutex);

    auto it = m_players.find(playerId);
    if (it == m_players.end())
        return;

    it->second->Despawn();
    PMGR_LOGI("Despawned player %u", playerId);
}

inline void CPlayerManager::Process()
{
    if (!m_initialized)
        return;

    uint64_t now = GetTimeMs();

    auto& padHooks = CPadHooks::GetInstance();
    const bool padHooksReady = padHooks.IsInitialized();

    // Lock and process all players
    {
        std::lock_guard<std::mutex> lock(m_playersMutex);

        for (auto& pair : m_players)
        {
            auto* player = pair.second.get();
            player->Update(now);

            if (padHooksReady)
            {
                auto slotIt = m_playerToSlot.find(pair.first);
                if (slotIt != m_playerToSlot.end())
                {
                    padHooks.SetRemotePlayerPed(slotIt->second, player->GetPedPtr());
                }
            }
        }
    }

    // Periodically check for stale players
    if (now - m_lastStaleCheckTime >= STALE_CHECK_INTERVAL_MS)
    {
        RemoveStalePlayers();
        m_lastStaleCheckTime = now;
    }

    m_lastProcessTime = now;
}

inline void CPlayerManager::RemoveStalePlayers()
{
    std::lock_guard<std::mutex> lock(m_playersMutex);

    std::vector<uint32_t> staleIds;

    for (const auto& pair : m_players)
    {
        if (pair.second->IsSyncStale())
        {
            staleIds.push_back(pair.first);
        }
    }

    for (uint32_t id : staleIds)
    {
        PMGR_LOGW("Removing stale player %u (no sync received)", id);

        std::string nickname;
        auto it = m_players.find(id);
        if (it != m_players.end())
        {
            nickname = it->second->GetNickname();
            m_players.erase(it);
        }

        if (m_callbacks.onPlayerRemoved)
        {
            m_callbacks.onPlayerRemoved(id);
        }
    }
}

inline void CPlayerManager::OnGameStateChange(int newState)
{
    if (newState == m_gameState)
    {
        return;
    }

    m_gameState = newState;
    PMGR_LOGD("Game state changed to %d", newState);

    // State 9 = GS_PLAYING_GAME
    if (newState == 9)
    {
        // Game is ready for players
        PMGR_LOGI("Game is now in playing state - players can be spawned");
    }
}

} // namespace MTA::Android::Multiplayer

#endif // CPLAYER_MANAGER_H
