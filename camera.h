// Include necessary Windows and math headers.
#include <windows.h>
#include <math.h>
#include "math_linear.h"

// Define movement speeds (tweak these constants as needed)
#define PAN_SPEED 0.005f  // Speed multiplier for panning
#define ORBIT_SPEED 0.05f // Speed multiplier for orbit/rotation
#define ZOOM_STEP 0.1f    // 0.0001f // Speed multiplier for dolly zoom
//------------------------------------------------------------------------------
// camera struct definition in C style using vec3 for vectors.
// Holds the camera's position, target, up vector, and orbit parameters.
typedef struct camera
{
    vec3 position;  // camera position in world space
    vec3 target;    // The point the camera is looking at
    vec3 up;        // Up vector (should be normalized, e.g. {0, 1, 0})
    float distance; // Distance from the camera to the target (for orbiting)
    float yaw;      // Horizontal rotation angle (in radians)
    float pitch;    // Vertical rotation angle (in radians)
} camera;

// Define the world up vector.
const vec3 WORLD_UP = {0.0f, 1.0f, 0.0f};

/*
 * @brief Computes the right vector of the camera based on its position, target, and up vector.
 *
 * This function calculates the right vector of the camera, which is perpendicular to both
 * the forward vector (from the camera's position to its target) and the up vector. It also
 * ensures that the up vector is orthogonal to the forward and right vectors by recomputing it.
 *
 * @param cam Pointer to the camera structure. If the pointer is null, a zero vector is returned.
 *
 * @return The right vector of the camera as a normalized vec3. If the camera pointer is null,
 *         a zero vector (vec3{0.0f, 0.0f, 0.0f}) is returned.
 */
vec3 get_camera_right_vec(camera *cam)
{
    if (!cam)
        return (vec3){0.0f, 0.0f, 0.0f}; // Return zero vector if camera is null

    // Compute the forward vector (from camera position to target)
    vec3 forward = cam->target - cam->position;
    forward = vector_normalize(forward);

    // Compute the right vector (perpendicular to forward and up)
    vec3 right = vector_cross(forward, cam->up);
    right = vector_normalize(right);

    // Recompute the up vector to ensure orthogonality
    cam->up = vector_cross(right, forward);

    return right;
}

/**
 * @brief Updates the position and orientation of the camera based on its target, distance, pitch, and yaw.
 *
 * This function recalculates the camera's position, forward, right, and up vectors to ensure
 * it is correctly oriented and positioned relative to its target. The camera's position is
 * determined by applying pitch and yaw rotations to an offset vector, and its orientation
 * vectors are recalculated to maintain a consistent view direction.
 *
 * @param cam Pointer to the camera object to update. If the pointer is null, the function returns immediately.
 *
 * Steps:
 * 1. Initializes the camera's offset position along the X-axis based on its distance from the target.
 * 2. Applies the pitch rotation around the initial right axis.
 * 3. Applies the yaw rotation around the world up axis.
 * 4. Updates the camera's position by adding the rotated offset to the target position.
 * 5. Computes the forward vector as the normalized direction from the camera's position to its target.
 * 6. Computes the right vector as the normalized cross product of the forward vector and the world up vector.
 * 7. Computes the up vector as the normalized cross product of the right and forward vectors to ensure no roll.
 */
static void update_camera_position(camera *cam)
{
    if (!cam)
        return;

    // 1. Start with the camera at a default position (distance units along X-axis)
    vec3 offset = {cam->distance, 0.0f, 0.0f};

    // 2. Apply pitch rotation first (around the initial right axis, which is WORLD_RIGHT = {1,0,0})
    vec3 initial_right = {0, 0.0f, -1.0f}; // Since offset starts at {distance,0,0}
    offset = vector_rotate(offset, initial_right, cam->pitch);

    // 3. Apply yaw rotation (around WORLD_UP)
    offset = vector_rotate(offset, WORLD_UP, cam->yaw);

    // 4. Set the final camera position (target + rotated offset)
    cam->position = cam->target + offset;

    // 5. Compute the new forward vector (camera to target)
    vec3 forward = vector_normalize(cam->target - cam->position);

    // 6. Recompute the right vector (perpendicular to forward and WORLD_UP)
    vec3 right = vector_cross(forward, WORLD_UP);
    right = vector_normalize(right);

    // 7. Recompute the up vector to ensure no roll
    cam->up = vector_cross(right, forward);
    cam->up = vector_normalize(cam->up);
}

//------------------------------------------------------------------------------
// Global variables to store the last mouse position and interaction state.
// These help calculate the delta movement.
static int last_mouse_x = 0;
static int last_mouse_y = 0;
static int is_rotating = 0;
static int is_panning = 0;

