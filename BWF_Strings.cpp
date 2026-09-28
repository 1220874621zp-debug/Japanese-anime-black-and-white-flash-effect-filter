#include "BWF_Strings.h"
#include "AEConfig.h"
#ifdef AE_OS_WIN
    #include <Windows.h>
#endif
#include "A.h"

extern "C" {

static A_char kStrName[] = "黑白闪";
static A_char kStrDescription[] = "渲染管线：阈值→SDF距离场→轮廓追踪→Douglas-Peucker简化→法线方向发射→径向模糊";

A_char *GetStringPtr(int strNum) {
    switch (strNum) {
        case StrID_Name: return kStrName;
        case StrID_Description: return kStrDescription;
        default: return (A_char*)"";
    }
}

}
