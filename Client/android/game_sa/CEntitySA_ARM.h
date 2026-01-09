/*
 * MTA:SA Android - CEntitySA ARM Implementation
 *
 * ARM-compatible implementation of CEntitySA.
 * This file provides the same interface as the original CEntitySA
 * but uses ARM-compatible function calls instead of x86-specific code.
 *
 * Design:
 *   - Uses GameSA_Platform.h for address resolution
 *   - Replaces __thiscall casts with portable function pointers
 *   - Replaces __asm blocks with C++ code
 */

#ifndef CENTITYSA_ARM_H
#define CENTITYSA_ARM_H

#include "GameSA_Platform.h"
#include <cstdint>

// Forward declarations
struct RpClump;
struct RwFrame;
struct RwMatrix;
struct RwV3d;
class CVector;
class CMatrix;
class CRect;

namespace MTA::Android::GameSA
{

//=============================================================================
// CPlaceableSAInterface - Base class for positioned objects
//=============================================================================

struct CSimpleTransform
{
    float x, y, z;      // Position
    float heading;      // Rotation around Z axis
};

struct CPlaceableSAInterface
{
    void*            vtable;        // 0x00 - Virtual table pointer
    CSimpleTransform m_transform;   // 0x04 - Position and heading
    CMatrix*         matrix;        // 0x14 - Full transformation matrix (may be null)
};

//=============================================================================
// CEntitySAInterface - Base entity interface
//=============================================================================

struct CEntitySAInterface : public CPlaceableSAInterface
{
    // Mirrors the virtual table from the original
    // vtable[0] = destructor
    // vtable[1] = Add()
    // vtable[2] = Add(rect)
    // vtable[3] = Remove()
    // etc.

    RpClump*      m_pRwObject;       // 0x18 - RenderWare clump
    uint32_t      m_nEntityFlags;    // 0x1C - Entity flags (bitfield)
    uint16_t      m_nRandomSeed;     // 0x20 - Random seed
    uint16_t      m_nModelIndex;     // 0x22 - Model ID
    void*         m_pReferences;     // 0x24 - Reference list
    void*         m_pStreamingLink;  // 0x28 - Streaming system link
    int16_t       m_nScanCode;       // 0x2C - Scan code for collision
    int8_t        m_nIplIndex;       // 0x2E - IPL index
    uint8_t       m_nAreaCode;       // 0x2F - Interior area code
    // Note: Actual size depends on game version

    //=========================================================================
    // ARM-Compatible Member Functions
    //=========================================================================

    /**
     * Transform a point from object space to world space
     */
    void TransformFromObjectSpace(CVector& outPosn, const CVector& offset)
    {
        using FuncType = void(*)(CEntitySAInterface*, CVector&, const CVector&);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CEntity_TransformFromObjectSpace));
        func(this, outPosn, offset);
    }

    /**
     * Get the center of the bounding box
     */
    CVector* GetBoundCentre(CVector* pOutCentre)
    {
        using FuncType = CVector*(*)(CEntitySAInterface*, CVector*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CEntity_GetBoundCentre));
        return func(this, pOutCentre);
    }

    /**
     * Update the RenderWare object's position/rotation
     */
    void UpdateRW()
    {
        using FuncType = void(*)(CEntitySAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CEntity_UpdateRW));
        func(this);
    }

    /**
     * Update hierarchical animation
     */
    void UpdateRpHAnim()
    {
        using FuncType = void(*)(CEntitySAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CEntity_UpdateRpHAnim));
        func(this);
    }

    /**
     * Set the RenderWare object's alpha (transparency)
     */
    void SetRwObjectAlpha(uint8_t alpha)
    {
        using FuncType = void(*)(CEntitySAInterface*, uint8_t);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CEntity_SetRwObjectAlpha));
        func(this, alpha);
    }

    /**
     * Check if the entity is visible
     */
    bool IsVisible()
    {
        using FuncType = bool(*)(CEntitySAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CEntity_IsVisible));
        return func(this);
    }

    /**
     * Get distance from center of mass to base of model
     */
    float GetDistanceFromCentreOfMassToBaseOfModel()
    {
        using FuncType = float(*)(CEntitySAInterface*);
        auto func = reinterpret_cast<FuncType>(GetGameAddress(GameAddr::CEntity_GetDistanceFromCentreOfMassToBaseOfModel));
        return func(this);
    }

    //=========================================================================
    // Virtual Function Calls (using vtable)
    //=========================================================================

    /**
     * Add entity to world
     */
    void Add()
    {
        // Call through vtable - index 1 (after destructor)
        using VFunc = void(*)(CEntitySAInterface*);
        auto vtable = *reinterpret_cast<VFunc**>(this);
        vtable[1](this);
    }

    /**
     * Remove entity from world
     */
    void Remove()
    {
        using VFunc = void(*)(CEntitySAInterface*);
        auto vtable = *reinterpret_cast<VFunc**>(this);
        vtable[3](this);
    }

    /**
     * Set model index
     */
    void SetModelIndex(uint32_t modelIndex)
    {
        using VFunc = void(*)(CEntitySAInterface*, uint32_t);
        auto vtable = *reinterpret_cast<VFunc**>(this);
        reinterpret_cast<VFunc>(vtable[5])(this, modelIndex);
    }

    /**
     * Create the RenderWare object
     */
    void CreateRwObject()
    {
        using VFunc = void(*)(CEntitySAInterface*);
        auto vtable = *reinterpret_cast<VFunc**>(this);
        vtable[7](this);
    }

    /**
     * Delete the RenderWare object
     */
    void DeleteRwObject()
    {
        using VFunc = void(*)(CEntitySAInterface*);
        auto vtable = *reinterpret_cast<VFunc**>(this);
        vtable[8](this);
    }

    /**
     * Process control (game logic update)
     */
    void ProcessControl()
    {
        using VFunc = void(*)(CEntitySAInterface*);
        auto vtable = *reinterpret_cast<VFunc**>(this);
        vtable[10](this);
    }

    /**
     * Render the entity
     */
    void Render()
    {
        using VFunc = void(*)(CEntitySAInterface*);
        auto vtable = *reinterpret_cast<VFunc**>(this);
        vtable[14](this);
    }

    /**
     * Teleport to position
     */
    void Teleport(const CVector& destination, bool resetRotation)
    {
        using VFunc = void(*)(CEntitySAInterface*, const CVector&, bool);
        auto vtable = *reinterpret_cast<VFunc**>(this);
        reinterpret_cast<VFunc>(vtable[13])(this, destination, resetRotation);
    }
};

