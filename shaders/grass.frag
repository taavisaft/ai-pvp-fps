#version 330 core
in vec3 worldPos;in vec3 vNormal;in float coverage;in float variation;
uniform vec3 eyePos;uniform vec3 sunDir;uniform vec3 skyZenith;uniform vec3 skyHorizon;uniform vec3 groundAmbient;uniform vec3 sunColor;uniform float fogDist;uniform float exposure;uniform float saturation;out vec4 fragColor;
float bayer4(vec2 p){ivec2 q=ivec2(mod(floor(p),4.0));int x=q.x,y=q.y;return(float((x&1)*8+(y&1)*4+(x&2)+(y&2)*2)+.5)/16.0;}
vec3 grade(vec3 c){c*=exposure;c=mix(vec3(dot(c,vec3(.299,.587,.114))),c,saturation);return clamp((c*(2.51*c+.03))/(c*(2.43*c+.59)+.14),0.0,1.0);}
void main(){if(coverage<=bayer4(gl_FragCoord.xy))discard;vec3 n=normalize(vNormal);if(!gl_FrontFacing)n=-n;vec3 base=mix(vec3(.055,.145,.035),vec3(.25,.38,.10),.25+variation*.52);vec3 ambient=mix(groundAmbient,skyZenith,n.y*.5+.5);vec3 lit=base*(ambient+sunColor*max(dot(n,normalize(sunDir)),0.0));float fog=clamp(length(worldPos-eyePos)/fogDist,0.0,1.0);fragColor=vec4(grade(mix(lit,skyHorizon,fog*fog)),1.0);}
