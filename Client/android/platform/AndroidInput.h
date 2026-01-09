/*
 * MTA:SA Android - Input System
 *
 * Handles all input on Android:
 *   - Touch screen input (taps, gestures, virtual controls)
 *   - Physical gamepad/controller input
 *   - Keyboard input (external keyboards)
 *   - Translation to GTA:SA control format
 *
 * Design:
 *   - Receives raw input events from Java via JNI
 *   - Processes and translates to MTA/GTA:SA format
 *   - Supports customizable virtual control layouts
 *   - Handles both on-foot and in-vehicle controls
 */

#ifndef ANDROID_INPUT_H
#define ANDROID_INPUT_H

#include <cstdint>
#include <array>
#include <functional>
#include <mutex>

#ifdef __ANDROID__
#include <android/input.h>
#include <android/keycodes.h>
#endif

namespace MTA::Android::Platform
{

//=============================================================================
// Input Constants
//=============================================================================

// Maximum simultaneous touch points
constexpr int MAX_TOUCH_POINTERS = 10;

// Maximum connected gamepads
constexpr int MAX_GAMEPADS = 4;

// Virtual control zones
constexpr int MAX_VIRTUAL_BUTTONS = 32;
constexpr int MAX_VIRTUAL_STICKS = 4;

//=============================================================================
// GTA:SA Control Indices (matches game's control system)
//=============================================================================

enum eControllerAction
{
    // On-foot controls
    CONTROL_FORWARDS = 0,
    CONTROL_BACKWARDS,
    CONTROL_LEFT,
    CONTROL_RIGHT,
    CONTROL_ZOOM_IN,
    CONTROL_ZOOM_OUT,
    CONTROL_ENTER_EXIT,
    CONTROL_CHANGE_CAMERA,
    CONTROL_JUMP,
    CONTROL_SPRINT,
    CONTROL_LOOK_BEHIND,
    CONTROL_CROUCH,
    CONTROL_ACTION,
    CONTROL_WALK,
    CONTROL_VEHICLE_FIRE,
    CONTROL_VEHICLE_SECONDARY_FIRE,
    CONTROL_VEHICLE_LEFT,
    CONTROL_VEHICLE_RIGHT,
    CONTROL_STEER_FORWARDS_DOWN,
    CONTROL_STEER_BACKWARDS_UP,
    CONTROL_ACCELERATE,
    CONTROL_BRAKE_REVERSE,
    CONTROL_RADIO_NEXT,
    CONTROL_RADIO_PREVIOUS,
    CONTROL_RADIO_USER_TRACK_SKIP,
    CONTROL_HORN,
    CONTROL_SUB_MISSION,
    CONTROL_HANDBRAKE,
    CONTROL_VEHICLE_LOOK_LEFT,
    CONTROL_VEHICLE_LOOK_RIGHT,
    CONTROL_VEHICLE_LOOK_BEHIND,
    CONTROL_VEHICLE_MOUSE_LOOK,
    CONTROL_SPECIAL_CONTROL_LEFT,
    CONTROL_SPECIAL_CONTROL_RIGHT,
    CONTROL_SPECIAL_CONTROL_DOWN,
    CONTROL_SPECIAL_CONTROL_UP,
    CONTROL_AIM_WEAPON,
    CONTROL_CONVERSATION_YES,
    CONTROL_CONVERSATION_NO,
    CONTROL_GROUP_CONTROL_FORWARDS,
    CONTROL_GROUP_CONTROL_BACK,

    NUM_CONTROLS
};

//=============================================================================
// Touch State
//=============================================================================

struct TouchPoint
{
    int32_t id;           // Pointer ID
    float x;              // X position (0-1 normalized)
    float y;              // Y position (0-1 normalized)
    float pressure;       // Pressure (0-1)
    float size;           // Touch size
    bool active;          // Is this touch point active?
    uint64_t downTime;    // Time when touch started (ms)
};

struct TouchState
{
    std::array<TouchPoint, MAX_TOUCH_POINTERS> pointers;
    int activeCount;

    // Gesture detection
    bool isTapping;
    bool isDragging;
    bool isPinching;
    float pinchScale;
    float dragDeltaX;
    float dragDeltaY;
};

//=============================================================================
// Gamepad State
//=============================================================================

struct GamepadState
{
    bool connected;
    int32_t deviceId;
    char name[128];

    // Analog sticks (-1 to 1)
    float leftStickX;
    float leftStickY;
    float rightStickX;
    float rightStickY;

    // Triggers (0 to 1)
    float leftTrigger;
    float rightTrigger;