//=============================================================================
// CEntitySA - MTA wrapper class for CEntitySAInterface
//=============================================================================

class CEntitySA
{
public:
    CEntitySA();
    virtual ~CEntitySA();

    // Interface access
    CEntitySAInterface* GetInterface() { return m_pInterface; }
    void SetInterface(CEntitySAInterface* pInterface) { m_pInterface = pInterface; }

    //=========================================================================
    // Position/Transform
    //=========================================================================

    void SetPosition(float x, float y, float z);
    void SetPosition(const CVector& pos);
    void GetPosition(CVector& outPos);
    CVector* GetPosition();

    void SetRotation(float x, float y, float z);
    void GetRotation(CVector& outRot);

    void SetMatrix(const CMatrix& matrix);
    void GetMatrix(CMatrix& outMatrix);
    CMatrix* GetMatrix();

    //=========================================================================
    // Model/Rendering
    //=========================================================================

    uint16_t GetModelIndex();
    void SetModelIndex(uint16_t modelIndex);

    void SetAlpha(uint8_t alpha);
    uint8_t GetAlpha();

    bool IsVisible();
    void SetVisible(bool visible);

    bool IsOnScreen();

    //=========================================================================
    // RenderWare Access
    //=========================================================================

    RpClump* GetRpClump();
    RwFrame* GetRwFrame();

    void UpdateRpHAnim();
    bool SetScaleInternal(const CVector& scale);

    //=========================================================================
    // World Interaction
    //=========================================================================

    void Teleport(float x, float y, float z);
    float GetDistanceFromCentreOfMassToBaseOfModel();

    //=========================================================================
    // Entity State
    //=========================================================================

    bool IsBeingDeleted() { return m_bBeingDeleted; }
    void SetBeingDeleted(bool b) { m_bBeingDeleted = b; }

    bool GetDoNotRemoveFromGame() { return m_bDoNotRemoveFromGame; }
    void SetDoNotRemoveFromGame(bool b) { m_bDoNotRemoveFromGame = b; }

protected:
    CEntitySAInterface* m_pInterface;
    bool                m_bBeingDeleted;
    bool                m_bDoNotRemoveFromGame;
    void*               m_pStoredPointer;
    CVector             m_LastGoodPosition;

    // Called when position is about to change
    virtual void OnChangingPosition(const CVector& newPos) {}
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * Get the RwFrame from an entity's RpClump
 */
inline RwFrame* GetEntityFrame(CEntitySAInterface* entity)
{
    if (!entity || !entity->m_pRwObject)
        return nullptr;

    // RpClump->object.parent points to RwFrame
    struct RpClumpHeader {
        uint8_t type;
        uint8_t subType;
        uint8_t flags;
        uint8_t privateFlags;
        void*   parent;  // RwFrame*
    };

    auto* clump = reinterpret_cast<RpClumpHeader*>(entity->m_pRwObject);
    return reinterpret_cast<RwFrame*>(clump->parent);
}

/**
 * Get the transformation matrix from an entity
 */
inline CMatrix* GetEntityMatrix(CEntitySAInterface* entity)
{
    if (!entity)
        return nullptr;

    return entity->matrix;
}

} // namespace MTA::Android::GameSA

#endif // CENTITYSA_ARM_H
