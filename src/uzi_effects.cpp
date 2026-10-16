#include "uzi_effects.h"
#include "terrain.h"
#include <cmath>

float UziEffects::random() {
    seed^=seed<<13; seed^=seed>>17; seed^=seed<<5;
    return float(seed&0xffffffu)/16777216.0f;
}
void UziEffects::clear() {
    for(auto& c:casings) c.life=0;
    shotAge=1; pending=false; next=0;
}
void UziEffects::shot(const glm::mat4& mount) {
    shotAge=0; shotMount=mount; pending=true;
}
float UziEffects::boltOffset() const {
    // Shot ignition is forward; extraction returns the open bolt to its rear rest.
    float t=glm::clamp(shotAge/CYCLE_TIME,0.0f,1.0f);
    return -.030f*t*t*(3-2*t);
}
void UziEffects::eject() {
    Casing& c=casings[next]; next=(next+1)%CAPACITY;
    c=Casing{}; c.life=1.6f;
    c.pos=glm::vec3(shotMount*glm::vec4(PORT,1));
    glm::vec3 right=glm::normalize(glm::vec3(shotMount[0]));
    glm::vec3 up=glm::normalize(glm::vec3(shotMount[1]));
    glm::vec3 forward=glm::normalize(glm::vec3(shotMount[2]));
    c.velocity=right*(1.8f+random()*.8f)+up*(.8f+random()*.5f)-forward*(.3f+random()*.6f);
    c.axis=glm::normalize(glm::vec3(.3f+random(),.5f+random(),random()-.5f));
    c.spin=12+random()*22; c.angle=random()*6.2831853f;
}
void UziEffects::tick(float dt) {
    if(!std::isfinite(dt) || dt<=0) return;
    shotAge=glm::min(shotAge+dt,1.0f);
    if(pending && shotAge>=EJECT_DELAY) { eject(); pending=false; }
    for(auto& c:casings) {
        if(c.life<=0) continue;
        c.life=glm::max(0.0f,c.life-dt);
        c.velocity.y-=9.81f*dt; c.pos+=c.velocity*dt; c.angle+=c.spin*dt;
        float floor=terrainHeight(c.pos.x,c.pos.z)+.005f;
        if(c.pos.y<floor && c.velocity.y<0) {
            c.pos.y=floor;
            if(!c.bounced) { c.velocity.y=-c.velocity.y*.25f; c.velocity.x*=.45f; c.velocity.z*=.45f; c.spin*=.25f; c.bounced=true; }
            else { c.velocity=glm::vec3(0); c.spin=0; }
        }
    }
}
