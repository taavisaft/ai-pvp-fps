#pragma once
#include <glm/glm.hpp>
#include "gl_loader.h"

struct Shader {
    GLuint program  = 0;
    GLint  locModel = -1;
    GLint  locView  = -1;
    GLint  locProj  = -1;
    GLint  locColor = -1;
    GLint  locAlpha = -1;
    GLint  locLit   = -1;
    GLint  locEye   = -1;
    GLint  locDiffuse = -1;
    GLint  locUseTex  = -1;
    GLint  locUseFacade = -1;  // basic program; 1 = UV facade sampling (buildings)
    GLint  locTile    = -1;
    GLint  locSpec    = -1;
    GLint  locTint    = -1;
    GLint  locTime    = -1;
    GLint  locGrass   = -1;
    // Daylight palette (basic + sky programs share these names where present)
    GLint  locSunDir    = -1;
    GLint  locSkyZenith = -1;
    GLint  locSkyHorizon= -1;
    GLint  locGroundAmb = -1;
    // Atmosphere pass (basic/foliage/inst/sky where present): preset-driven mood
    GLint  locSunColor  = -1;  // direct sun tint (was hardcoded warm white)
    GLint  locFogDist   = -1;  // distance-fog reach in meters
    GLint  locFogHeight = -1;  // low-altitude fog boost (valleys fill with haze)
    GLint  locCloud     = -1;  // cloud-shadow strength 0..1 (drifting noise)
    GLint  locExposure  = -1;  // pre-tonemap exposure
    GLint  locSaturation= -1;  // color-grade saturation
    GLint  locInvVP     = -1;  // sky program only
    GLint  locLightSpace= -1;  // basic + depth programs
    GLint  locShadowMap = -1;  // basic program; sampler on texture unit 1
    GLint  locUseShadow = -1;  // basic program; 1 = sample shadow map
    GLint  locHasNormal = -1;  // basic program; 1 = use supplied vertex normal
    // Terrain slope/height splat (basic program; terrain draw only)
    GLint  locSplat    = -1;   // 1 = blend grass/dirt/rock by slope+height
    GLint  locRockMap  = -1;   // sampler, texture unit 2
    GLint  locDirtMap  = -1;   // sampler, texture unit 3
    GLint  locForestMap= -1;   // sampler, texture unit 4
    GLint  locRockTile = -1;   // world meters per rock repeat
    GLint  locDirtTile = -1;   // world meters per dirt repeat
    GLint  locForestTile=-1;   // world meters per forest-floor repeat

    bool load(const char* vertPath, const char* fragPath);
    void use() const;
    void setMat4(GLint loc, const glm::mat4& m) const;
    void setVec3(GLint loc, const glm::vec3& v) const;
    void setFloat(GLint loc, float f) const;
    void setInt(GLint loc, int v) const;
    void destroy();
};
