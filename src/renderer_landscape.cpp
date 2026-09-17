#include "renderer.h"
#include "map.h"
#include "texture.h"
#include "perf.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>
#include <cstdlib>

static constexpr int POND_W=640, POND_H=360;

bool Renderer::initLandscape(const char* base) {
    char p[768],v[768],f[768];
    snprintf(p,sizeof(p),"%stextures/meadow_sky.png",base);
    meadowSky=loadTexture(p);
    if(!meadowSky) meadowSky=loadTexture("textures/meadow_sky.png");
    if(!meadowSky) return false;
    glBindTexture(GL_TEXTURE_2D,meadowSky);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    landscapeMapLoc=glGetUniformLocation(shader.program,"landscapeMap");
    shader.use();
    shader.setInt(landscapeMapLoc,8);
    skyPanoramaLoc=glGetUniformLocation(skyShader.program,"panorama");
    skyUsePanoramaLoc=glGetUniformLocation(skyShader.program,"usePanorama");
    skyPanoramaTurnLoc=glGetUniformLocation(skyShader.program,"panoramaTurn");
    snprintf(v,sizeof(v),"%sshaders/pond.vert",base);
    snprintf(f,sizeof(f),"%sshaders/pond.frag",base);
    if(!pondShader.load(v,f) && !pondShader.load("shaders/pond.vert","shaders/pond.frag")) return false;
    pondReflectionLoc=glGetUniformLocation(pondShader.program,"reflectionMap");
    pondTimeLoc=glGetUniformLocation(pondShader.program,"time");
    pondReflectionMixLoc=glGetUniformLocation(pondShader.program,"reflectionMix");
    glGenFramebuffers(1,&pondFBO); glBindFramebuffer(GL_FRAMEBUFFER,pondFBO);
    glGenTextures(1,&pondTexture); glBindTexture(GL_TEXTURE_2D,pondTexture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,POND_W,POND_H,0,GL_RGB,GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,pondTexture,0);
    glGenTextures(1,&pondDepth); glBindTexture(GL_TEXTURE_2D,pondDepth);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,POND_W,POND_H,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,pondDepth,0);
    bool ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    return ok;
}

void Renderer::drawPondReflection(const glm::mat4& view,const glm::mat4& proj,const glm::vec3& eye) {
    static const bool noReflection=getenv("FPS_NOREFLECT")!=nullptr;
    if(noReflection) { pondReflectionMix=0; return; }
    if(++pondFrame%2 && pondReflectionMix>0) return;
    pondReflectionMix=0;
    if(gMapId!=MAP_LOBBY || eye.y<TRAINING_POND_Y) return;
    Frustum fr=Frustum::fromVP(proj*view);
    if(!fr.aabbVisible({0,TRAINING_POND_Y,TRAINING_POND_Z},{50,1,36})) return;
    float pondDistance=glm::distance(eye,glm::vec3(0,TRAINING_POND_Y,TRAINING_POND_Z));
    if(pondDistance>350) return;
    pondReflectionMix=1-glm::smoothstep(250.0f,350.0f,pondDistance);
    glm::mat4 mirror=glm::scale(glm::translate(glm::mat4(1),{0,2*TRAINING_POND_Y,0}),{1,-1,1});
    glm::mat4 reflected=view*mirror;
    glm::vec3 reflectedEye(eye.x,2*TRAINING_POND_Y-eye.y,eye.z);
    glBindFramebuffer(GL_FRAMEBUFFER,pondFBO); glViewport(0,0,POND_W,POND_H);
    reflectionPass=true;
    beginFrame(reflected,proj,reflectedEye);
    drawSky(reflected,proj,reflectedEye);
    glDisable(GL_CULL_FACE);
    Frustum reflectionFr=Frustum::fromVP(proj*reflected);
    drawTerrain(reflectionFr,reflectedEye);
    drawVegetation(reflectionFr,reflectedEye);
    reflectionPass=false;
    glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(0,0,fbW,fbH);
    glEnable(GL_CULL_FACE);
}

void Renderer::drawPond() {
    pondShader.use();
    glm::mat4 model=glm::scale(glm::translate(glm::mat4(1),{0,TRAINING_POND_Y,TRAINING_POND_Z}),{1.0f,1,.72f});
    pondShader.setMat4(pondShader.locModel,model);
    pondShader.setMat4(pondShader.locView,curView);
    pondShader.setMat4(pondShader.locProj,curProj);
    pondShader.setFloat(pondTimeLoc,frameTime);
    pondShader.setFloat(pondReflectionMixLoc,pondReflectionMix);
    pondShader.setVec3(pondShader.locSkyZenith,skyZenith);
    pondShader.setVec3(pondShader.locSkyHorizon,skyHorizon);
    pondShader.setFloat(pondShader.locExposure,exposure);
    pondShader.setFloat(pondShader.locSaturation,saturation);
    // Eye position recovered from the view to include leaning/third person.
    pondShader.setVec3(pondShader.locEye,glm::vec3(glm::inverse(curView)[3]));
    pondShader.setInt(pondReflectionLoc,7);
    glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D,pondTexture);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_CULL_FACE);
    ground.draw();
    glEnable(GL_CULL_FACE);
    shader.use();
}

void Renderer::destroyLandscape() {
    pondShader.destroy();
    if(meadowSky) glDeleteTextures(1,&meadowSky);
    if(pondTexture) glDeleteTextures(1,&pondTexture);
    if(pondDepth) glDeleteTextures(1,&pondDepth);
    if(pondFBO) glDeleteFramebuffers(1,&pondFBO);
    meadowSky=pondTexture=pondDepth=pondFBO=0;
}
