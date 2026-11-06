#define XR_NO_PROTOTYPES
#include <windows.h>
#include <openxr/openxr.h>
#include <stdio.h>
#include <stdlib.h>
int main(void){HMODULE d=LoadLibraryA("openxr_loader.dll");if(!d)return 1;PFN_xrGetInstanceProcAddr g=(void *)GetProcAddress(d,"xrGetInstanceProcAddr");PFN_xrEnumerateInstanceExtensionProperties f=NULL;g(XR_NULL_HANDLE,"xrEnumerateInstanceExtensionProperties",(PFN_xrVoidFunction *)&f);uint32_t n=0;XrResult r=f(NULL,0,&n,NULL);if(XR_FAILED(r))return 2;XrExtensionProperties *p=calloc(n,sizeof *p);for(uint32_t i=0;i<n;i++)p[i].type=XR_TYPE_EXTENSION_PROPERTIES;r=f(NULL,n,&n,p);printf("Extension query %d, %u entries\n",r,n);for(uint32_t i=0;i<n;i++)if(strstr(p[i].extensionName,"depth")||strstr(p[i].extensionName,"space_warp")||strstr(p[i].extensionName,"space_delta")||strstr(p[i].extensionName,"refresh_rate"))puts(p[i].extensionName);free(p);FreeLibrary(d);return XR_FAILED(r)?3:0;}
