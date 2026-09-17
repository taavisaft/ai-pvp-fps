#pragma once
// Included after terrain noise helpers. Shared by rendering and collision.
constexpr float TRAINING_POND_Y=6.0f;
constexpr float TRAINING_POND_Z=184.0f;
constexpr float TRAINING_POND_RADIUS_X=44.0f;
constexpr float TRAINING_POND_RADIUS_Z=30.0f;
inline float trainingPondRadius(float x,float z) {
    float dx=x/TRAINING_POND_RADIUS_X,dz=(z-TRAINING_POND_Z)/TRAINING_POND_RADIUS_Z;
    return sqrtf(dx*dx+dz*dz)*(1.0f+.06f*sinf(x*.12f+z*.09f));
}
inline float trainingLandscapeHeight(float x,float z) {
    float h=12.0f+14.0f*terrValueNoise(x*.008f+7,z*.008f-2);
    h+=9.0f*sinf(x*.011f+z*.006f)*sinf(z*.012f+.4f);
    // Open valley framed by overlapping, wooded ridges.
    float ridge=lobbySmooth(260,650,z);
    h+=ridge*(34+52*terrValueNoise(x*.0035f+41,z*.004f+13));
    h+=lobbySmooth(110,410,fabsf(x))*(18+24*terrValueNoise(x*.005f,z*.006f));
    float r=trainingPondRadius(x,z);
    float basin=1-lobbySmooth(1.5f,5.5f,r);
    h=h*(1-basin)+(6.3f+2.2f*(r-1.0f)+.12f*(h-12.0f))*basin;
    float bowl=1-lobbySmooth(.45f,1.05f,r);
    h=h*(1-bowl)+3.8f*bowl;
    h+=(terrValueNoise(x*.08f,z*.08f)-.5f)*.40f;
    return h;
}
inline float trainingReeds(float x,float z) {
    float shore=1-lobbySmooth(1.05f,1.7f,trainingPondRadius(x,z));
    float farSide=fmaxf(lobbySmooth(176,190,z),lobbySmooth(24,36,fabsf(x)));
    return shore*farSide;
}
inline float trainingForest(float x,float z) {
    float patch=terrValueNoise(x*.015f+19,z*.013f-7);
    float far=lobbySmooth(290,520,z);
    float flank=lobbySmooth(90,230,fabsf(x));
    float density=fmaxf(far*.95f,flank*.85f)*lobbySmooth(.38f,.52f,patch);
    float pondRing=(1-lobbySmooth(1.5f,2.4f,trainingPondRadius(x,z)))*lobbySmooth(.95f,1.3f,trainingPondRadius(x,z));
    float clump=lobbySmooth(.50f,.64f,terrValueNoise(x*.045f+3,z*.045f+11));
    density=fmaxf(density,pondRing*.55f*clump*lobbySmooth(173,202,z));
    return density;
}
inline float trainingRangeHeight(float x, float z) {
        float lane = terrSmooth(terrClamp01((fabsf(z) - 14.0f) / 12.0f));
        float edge = terrSmooth(terrClamp01((sqrtf(x*x + z*z) - 18.0f) / 35.0f));
        float broad = (terrValueNoise(x * 0.035f + 8.0f, z * 0.035f - 3.0f) - 0.42f) * 5.0f;
        float detail = (terrValueNoise(x * 0.11f, z * 0.11f) - 0.5f) * 0.7f;
        float h = fmaxf(0.0f, (broad + detail) * fmaxf(lane, edge));
        // Ballistics hill (west, behind the firing line): ~16 m terraced rise —
        // Paldiski's terraced swells in miniature. The flat ledges are known
        // shooter heights for comparing per-weapon drop/holdover on downhill
        // shots at the target wall (~60-90 m).
        float dw = sqrtf((x + 40.0f) * (x + 40.0f) + z * z);
        h += terrTerrace(terrSmooth(terrClamp01(1.0f - dw / 34.0f)) * 16.0f, 5.0f, 0.6f);
        // Target knoll (north-east, past the wall's end): a smooth bare slope
        // facing the pad, so uphill impact points read directly as drop at range.
        float dk = sqrtf((x - 40.0f) * (x - 40.0f) + (z - 42.0f) * (z - 42.0f));
        h += terrSmooth(terrClamp01(1.0f - dk / 30.0f)) * 9.0f;
        // Low meadow hummocks outside the established range/prop pad.
        float meadow = lobbySmooth(14, 22, fabsf(z));
        float rolls = 0.45f + 1.15f*terrValueNoise(x*.085f+17, z*.085f-9);
        float rough = (terrValueNoise(x*.55f, z*.55f)-.5f)*.14f;
        h += meadow * (rolls + rough) * (1-.7f*lobbyWear(x,z));
        return h;
}

inline float trainingHeight(float x,float z) {
    float blend=lobbySmooth(58,140,sqrtf(x*x+z*z));
    if(blend==0) return trainingRangeHeight(x,z);
    if(blend==1) return trainingLandscapeHeight(x,z);
    return trainingRangeHeight(x,z)*(1-blend)+trainingLandscapeHeight(x,z)*blend;
}
