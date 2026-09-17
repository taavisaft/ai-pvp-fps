#include "renderer.h"
#include "map.h"
#include "stand_mesh.h"
#include "player_mesh.h"
#include "uzi_mesh.h"
#include "player_lod_mesh.h"
#include "uzi_lod_mesh.h"
#include "texture.h"
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>

bool Renderer::init(const char* title, int w, int h, int msaaSamples) {
    width = w;
    height = h;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);   // planar mirror reflection mask

    msaa = msaaSamples;
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, msaa > 0 ? 1 : 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, msaa);
    window = SDL_CreateWindow(title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
        SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window && msaa > 0) {
        msaa = 0;
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
        window = SDL_CreateWindow(title,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
            SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
    }
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return false;
    }

    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
        return false;
    }
    SDL_GL_SetSwapInterval(0);  // VSync off: uncapped frame rate

    if (!loadGLFunctions()) return false;

    refreshWindowSize();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    if (msaa > 0) glEnable(GL_MULTISAMPLE);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    char base[512];
    const char* sdlBase = SDL_GetBasePath();
    char vpath[600], fpath[600];
    snprintf(base, sizeof(base), "%s", sdlBase ? sdlBase : "");
    snprintf(vpath, sizeof(vpath), "%sshaders/basic.vert", base);
    snprintf(fpath, sizeof(fpath), "%sshaders/basic.frag", base);
    if (!shader.load(vpath, fpath)) {
        // fallback: run from source tree, shaders/ in cwd
        if (!shader.load("shaders/basic.vert", "shaders/basic.frag")) return false;
    }

    char skyv[600], skyf[600];
    snprintf(skyv, sizeof(skyv), "%sshaders/sky.vert", base);
    snprintf(skyf, sizeof(skyf), "%sshaders/sky.frag", base);
    if (!skyShader.load(skyv, skyf)) {
        if (!skyShader.load("shaders/sky.vert", "shaders/sky.frag")) return false;
    }

    char dpv[600], dpf[600];
    snprintf(dpv, sizeof(dpv), "%sshaders/depth.vert", base);
    snprintf(dpf, sizeof(dpf), "%sshaders/depth.frag", base);
    if (!depthShader.load(dpv, dpf)) {
        if (!depthShader.load("shaders/depth.vert", "shaders/depth.frag")) return false;
    }

    char txv[600], txf[600];
    snprintf(txv, sizeof(txv), "%sshaders/texquad.vert", base);
    snprintf(txf, sizeof(txf), "%sshaders/texquad.frag", base);
    if (!texShader.load(txv, txf)) {
        if (!texShader.load("shaders/texquad.vert", "shaders/texquad.frag")) return false;
    }
    texUvCenterLoc = glGetUniformLocation(texShader.program, "uvCenter");
    texUvHalfLoc   = glGetUniformLocation(texShader.program, "uvHalf");

    if (!createUnitCube(cube)) return false;
    if (!createGroundQuad(ground)) return false;
    if (!stand.create(STAND_VERTS, sizeof(STAND_VERTS) / sizeof(float),
                      STAND_IDX, STAND_IDX_COUNT)) return false;
    for (int i = 0; i < PART_COUNT; i++) {
        const PMeshPart& p = PMESH_PARTS[i];
        if (!playerPart[i].create(p.verts, p.floatCount, p.idx, p.idxCount, true, false, true)) return false;
        const PMeshPart& low = PMESH_LOD_PARTS[i];
        if (!playerLod[i].create(low.verts, low.floatCount, low.idx, low.idxCount,
                                true, false, true)) return false;
    }
    if (!uzi.create(WMESH_UZI_VERTS, sizeof(WMESH_UZI_VERTS) / sizeof(float),
                    WMESH_UZI_IDX, sizeof(WMESH_UZI_IDX) / sizeof(unsigned),
                    true, false, true)) return false;
    if (!uziLod.create(WLOD_UZI_VERTS, sizeof(WLOD_UZI_VERTS) / sizeof(float),
                       WLOD_UZI_IDX, sizeof(WLOD_UZI_IDX) / sizeof(unsigned),
                       true, false, true)) return false;
    // Heightfield mesh: setMap must have run first (pads + terrain mode set) so the
    // mesh matches the ground the server simulates.
    // The 2 km taiga ground is chunk-built lazily (taigaTerrain); this single mesh
    // only serves the small lobby heightfield, rebuilt on map switch below.
    // Training terrain is prepared on map entry.
    if (!createQuad2D(quad2d)) return false;
    if (!initLandscape(base)) return false;
    if (!font.init()) return false;
    if (!materials.init()) return false;

    // Shadow map: a depth-only texture rendered from the sun each frame.
    // Created before veg.init so unit 1 is valid and loaded during impostor baking.
    glGenFramebuffers(1, &shadowFBO);
    glGenTextures(1, &shadowTex);
    glBindTexture(GL_TEXTURE_2D, shadowTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadowSize, shadowSize, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    // Clamp to a white border so anything sampled outside the map reads "lit" (depth 1).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex, 0);
    glDrawBuffer(GL_NONE);   // no color attachment
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "shadow FBO incomplete\n");
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (!veg.init(base, shadowTex)) return false;
    glViewport(0, 0, fbW, fbH);   // the impostor bake in veg.init resized it
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, materials.mats[MAT_GROUND].tex);

    active = &shader;
    return true;
}

void Renderer::setShadowMapSize(int size) {
    if (size < 512) size = 512;
    if (size > 4096) size = 4096;
    if (size == shadowSize) return;
    shadowSize = size;
    if (shadowTex) {
        glBindTexture(GL_TEXTURE_2D, shadowTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadowSize, shadowSize, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        printf("[quality] shadow map resized to %dx%d\n", shadowSize, shadowSize);
    }
}

void Renderer::endFrame() {
    SDL_GL_SwapWindow(window);
}

void Renderer::toggleWireframe() {
    wireframe = !wireframe;
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
}

void Renderer::shutdown() {
    destroyLandscape();
    veg.destroy();
    materials.destroy();
    font.destroy();
    quad2d.destroy();
    terrain.destroy();
    taigaTerrain.destroy();
    ground.destroy();
    stand.destroy();
    for (int i = 0; i < PART_COUNT; i++) playerPart[i].destroy();
    uzi.destroy();
    uziLod.destroy();
    for (int i = 0; i < PART_COUNT; i++) playerLod[i].destroy();
    cube.destroy();
    shader.destroy();
    skyShader.destroy();
    depthShader.destroy();
    texShader.destroy();
    for (int i = 0; i < MAP_COUNT; i++) if (mapTex[i]) glDeleteTextures(1, &mapTex[i]);
    if (shadowTex) glDeleteTextures(1, &shadowTex);
    if (shadowFBO) glDeleteFramebuffers(1, &shadowFBO);
    if (glContext) SDL_GL_DeleteContext(glContext);
    if (window) SDL_DestroyWindow(window);
    glContext = nullptr;
    window = nullptr;
}
