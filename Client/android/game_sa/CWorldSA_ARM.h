/*
 * MTA:SA Android - CWorldSA ARM Implementation
 *
 * ARM-compatible implementation of CWorld functionality.
 * Provides world-level operations: collision, line of sight, entity management.
 */

#ifndef CWORLDSA_ARM_H
#define CWORLDSA_ARM_H

#include "CVehicleSA_ARM.h"
#include "GameSA_Platform.h"

namespace MTA::Android::GameSA
{

//=============================================================================
// Collision Structures
//=============================================================================

struct CColPointSA
{
    CVector m_vecPoint;          // Collision point position
    float   m_fUnk1;
    CVector m_vecNormal;         // Surface normal
    float   m_fUnk2;
    uint8_t m_nSurfaceTypeA;     // Surface type A
    uint8_t m_nPieceTypeA;       // Piece type A
    uint8_t m_nLightingA;        // Lighting A
    uint8_t m_nSurfaceTypeB;     // Surface type B
    uint8_t m_nPieceTypeB;       // Piece type B
    uint8_t m_nLightingB;        // Lighting B
    float   m_fDepth;            // Penetration depth
};

struct CColLineSA
{
    CVector m_vecStart;
    float   m_fStartSize;
    CVector m_vecEnd;
    float   m_fEndSize;
};

//=============================================================================
// Line of Sight Flags
//=============================================================================

struct SLineOfSightFlags
{
    bool bCheckBuildings;
    bool bCheckVehicles;
    bool bCheckPeds;
    bool bCheckObjects;
    bool bCheckDummies;
    bool bSeeThroughStuff;
    bool bIgnoreSomeObjectsForCamera;
    bool bShootThroughStuff;

    SLineOfSightFlags()
        : bCheckBuildings(true)
        , bCheckVehicles(true)
        , bCheckPeds(true)
        , bCheckObjects(true)
        , bCheckDummies(true)
        , bSeeThroughStuff(false)
        , bIgnoreSomeObjectsForCamera(false)
        , bShootThroughStuff(false)
    {
    }
};

//=============================================================================
// World Sectors
//=============================================================================

struct CSectorSA
{
    void* m_pBuildings;     // Doubly linked list of buildings
    void* m_pDummies;       // Doubly linked list of dummies
};

struct CRepeatSectorSA
{
    void* m_pLodList;       // LOD list
    void* m_pVehicles;      // Vehicles in sector
    void* m_pPeds;          // Peds in sector
    void* m_pObjects;       // Objects in sector
};

//=============================================================================
// CWorldSA - World operations (static functions)
//=============================================================================

class CWorldSA
{
public:
    //=========================================================================
    // Line of Sight Tests
    //=========================================================================

    /**
     * Process line of sight test
     * @param start Start position
     * @param end End position
     * @param colPoint Output collision point
     * @param entity Output entity hit (if any)
     * @param flags LOS check flags
     * @param ignoredEntity Entity to ignore in check
     * @return true if line is blocked
     */
    static bool ProcessLineOfSight(
        const CVector& start,
        const CVector& end,
        CColPointSA* colPoint,
        CEntitySAInterface** entity,
        const SLineOfSightFlags& flags,
        CEntitySAInterface* ignoredEntity = nullptr)
    {
        using FuncType = bool(*)(
            const CVector*, const CVector*,
            CColPointSA*, CEntitySAInterface**,
            bool, bool, bool, bool, bool, bool, bool, bool,
            CEntitySAInterface*);

        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CWorld_ProcessLineOfSight));

