#pragma once
#include <stdbool.h>
// Captured on the game thread with its completed frame, then consumed in queue order.
typedef struct {
    float fovy,aspect,nearz,farz;
    float orientation[4],position[3];
    bool valid,title_menu;
} VrCameraState;
void vr_camera_snapshot(VrCameraState *out);
void vr_camera_queued(const VrCameraState *state);
