#include "grass.h"
#include "map.h"
#include <cstdio>
#include <cmath>

namespace {
constexpr float SPACING=1.0f, BUILD_RADIUS=90.0f;
constexpr int ANCHOR_CELLS=8, ROWS_PER_FRAME=8;
constexpr size_t MAX_INSTANCES=26000;
float fractf2(float x){return x-floorf(x);}
float shash(float x,float z){x=fractf2(x*123.34f);z=fractf2(z*456.21f);float d=x*(x+45.32f)+z*(z+45.32f);return fractf2((x+d)*(z+d));}
float snoise(float x,float z){float ix=floorf(x),iz=floorf(z),fx=fractf2(x),fz=fractf2(z);fx=fx*fx*(3-2*fx);fz=fz*fz*(3-2*fz);float a=shash(ix,iz),b=shash(ix+1,iz),c=shash(ix,iz+1),d=shash(ix+1,iz+1);return (a+(b-a)*fx)+((c+(d-c)*fx)-(a+(b-a)*fx))*fz;}
float sfbm(float x,float z){float v=0,a=.5f;for(int i=0;i<4;i++){v+=a*snoise(x,z);x*=2;z*=2;a*=.5f;}return v;}
float meshHeight(float x,float z){constexpr float s=2;float x0=floorf((x+PALDISKI_HALF)/s)*s-PALDISKI_HALF,z0=floorf((z+PALDISKI_HALF)/s)*s-PALDISKI_HALF;float tx=(x-x0)/s,tz=(z-z0)/s;float a=terrainHeight(x0,z0),b=terrainHeight(x0+s,z0),c=terrainHeight(x0,z0+s),d=terrainHeight(x0+s,z0+s);return tx<=tz?a*(1-tz)+c*(tz-tx)+d*tx:a*(1-tx)+b*(tx-tz)+d*tz;}
void blade(std::vector<float>&v,std::vector<unsigned>&ix,float x,float z,float ang,float h,float w){glm::vec3 r(cosf(ang)*w,0,sinf(ang)*w),p(x,0,z),n(-sinf(ang),0,cosf(ang));unsigned k=(unsigned)(v.size()/6);for(glm::vec3 q:{p-r,p+r,p+glm::vec3(0,h,0)})v.insert(v.end(),{q.x,q.y,q.z,n.x,n.y,n.z});ix.insert(ix.end(),{k,k+1,k+2});}
}

bool GrassRenderer::init(const char* base){
    char vp[600],fp[600];snprintf(vp,sizeof(vp),"%sshaders/grass.vert",base?base:"");snprintf(fp,sizeof(fp),"%sshaders/grass.frag",base?base:"");
    if(!shader.load(vp,fp)&&!shader.load("shaders/grass.vert","shaders/grass.frag"))return false;
    std::vector<float>v;std::vector<unsigned>ix;for(int i=0;i<11;i++){float a=i*2.399963f,r=.035f+.035f*(i%4);blade(v,ix,cosf(a)*r,sinf(a)*r,a,.42f+.085f*(i%5),.011f+.002f*(i%3));}
    if(!mesh.create(v.data(),v.size(),ix.data(),ix.size(),true))return false;
    glBindVertexArray(mesh.vao);glGenBuffers(1,&instanceVbo);glBindBuffer(GL_ARRAY_BUFFER,instanceVbo);glBufferData(GL_ARRAY_BUFFER,MAX_INSTANCES*sizeof(Instance),nullptr,GL_DYNAMIC_DRAW);glEnableVertexAttribArray(3);glVertexAttribPointer(3,4,GL_FLOAT,GL_FALSE,sizeof(Instance),(void*)0);glVertexAttribDivisor(3,1);glBindVertexArray(0);
    activeInstances.reserve(MAX_INSTANCES);pendingInstances.reserve(MAX_INSTANCES);ready=instanceVbo!=0;return ready;
}