//------------------------------------------------------------------------------
// Function: process_camera_input
// Purpose: Processes Windows messages related to mouse input to control the
// camera. The behavior is as follows:
//    - Shift + Right Click: Pan the camera (move the target and camera
//      position parallel to the view plane).
//    - Right Click (without Shift): Orbit the camera around the target.
//    - Mouse Wheel: Dolly zoom (adjust the distance from the target).
//
// Parameters:
//    - cam: Pointer to the camera struct to be updated.
//    - hwnd: Handle to the window (needed for screen-to-client coordinate conversion).
//    - msg: Windows message identifier.
//    - w_param, l_param: Message parameters containing details about the input event.
//
// Error handling and edge cases are considered (e.g., null camera pointer).
void process_camera_input(camera *cam, HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
{
    if (!cam)
        return; // Error handling: Ensure the camera pointer is valid

    int mouse_x, mouse_y;
    switch (msg)
    {
        // Handle right mouse button press.
    // Handle right mouse button press.
    case WM_RBUTTONDOWN:
    {
        // Retrieve current mouse position in screen coordinates.
        POINT pt;
        if (GetCursorPos(&pt))
        {
            // Convert screen coordinates to client coordinates.
            if (ScreenToClient(hwnd, &pt))
            {
                last_mouse_x = pt.x;
                last_mouse_y = pt.y;
            }
        }
        // Check if the SHIFT key is held down.
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
        {
            is_panning = 1;
            is_rotating = 0;
        }
        else
        {
            is_rotating = 1;
            is_panning = 0;
        }
        break;
    }

    // Handle right mouse button release.
    case WM_RBUTTONUP:
    {
        // Reset flags when the right button is released.
        is_rotating = 0;
        is_panning = 0;
        break;
    }

    // Handle mouse movement.
    case WM_MOUSEMOVE:
    {
        // Process movement only if the right button is held down.
        if (w_param & MK_RBUTTON)
        {
            // l_param contains the current client coordinates.
            POINT pt;
            pt.x = LOWORD(l_param);
            pt.y = HIWORD(l_param);
            mouse_x = pt.x;
            mouse_y = pt.y;

            // Calculate the difference from the last recorded mouse position.
            int delta_x = mouse_x - last_mouse_x;
            int delta_y = mouse_y - last_mouse_y;

            if (is_panning)
            {
                // Panning: Move the camera's position and target parallel to the view plane.
                // Compute the forward vector (view direction).
                vec3 forward = cam->target - cam->position;
                forward = vector_normalize(forward);

                // Compute the right vector in the view plane using the world up.
                vec3 right = vector_cross(forward, WORLD_UP);
                right = vector_normalize(right);

                // Compute the "pan up" vector as perpendicular to both right and forward.
                vec3 pan_up = vector_cross(right, forward);
                pan_up = vector_normalize(pan_up);

                // Determine pan offsets based on mouse delta and PAN_SPEED.
                float pan_offset_x = -delta_x * PAN_SPEED;
                float pan_offset_y = delta_y * PAN_SPEED;

                // Update the target and camera position in the view plane.
                cam->target.x += right.x * pan_offset_x + pan_up.x * pan_offset_y;
                cam->target.y += right.y * pan_offset_x + pan_up.y * pan_offset_y;
                cam->target.z += right.z * pan_offset_x + pan_up.z * pan_offset_y;

                cam->position.x += right.x * pan_offset_x + pan_up.x * pan_offset_y;
                cam->position.y += right.y * pan_offset_x + pan_up.y * pan_offset_y;
                cam->position.z += right.z * pan_offset_x + pan_up.z * pan_offset_y;
            }
            else if (is_rotating)
            {
                // Rotation:
                // Horizontal mouse movement rotates the camera around the world vertical axis (yaw).
                // Vertical mouse movement pitches the camera (pitch).

                float this_orbit = delta_x * ORBIT_SPEED;

                cam->yaw -= this_orbit;              // delta_x * ORBIT_SPEED;
                cam->pitch -= delta_y * ORBIT_SPEED; // Subtract to pitch up when moving mouse upward.

                // Clamp pitch to avoid flipping the camera (gimbal lock).
                if (cam->pitch > 1.56f)
                    cam->pitch = 1.56f; // ~90 degrees in radians.
                if (cam->pitch < -1.56f)
                    cam->pitch = -1.56f;

                // Update the camera position based on the new yaw and pitch.
                // Make sure update_camera_position uses WORLD_UP for yaw rotation.
                update_camera_position(cam);
            }

            // Update last mouse position for the next movement calculation.
            last_mouse_x = mouse_x;
            last_mouse_y = mouse_y;
        }
        break;
    }

    // Handle the mouse wheel event for dolly zoom.
    case WM_MOUSEWHEEL:
    {
        // Extract the wheel delta (z_delta) from w_param.
        int z_delta = (short)HIWORD(w_param);

        // Calculate the number of notches scrolled (WHEEL_DELTA is 120).
        int notches = z_delta / WHEEL_DELTA;

        // Adjust the camera's distance by a fixed amount per notch.
        cam->distance -= notches * ZOOM_STEP;

        // Clamp the distance to a minimum value to avoid inverting the camera.
        if (cam->distance < 0.1f)
            cam->distance = 0.1f;

        // Update camera position based on the new distance.
        update_camera_position(cam);
        break;
    }
    default:
        break;
    }
}

//------------------------------------------------------------------------------
// Function: view_matrix_from_cam
// Purpose: Computes the view matrix for the given camera object.
// The view matrix transforms world coordinates to camera (view) space.
//
// Parameters:
//    - cam: Pointer to the camera struct.
//
// Returns:
//    - A mat4 representing the view matrix.
mat4 view_matrix_from_cam(const camera *cam)
{
    if (!cam)
        return mat4_identity();

    // Compute the forward vector (from camera to target)
    vec3 forward = vector_normalize(cam->target - cam->position);
    // Compute the right vector (must use cam->up to ensure stability)
    vec3 right = vector_normalize(vector_cross(forward, cam->up));

    // Recompute the true up vector (ensures orthogonality)
    vec3 up = vector_normalize(vector_cross(right, forward));

    // Construct the view matrix (column-major)
    mat4 view = {{// Column 0: Right vector
                  {right.x, up.x, -forward.x, 0.0f},
                  // Column 1: Up vector
                  {right.y, up.y, -forward.y, 0.0f},
                  // Column 2: Negative forward vector
                  {right.z, up.z, -forward.z, 0.0f},
                  // Column 3: Translation (dot products)
                  {-vector_dot(right, cam->position),
                   -vector_dot(up, cam->position),
                   vector_dot(forward, cam->position),
                   1.0f}}};

    return view;
}