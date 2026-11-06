#pragma once
#define XR_NO_PROTOTYPES
#include <openxr/openxr.h>
#include "vr_input_state.h"
void vr_input_init(XrInstance instance,PFN_xrGetInstanceProcAddr get);
void vr_input_attach(XrSession session);
void vr_input_poll(XrSession session,bool focused);
void vr_input_clear(void);
void vr_input_profiles(XrSession session);
