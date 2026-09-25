#include "camera.h"
#include "game.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>

int main() {
    int failures=0;
    auto check=[&](bool ok,const char* label) {
        if(!ok) { std::fprintf(stderr,"camera look: %s\n",label); ++failures; }
    };
    Camera hip; hip.yaw=0; hip.addLook(1,1);
    check(std::fabs(hip.yaw-.1f)<1e-6f,"hip sensitivity preserved");
    const float screenStep=std::tan(glm::radians(hip.yaw))/std::tan(glm::radians(HIP_FOV*.5f));
    for(float fov : {HIP_FOV, 65.0f, ADS_FOV, 30.0f, SCOPE_8X_FOV}) {
        Camera cam; cam.yaw=0; cam.fov=fov; cam.addLook(1,1);
        float step=std::tan(glm::radians(cam.yaw))/std::tan(glm::radians(fov*.5f));
        check(std::fabs(step/screenStep-1)<1e-4f,"one-count reticle movement stays consistent through zoom");
        check(cam.yaw>0 && std::fabs(cam.pitch+cam.yaw)<1e-6f,"both axes retain tiny movements");
    }
    Camera scope; scope.yaw=0; scope.fov=SCOPE_8X_FOV; scope.addLook(1,0);
    check(std::fabs(scope.yaw-.0125f)<1e-6f,"8x reduces angular step eightfold");
    Camera batched=scope, separate=scope;
    batched.addLook(100,-100);
    for(int i=0;i<100;++i) separate.addLook(1,-1);
    check(std::fabs(batched.yaw-separate.yaw)<1e-5f && std::fabs(batched.pitch-separate.pitch)<1e-5f,
          "small events accumulate without truncation");
    scope.pitch=88.99f; scope.addLook(0,-1000);
    check(scope.pitch==89,"pitch remains clamped");
    return failures ? 1 : 0;
}