        return func(
            &start, &end,
            colPoint, entity,
            flags.bCheckBuildings,
            flags.bCheckVehicles,
            flags.bCheckPeds,
            flags.bCheckObjects,
            flags.bCheckDummies,
            flags.bSeeThroughStuff,
            flags.bIgnoreSomeObjectsForCamera,
            flags.bShootThroughStuff,
            ignoredEntity);
    }

    /**
     * Simple line of sight test (just returns blocked or not)
     */
    static bool IsLineOfSightClear(
        const CVector& start,
        const CVector& end,
        bool checkBuildings = true,
        bool checkVehicles = true,
        bool checkPeds = true,
        bool checkObjects = true,
        bool checkDummies = true,
        bool seeThroughStuff = false,
        bool ignoreSomeObjects = false)
    {
        using FuncType = bool(*)(
            const CVector*, const CVector*,
            bool, bool, bool, bool, bool, bool, bool);

        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CWorld_IsLineOfSightClear));

        return func(
            &start, &end,
            checkBuildings, checkVehicles, checkPeds,
            checkObjects, checkDummies, seeThroughStuff, ignoreSomeObjects);
    }

    //=========================================================================
    // Ground/Height Tests
    //=========================================================================

    /**
     * Find ground Z coordinate at position
     */
    static float FindGroundZForCoord(float x, float y)
    {
        using FuncType = float(*)(float, float);
        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CWorld_FindGroundZForCoord));
        return func(x, y);
    }

    /**
     * Find ground Z with additional info
     */
    static float FindGroundZFor3DCoord(float x, float y, float z, bool* outWaterGround = nullptr)
    {
        using FuncType = float(*)(float, float, float, bool*);
        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CWorld_FindGroundZFor3DCoord));
        return func(x, y, z, outWaterGround);
    }

    /**
     * Find lowest Z coordinate at position
     */
    static float FindLowestZForCoord(float x, float y)
    {
        using FuncType = float(*)(float, float);
        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CWorld_FindLowestZForCoord));
        return func(x, y);
    }

    /**
     * Get water level at position
     */
    static bool GetWaterLevel(float x, float y, float z, float* outLevel, bool checkWaves = false)
    {
        using FuncType = bool(*)(float, float, float, float*, bool);
        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CWorld_GetWaterLevel));
        return func(x, y, z, outLevel, checkWaves);
    }

    //=========================================================================
    // Entity Management
    //=========================================================================

    /**
     * Add entity to world
     */
    static void Add(CEntitySAInterface* entity)
    {
        using FuncType = void(*)(CEntitySAInterface*);
        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CWorld_Add));
        func(entity);
    }

    /**
     * Remove entity from world
     */
    static void Remove(CEntitySAInterface* entity)
    {
        using FuncType = void(*)(CEntitySAInterface*);
        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CWorld_Remove));
        func(entity);
    }

    /**
     * Find objects in range
     */
    static int16_t FindObjectsInRange(
        const CVector& position,
        float radius,
        bool check2D,
        int16_t* outCount,
        int16_t maxCount,
        CEntitySAInterface** outEntities,
        bool checkBuildings,
        bool checkVehicles,
        bool checkPeds,
        bool checkObjects,
        bool checkDummies)
    {
        using FuncType = int16_t(*)(
            const CVector*, float, bool, int16_t*, int16_t,
            CEntitySAInterface**, bool, bool, bool, bool, bool);

        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CWorld_FindObjectsInRange));

        return func(
            &position, radius, check2D, outCount, maxCount, outEntities,
            checkBuildings, checkVehicles, checkPeds, checkObjects, checkDummies);
    }

    /**
     * Find objects intersecting a line
     */
    static void FindObjectsIntersectingCube(
        const CVector& corner1,
        const CVector& corner2,
        int16_t* outCount,
        int16_t maxCount,
        CEntitySAInterface** outEntities,
        bool checkBuildings,
        bool checkVehicles,
        bool checkPeds,
        bool checkObjects,
        bool checkDummies)
    {
        using FuncType = void(*)(
            const CVector*, const CVector*, int16_t*, int16_t,
            CEntitySAInterface**, bool, bool, bool, bool, bool);

        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CWorld_FindObjectsIntersectingCube));

        func(
            &corner1, &corner2, outCount, maxCount, outEntities,
            checkBuildings, checkVehicles, checkPeds, checkObjects, checkDummies);
    }

    //=========================================================================
    // Collision Tests
    //=========================================================================

    /**
     * Test sphere against world
     */
    static bool TestSphereAgainstWorld(
        const CVector& position,
        float radius,
        CEntitySAInterface* ignoreEntity,
        bool checkBuildings,
        bool checkVehicles,
        bool checkPeds,
        bool checkObjects,
        bool checkDummies,
        bool ignoreSomeStuff)
    {
        using FuncType = bool(*)(
            const CVector*, float, CEntitySAInterface*,
            bool, bool, bool, bool, bool, bool);

        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CWorld_TestSphereAgainstWorld));

        return func(
            &position, radius, ignoreEntity,
            checkBuildings, checkVehicles, checkPeds,
            checkObjects, checkDummies, ignoreSomeStuff);
    }

    /**
     * Clear the world of excluded entities
     */
    static void ClearExcitingStuffFromArea(
        const CVector& position,
        float radius,
        bool removeFrozenPeds)
    {
        using FuncType = void(*)(const CVector*, float, bool);
        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CWorld_ClearExcitingStuffFromArea));
        func(&position, radius, removeFrozenPeds);
    }

    //=========================================================================
    // Weather/Time
    //=========================================================================

    /**
     * Set current area (interior)
     */
    static void SetCurrentArea(uint8_t area)
    {
        using FuncType = void(*)(uint8_t);
        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CWorld_SetCurrentArea));
        func(area);
    }

    /**
     * Get current area (interior)
     */
    static uint8_t GetCurrentArea()
    {
        // This is typically a global variable read
        // For now, return 0 (exterior)
        return 0;
    }

    //=========================================================================
    // Camera Collision
    //=========================================================================

    /**
     * Process camera collision
     */
    static void ProcessCamera(const CVector& position)
    {
        using FuncType = void(*)(const CVector*);
        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CWorld_ProcessCamera));
        func(&position);
    }

    //=========================================================================
    // Streaming
    //=========================================================================

    /**
     * Request a model to be loaded
     */
    static void RequestModel(int32_t modelId, int32_t flags)
    {
        using FuncType = void(*)(int32_t, int32_t);
        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CStreaming_RequestModel));
        func(modelId, flags);
    }

    /**
     * Load all requested models
     */
    static void LoadAllRequestedModels(bool onlyPriority)
    {
        using FuncType = void(*)(bool);
        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CStreaming_LoadAllRequestedModels));
        func(onlyPriority);
    }

    /**
     * Check if model is loaded
     */
    static bool HasModelLoaded(int32_t modelId)
    {
        using FuncType = bool(*)(int32_t);
        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CStreaming_HasModelLoaded));
        return func(modelId);
    }

    /**
     * Remove model from memory
     */
    static void RemoveModel(int32_t modelId)
    {
        using FuncType = void(*)(int32_t);
        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CStreaming_RemoveModel));
        func(modelId);
    }