    // D-pad
    bool dpadUp;
    bool dpadDown;
    bool dpadLeft;
    bool dpadRight;

    // Face buttons
    bool buttonA;         // Cross/A
    bool buttonB;         // Circle/B
    bool buttonX;         // Square/X
    bool buttonY;         // Triangle/Y

    // Shoulder buttons
    bool leftShoulder;
    bool rightShoulder;

    // Stick buttons
    bool leftStickButton;
    bool rightStickButton;

    // Menu buttons
    bool startButton;
    bool selectButton;
};

//=============================================================================
// Virtual Control Definition
//=============================================================================

enum class VirtualControlType
{
    Button,           // Simple tap button
    Stick,            // Analog stick
    Slider,           // Horizontal/vertical slider
    DPad,             // 4/8 directional pad
    TouchArea         // Generic touch area
};

struct VirtualControl
{
    VirtualControlType type;
    eControllerAction action;       // Primary action
    eControllerAction altAction;    // Secondary action (for sticks)

    // Position and size (normalized 0-1)
    float x, y;
    float width, height;

    // Appearance
    uint32_t textureId;
    float opacity;
    bool visible;

    // State
    bool pressed;
    float valueX;     // For sticks/sliders
    float valueY;
    int touchId;      // Which touch point is controlling this
};

//=============================================================================
// Control Mapping
//=============================================================================

struct ControlMapping
{
    // Keyboard mappings (Android keycode -> control)
    std::array<eControllerAction, 256> keyboardMap;

    // Gamepad button mappings
    eControllerAction gamepadA;
    eControllerAction gamepadB;
    eControllerAction gamepadX;
    eControllerAction gamepadY;
    eControllerAction gamepadLB;
    eControllerAction gamepadRB;
    eControllerAction gamepadStart;
    eControllerAction gamepadSelect;
    eControllerAction gamepadL3;
    eControllerAction gamepadR3;

    // Analog assignments
    bool leftStickMove;       // Left stick controls movement
    bool rightStickCamera;    // Right stick controls camera
    bool triggersAccelBrake;  // Triggers for vehicle accel/brake
};

//=============================================================================
// Input Callbacks
//=============================================================================

using TouchCallback = std::function<void(int action, int pointerId, float x, float y)>;
using KeyCallback = std::function<void(int keyCode, bool down)>;
using GamepadCallback = std::function<void(int deviceId, int axis, float value)>;

//=============================================================================
// AndroidInput - Main Input Handler
//=============================================================================

class AndroidInput
{
public:
    static AndroidInput& Instance();

    // Initialization
    bool Initialize(int screenWidth, int screenHeight);
    void Shutdown();

    // Screen dimensions (for touch normalization)
    void SetScreenSize(int width, int height);

    //=========================================================================
    // Input Event Handlers (called from JNI)
    //=========================================================================

    // Touch events
    void OnTouchEvent(int action, int pointerId, float x, float y, float pressure);
    void OnTouchDown(int pointerId, float x, float y);
    void OnTouchMove(int pointerId, float x, float y);
    void OnTouchUp(int pointerId);
    void OnTouchCancel();

    // Keyboard events
    void OnKeyEvent(int keyCode, int action, int metaState);
    void OnKeyDown(int keyCode);
    void OnKeyUp(int keyCode);

    // Gamepad events
    void OnGamepadConnected(int deviceId, const char* name);
    void OnGamepadDisconnected(int deviceId);
    void OnGamepadButton(int deviceId, int button, bool pressed);
    void OnGamepadAxis(int deviceId, int axis, float value);

    //=========================================================================
    // State Queries
    //=========================================================================

    // Get current touch state
    const TouchState& GetTouchState() const { return m_touchState; }

    // Get gamepad state
    const GamepadState& GetGamepadState(int index = 0) const;
    bool IsGamepadConnected(int index = 0) const;

    // Control state queries
    bool IsControlPressed(eControllerAction control) const;
    float GetControlValue(eControllerAction control) const;

    // Raw input queries
    bool IsKeyDown(int keyCode) const;
    bool IsTouchActive() const;
    int GetActiveTouchCount() const;

    //=========================================================================
    // Virtual Controls
    //=========================================================================

    // Setup virtual controls
    void SetupDefaultVirtualControls();
    void LoadVirtualControlLayout(const char* layoutFile);
    void SaveVirtualControlLayout(const char* layoutFile);

    // Virtual control management
    int AddVirtualButton(eControllerAction action, float x, float y, float size);
    int AddVirtualStick(eControllerAction moveAction, float x, float y, float size);
    void RemoveVirtualControl(int id);
    void SetVirtualControlVisible(int id, bool visible);
    void SetVirtualControlOpacity(float opacity);

