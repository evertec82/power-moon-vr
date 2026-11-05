#pragma once
#include <openxr/openxr.h>
// Fixed in LOCAL reference space. The live head pose is deliberately not an
// input: only recentering changes the anchor used for the next submission.
static XrPosef vr_menu_panel_pose(XrPosef anchor,float distance) {
    XrQuaternionf q=anchor.orientation;
    anchor.position.x-=distance*2*(q.x*q.z+q.y*q.w);
    anchor.position.y-=distance*2*(q.y*q.z-q.x*q.w);
    anchor.position.z-=distance*(1-2*(q.x*q.x+q.y*q.y));
    return anchor;
}