private:
    // No instances - static class
    CWorldSA() = delete;
};

//=============================================================================
// CColModelSA - Collision model interface
//=============================================================================

struct CColModelSA
{
    CVector m_vecBoundingBoxMin;
    CVector m_vecBoundingBoxMax;
    CVector m_vecBoundingSphereCenter;
    float   m_fBoundingSphereRadius;

    void*   m_pColData;  // CColData*

    /**
     * Check if a line intersects this collision model
     */
    bool TestLine(const CVector& start, const CVector& end, CColPointSA& colPoint)
    {
        using FuncType = bool(*)(CColModelSA*, const CVector*, const CVector*, CColPointSA*);
        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CColModel_TestLine));
        return func(this, &start, &end, &colPoint);
    }

    /**
     * Check sphere collision
     */
    bool TestSphere(const CVector& position, float radius)
    {
        using FuncType = bool(*)(CColModelSA*, const CVector*, float);
        auto func = reinterpret_cast<FuncType>(
            GetGameAddress(GameAddr::CColModel_TestSphere));
        return func(this, &position, radius);
    }
};

//=============================================================================
// Raycast Helper
//=============================================================================

struct CRaycastResult
{
    bool bHit;
    CVector vecHitPosition;
    CVector vecHitNormal;
    CEntitySAInterface* pHitEntity;
    uint8_t nSurfaceType;
    float fDistance;

    CRaycastResult()
        : bHit(false)
        , vecHitPosition()
        , vecHitNormal()
        , pHitEntity(nullptr)
        , nSurfaceType(0)
        , fDistance(0.0f)
    {
    }
};

/**
 * Perform a raycast in the world
 */
inline CRaycastResult PerformRaycast(
    const CVector& start,
    const CVector& end,
    const SLineOfSightFlags& flags = SLineOfSightFlags(),
    CEntitySAInterface* ignoreEntity = nullptr)
{
    CRaycastResult result;
    CColPointSA colPoint;
    CEntitySAInterface* hitEntity = nullptr;

    result.bHit = CWorldSA::ProcessLineOfSight(
        start, end, &colPoint, &hitEntity, flags, ignoreEntity);

    if (result.bHit)
    {
        result.vecHitPosition = colPoint.m_vecPoint;
        result.vecHitNormal = colPoint.m_vecNormal;
        result.pHitEntity = hitEntity;
        result.nSurfaceType = colPoint.m_nSurfaceTypeB;

        // Calculate distance
        float dx = colPoint.m_vecPoint.x - start.x;
        float dy = colPoint.m_vecPoint.y - start.y;
        float dz = colPoint.m_vecPoint.z - start.z;
        result.fDistance = sqrtf(dx * dx + dy * dy + dz * dz);
    }

    return result;
}

} // namespace MTA::Android::GameSA

#endif // CWORLDSA_ARM_H
