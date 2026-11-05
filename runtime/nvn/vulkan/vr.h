// Experimental OpenXR stereo rendering. Rendering runs under the render lock.
// The simulation deadline helper belongs to the game presenting thread.
#pragma once
#include "vk.h"
#include "../interp.h"
bool vr_enabled(void);
int vr_eye_index(void);
bool vr_filter_effects(void);
void vr_bootstrap(void);
void vr_instance_extensions(const char **ext, uint32_t *count, uint32_t capacity);
void vr_device_extensions(const char **ext, uint32_t *count, uint32_t capacity);
VkPhysicalDevice vr_physical_device(VkInstance instance);
void vr_session_init(void);
bool vr_frame(InterpFrame *frame, NvnTexture *texture, uint32_t epoch, uint64_t deadline);
bool vr_simulation_enabled(void);
uint64_t vr_simulation_deadline(uint64_t now);
// Column-major clip-space transform; identity outside an eye replay.
void vr_clip_matrix(float out[16], bool world, bool flip_y, bool depth_minus_one,
                    const void *uniforms, uint64_t bytes);


void vr_game_projection(float fovy,float aspect,float nearz,float farz);

bool vr_camera_target(float *fovy,float *aspect);

void vr_apply_game_camera(float *view_and_pose);

void vr_request_capture(void);

bool vr_high_rate_pacing(void);
bool vr_multiple_samples(void);
bool vr_fps(float *submitted,float *target);

void vr_eye_replay_begin(void);

float vr_ui_scale(void);

// Frontend scene state; captured with each queued game frame.
void vr_set_title_menu(bool active);

// Camera selection; controller clicks are reserved for this toggle in VR.
void vr_toggle_first_person(void);
void vr_poll_view_buttons(uint64_t buttons);
uint64_t vr_filter_view_buttons(uint64_t buttons);
bool vr_first_person_requested(void);
bool vr_first_person_applied(void);
void vr_first_person_frame_begin(void);
