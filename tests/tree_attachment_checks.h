#pragma once
#include "tree_collision.h"
#include <algorithm>

// Inspect emitted mesh geometry, not generator metadata. Every wood segment must
// meet previously connected wood; the atlas's cut stem must meet its twig tip.
inline int checkTreeAttachments(const std::vector<float>& v,const std::vector<unsigned>& idx,int species) {
    struct Branch { glm::vec3 root,tip; float r0,r1; };
    std::vector<Branch> branches{{{0,0,0},{0,TREE_TRUNK_HEIGHT,0},TREE_TRUNK_BASE,TREE_TRUNK_TOP}};
    auto pos=[&](unsigned i) { return glm::vec3(v[i*12],v[i*12+1],v[i*12+2]); };
    int failures=0,cards=0;
    size_t at=TREE_TRUNK_SIDES*12; // side triangles plus both caps
    while(at<idx.size()) {
        unsigned first=idx[at];
        if(v[first*12+10]>=0) {
            const float stemU[]={.476f,.457f,.453f};
            float anchor=species<0 ? .5f : (stemU[species]-.012f)/.976f;
            glm::vec3 stem=glm::mix(pos(first),pos(first+1),anchor);
            bool attached=glm::distance(stem,branches.back().tip)<.00001f;
            // Spruce inner sprays and the terminal leader may use an earlier tip.
            if(species<0) for(const auto& b:branches)
                if(glm::distance(stem,b.tip)<.00001f) { attached=true; break; }
            if(!attached) ++failures;
            int rows=v[(first+2)*12+11]>.9f ? 2 : 3;
            at+=(rows-1)*6; ++cards; continue;
        }
        int sides=0;
        do {
            if(at+size_t(sides)*6+5>=idx.size() || sides>=6) return failures+1;
            ++sides;
        } while(idx[at+size_t(sides-1)*6+1]!=first);
        glm::vec3 root(0),tip(0);
        for(int j=0;j<sides;++j) { root+=pos(first+j*2); tip+=pos(first+j*2+1); }
        root/=float(sides); tip/=float(sides);
        float r0=glm::distance(root,pos(first)),r1=glm::distance(tip,pos(first+1));
        bool attached=false;
        for(const auto& b:branches) {
            glm::vec3 axis=b.tip-b.root;
            float t=glm::clamp(glm::dot(root-b.root,axis)/glm::dot(axis,axis),0.0f,1.0f);
            if(glm::distance(root,b.root+axis*t)<=glm::mix(b.r0,b.r1,t)+.00001f) { attached=true; break; }
        }
        if(!attached) ++failures;
        branches.push_back({root,tip,r0,r1});
        at+=sides*6;
    }
    return failures+(cards==0);
}
