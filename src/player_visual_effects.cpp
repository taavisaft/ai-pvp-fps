#include "player_visual.h"
#include "playerpose.h"
#include <glm/gtc/matrix_transform.hpp>

void fireUziCosmetics(ViewModel& vm,const Player& player,float crouch,const PoseAnim& anim) {
    Player uzi=player; uzi.weaponId=WEP_UZI;
    glm::mat4 hold=playerWeaponHold(uzi,crouch,vm.adsT,anim);
    vm.uzi.shot(glm::translate(hold,glm::vec3(0,.03f,.06f)));
}