void GrassRenderer::appendRow(int dz){
    int cells=(int)ceilf(BUILD_RADIUS/SPACING);
    for(int dx=-cells;dx<=cells;dx++){
        int gx=buildX+dx,gz=buildZ+dz;float x=(gx+.5f+(terrHash(gx,gz)-.5f)*.78f)*SPACING,z=(gz+.5f+(terrHash(gx+917,gz-283)-.5f)*.78f)*SPACING;
        float ex=x-buildX*SPACING,ez=z-buildZ*SPACING;if(ex*ex+ez*ez>BUILD_RADIUS*BUILD_RADIUS)continue;
        float y=terrainHeight(x,z);if(y<1.35f)continue;float dirt=terrSmooth(terrClamp01((sfbm(x*.004f,z*.004f)-.42f)/.18f));if(dirt>.78f)continue;
        float e=1.25f,sx=(terrainHeight(x+e,z)-terrainHeight(x-e,z))/(2*e),sz=(terrainHeight(x,z+e)-terrainHeight(x,z-e))/(2*e);if(sx*sx+sz*sz>.32f)continue;
        float scale=.78f+terrHash(gx-61,gz+103)*.55f;if(pendingInstances.size()<MAX_INSTANCES)pendingInstances.push_back({glm::vec4(x,meshHeight(x,z)+.015f,z,scale)});
    }
}

void GrassRenderer::uploadActive(){glBindBuffer(GL_ARRAY_BUFFER,instanceVbo);glBufferSubData(GL_ARRAY_BUFFER,0,activeInstances.size()*sizeof(Instance),activeInstances.data());instanceCount=(GLsizei)activeInstances.size();}

void GrassRenderer::updatePatch(const glm::vec3& eye){
    int rawX=(int)floorf(eye.x/SPACING),rawZ=(int)floorf(eye.z/SPACING);int cx=(int)floorf((float)rawX/ANCHOR_CELLS)*ANCHOR_CELLS,cz=(int)floorf((float)rawZ/ANCHOR_CELLS)*ANCHOR_CELLS,cells=(int)ceilf(BUILD_RADIUS/SPACING);
    if(patchMap!=(int)gMapId){buildX=cx;buildZ=cz;pendingInstances.clear();for(int row=-cells;row<=cells;row++)appendRow(row);activeInstances.swap(pendingInstances);patchX=cx;patchZ=cz;patchMap=(int)gMapId;building=false;uploadActive();return;}
    if(cx==patchX&&cz==patchZ&&!building)return;
    if(!building||cx!=buildX||cz!=buildZ){buildX=cx;buildZ=cz;buildRow=-cells;pendingInstances.clear();building=true;}
    for(int n=0;n<ROWS_PER_FRAME&&buildRow<=cells;n++,buildRow++)appendRow(buildRow);
    if(buildRow>cells){activeInstances.swap(pendingInstances);patchX=buildX;patchZ=buildZ;building=false;uploadActive();}
}

void GrassRenderer::draw(const glm::vec3&eye,float time,const glm::vec3&sunDir,const glm::vec3&skyZenith,const glm::vec3&skyHorizon,const glm::vec3&groundAmbient,const glm::vec3&sunColor,float fogDist,float exposure,float saturation){if(!ready)return;updatePatch(eye);if(!instanceCount)return;shader.use();shader.setMat4(shader.locView,view);shader.setMat4(shader.locProj,proj);shader.setVec3(shader.locEye,eye);shader.setFloat(shader.locTime,time);shader.setVec3(shader.locSunDir,sunDir);shader.setVec3(shader.locSkyZenith,skyZenith);shader.setVec3(shader.locSkyHorizon,skyHorizon);shader.setVec3(shader.locGroundAmb,groundAmbient);shader.setVec3(shader.locSunColor,sunColor);shader.setFloat(shader.locFogDist,fogDist);shader.setFloat(shader.locExposure,exposure);shader.setFloat(shader.locSaturation,saturation);glDisable(GL_CULL_FACE);glBindVertexArray(mesh.vao);glDrawElementsInstanced(GL_TRIANGLES,mesh.indexCount,GL_UNSIGNED_INT,nullptr,instanceCount);glEnable(GL_CULL_FACE);}
void GrassRenderer::destroy(){mesh.destroy();if(instanceVbo)glDeleteBuffers(1,&instanceVbo);instanceVbo=0;shader.destroy();activeInstances.clear();pendingInstances.clear();ready=false;}
