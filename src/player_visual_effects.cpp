#include "player_visual.h"
#include "playerpose.h"
#include <glm/gtc/matrix_transform.hpp>

void fireUziCosmetics(ViewModel& vm,const Camera& cam,const Player& player,bool thirdPerson) {
    if(!thirdPerson) { vm.uzi.shot(viewWeaponMount(cam,vm,WEP_UZI)); return; }
    PoseBox boxes[MAX_POSE_BOXES];
    int count=buildPlayerPose(player.pos,player.yaw,player.pitch,player.lean,WEP_UZI,
        player.crouched,player.ads,0,0,boxes);
    for(int i=0;i<count;++i) if(boxes[i].part==POSE_GUN_METAL) {
        glm::mat4 hold=glm::translate(boxes[i].M,glm::vec3(0,-.02f,-.16f));
        vm.uzi.shot(glm::translate(hold,glm::vec3(0,.03f,.06f)));
        return;
    }
}
