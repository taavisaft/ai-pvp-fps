// Lobby experiment: identical illumination for mesh and baked leafy crowns.
// The albedo bake already retains canopy access; apply it once in both paths.
vec3 broadleafLight(float cloud) {
    return sunColor * (0.35 * cloud)
         + mix(groundAmbient, skyZenith, 0.72) * 0.8;
}
