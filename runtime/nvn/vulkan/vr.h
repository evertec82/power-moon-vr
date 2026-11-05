// Experimental OpenXR stereo rendering. All functions run under the render lock.
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
void vr_frame(InterpFrame *frame, NvnTexture *texture, uint32_t epoch);
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