    // Get virtual controls for rendering
    const std::array<VirtualControl, MAX_VIRTUAL_BUTTONS>& GetVirtualControls() const
    {
        return m_virtualControls;
    }

    //=========================================================================
    // Control Mapping
    //=========================================================================

    void SetControlMapping(const ControlMapping& mapping);
    const ControlMapping& GetControlMapping() const { return m_controlMapping; }
    void ResetToDefaultMapping();

    //=========================================================================
    // Mode Switching
    //=========================================================================

    void SetOnFootMode();      // Configure for walking/running
    void SetInVehicleMode();   // Configure for driving
    void SetAimingMode();      // Configure for weapon aiming

    //=========================================================================
    // Callbacks
    //=========================================================================

    void SetTouchCallback(TouchCallback callback) { m_touchCallback = callback; }
    void SetKeyCallback(KeyCallback callback) { m_keyCallback = callback; }

    //=========================================================================
    // Frame Update
    //=========================================================================

    void Update(float deltaTime);

    // Apply input to game
    void ApplyToGame();

private:
    AndroidInput();
    ~AndroidInput();
    AndroidInput(const AndroidInput&) = delete;
    AndroidInput& operator=(const AndroidInput&) = delete;

    // Internal methods
    void ProcessTouchForVirtualControls(int pointerId, float x, float y, bool down);
    void UpdateControlStates();
    void DetectGestures();
    VirtualControl* FindVirtualControlAt(float x, float y);

    // Apply deadzone to analog values
    float ApplyDeadzone(float value, float deadzone = 0.15f);

    // Convert screen coordinates to normalized (0-1)
    void NormalizeCoordinates(float& x, float& y);

private:
    bool m_initialized;
    int m_screenWidth;
    int m_screenHeight;

    // Input states
    TouchState m_touchState;
    std::array<GamepadState, MAX_GAMEPADS> m_gamepadStates;
    std::array<bool, 256> m_keyStates;

    // Control states (combined from all inputs)
    std::array<bool, NUM_CONTROLS> m_controlPressed;
    std::array<float, NUM_CONTROLS> m_controlValues;

    // Virtual controls
    std::array<VirtualControl, MAX_VIRTUAL_BUTTONS> m_virtualControls;
    int m_virtualControlCount;

    // Control mapping
    ControlMapping m_controlMapping;

    // Callbacks
    TouchCallback m_touchCallback;
    KeyCallback m_keyCallback;

    // Thread safety
    mutable std::mutex m_mutex;

    // Mode
    enum class InputMode { OnFoot, InVehicle, Aiming };
    InputMode m_currentMode;

    // Gesture detection state
    uint64_t m_lastTapTime;
    float m_lastTapX, m_lastTapY;
    bool m_doubleTapDetected;
};

//=============================================================================
// Inline Implementations
//=============================================================================

inline AndroidInput& AndroidInput::Instance()
{
    static AndroidInput instance;
    return instance;
}

inline bool AndroidInput::IsControlPressed(eControllerAction control) const
{
    if (control >= NUM_CONTROLS) return false;
    return m_controlPressed[control];
}

inline float AndroidInput::GetControlValue(eControllerAction control) const
{
    if (control >= NUM_CONTROLS) return 0.0f;
    return m_controlValues[control];
}

inline bool AndroidInput::IsKeyDown(int keyCode) const
{
    if (keyCode < 0 || keyCode >= 256) return false;
    return m_keyStates[keyCode];
}

inline bool AndroidInput::IsTouchActive() const
{
    return m_touchState.activeCount > 0;
}

inline int AndroidInput::GetActiveTouchCount() const
{
    return m_touchState.activeCount;
}

inline const GamepadState& AndroidInput::GetGamepadState(int index) const
{
    if (index < 0 || index >= MAX_GAMEPADS)
    {
        static GamepadState empty = {};
        return empty;
    }
    return m_gamepadStates[index];
}

inline bool AndroidInput::IsGamepadConnected(int index) const
{
    if (index < 0 || index >= MAX_GAMEPADS) return false;
    return m_gamepadStates[index].connected;
}

inline float AndroidInput::ApplyDeadzone(float value, float deadzone)
{
    if (value > -deadzone && value < deadzone)
        return 0.0f;

    // Rescale to full range
    if (value > 0)
        return (value - deadzone) / (1.0f - deadzone);
    else
        return (value + deadzone) / (1.0f - deadzone);
}

} // namespace MTA::Android::Platform

#endif // ANDROID_INPUT_H
