#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// The cached image must be projected with its capture camera, even when the
// main camera moves on a frame where the low-frequency reflection is reused.
struct PondReflectionCache {
    glm::mat4 captureVP{1}, captureProjection{1}, captureView{1};
    glm::vec3 captureEye{0};
    bool valid=false;
    unsigned age=0;

    void invalidate() { valid=false; age=0; }
    bool needsCapture(const glm::mat4& view,const glm::mat4& projection,const glm::vec3& eye) {
        ++age;
        // Reusing a mirror camera while the player walks or leans makes nearby
        // scenery jump when the capture finally updates. Keep the two-frame
        // cadence only while the camera is effectively still.
        if(!valid || age>=2 || glm::distance(eye,captureEye)>.01f) return true;
        for(int c=0;c<4;++c) for(int r=0;r<4;++r)
            if(glm::abs(view[c][r]-captureView[c][r])>.0005f) return true;
        for(int c=0;c<4;++c) for(int r=0;r<4;++r)
            if(projection[c][r]!=captureProjection[c][r]) return true;
        return false;
    }
    void capture(const glm::mat4& view,const glm::mat4& projection,const glm::vec3& eye,float waterY) {
        glm::mat4 mirror=glm::scale(glm::translate(glm::mat4(1),{0,2*waterY,0}),{1,-1,1});
        captureVP=projection*view*mirror;
        captureProjection=projection;
        captureView=view;
        captureEye=eye;
        valid=true; age=0;
    }
};
