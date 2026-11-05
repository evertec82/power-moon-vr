// Read-only OpenXR runtime/headset preflight. Does not change the active runtime.
#define XR_NO_PROTOTYPES
#include <windows.h>
#include <openxr/openxr.h>
#include <stdio.h>
int main(void) {
    HMODULE dll=LoadLibraryA("openxr_loader.dll");
    if(!dll) { printf("OpenXR loader unavailable (%lu)\n",GetLastError()); return 1; }
    PFN_xrGetInstanceProcAddr get=(PFN_xrGetInstanceProcAddr)(void *)GetProcAddress(dll,"xrGetInstanceProcAddr");
    if(!get) return 2;
    PFN_xrCreateInstance create; PFN_xrGetSystem system; PFN_xrGetInstanceProperties properties; PFN_xrDestroyInstance destroy;
    get(XR_NULL_HANDLE,"xrCreateInstance",(PFN_xrVoidFunction *)&create);
    const char *ext="XR_KHR_vulkan_enable";
    XrInstanceCreateInfo ci={.type=XR_TYPE_INSTANCE_CREATE_INFO,.enabledExtensionCount=1,.enabledExtensionNames=&ext};
    strcpy(ci.applicationInfo.applicationName,"Power Moon VR preflight"); ci.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);
    XrInstance instance; XrResult r=create(&ci,&instance);
    printf("xrCreateInstance: %d\n",r); if(XR_FAILED(r)) return 3;
    get(instance,"xrGetInstanceProperties",(PFN_xrVoidFunction *)&properties);
    get(instance,"xrGetSystem",(PFN_xrVoidFunction *)&system);
    get(instance,"xrDestroyInstance",(PFN_xrVoidFunction *)&destroy);
    XrInstanceProperties ip={.type=XR_TYPE_INSTANCE_PROPERTIES};
    r=properties(instance,&ip); if(XR_SUCCEEDED(r)) printf("Runtime: %s\n",ip.runtimeName);
    XrSystemGetInfo si={.type=XR_TYPE_SYSTEM_GET_INFO,.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY};
    XrSystemId id; r=system(instance,&si,&id); printf("xrGetSystem (headset): %d\n",r);
    destroy(instance); FreeLibrary(dll); return XR_SUCCEEDED(r)?0:4;
}
