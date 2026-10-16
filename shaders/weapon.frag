#version 330 core
in vec3 worldPos;
in vec3 vNormal;
in vec2 vUV;
in vec4 lightSpacePos;
out vec4 fragColor;
uniform sampler2D diffuseMap;
uniform sampler2D rockMap; // normal texture, unit 2
uniform sampler2D dirtMap; // roughness/metallic, unit 3
uniform sampler2D shadowMap;
uniform int useTexture, useShadow;
uniform vec3 eyePos, sunDir, sunColor, skyZenith, skyHorizon, groundAmbient;
uniform float exposure, saturation, fogDist, fogHeightAmt;

void main() {
    vec3 N=normalize(vNormal);
    vec3 albedo=vec3(.09,.10,.11);
    float rough=.38,metal=.8;
    if(useTexture==1) {
        albedo=pow(texture(diffuseMap,vUV).rgb,vec3(2.2));
        vec2 surface=texture(dirtMap,vUV).rg;
        rough=clamp(surface.r,.16,.96);metal=surface.g;
        vec3 q1=dFdx(worldPos),q2=dFdy(worldPos);
        vec2 uv1=dFdx(vUV),uv2=dFdy(vUV);
        vec3 p1=cross(q2,N),p2=cross(N,q1);
        vec3 T=p1*uv1.x+p2*uv2.x,B=p1*uv1.y+p2*uv2.y;
        float norm=inversesqrt(max(max(dot(T,T),dot(B,B)),1e-12));
        N=normalize(mat3(T*norm,B*norm,N)*(texture(rockMap,vUV).xyz*2.0-1.0));
    }
    vec3 L=normalize(sunDir),V=normalize(eyePos-worldPos),H=normalize(L+V);
    float nl=max(dot(N,L),0.0),nv=max(dot(N,V),.001);
    float nh=max(dot(N,H),0.0),vh=max(dot(V,H),0.0);
    float a=rough*rough,a2=a*a,d=nh*nh*(a2-1.0)+1.0;
    float D=a2/max(3.141593*d*d,1e-5);
    float k=(rough+1.0)*(rough+1.0)/8.0;
    float G=(nv/(nv*(1.0-k)+k))*(nl/(nl*(1.0-k)+k));
    vec3 F0=mix(vec3(.04),albedo,metal);
    vec3 F=F0+(1.0-F0)*pow(1.0-vh,5.0);
    float visibility=1.0;
    if(useShadow==1) {
        vec3 p=lightSpacePos.xyz/lightSpacePos.w*.5+.5;
        if(all(greaterThan(p,vec3(0))) && all(lessThan(p,vec3(1))))
            visibility=p.z-.0015>texture(shadowMap,p.xy).r ? .30 : 1.0;
    }
    vec3 ambient=mix(groundAmbient,skyZenith,N.y*.5+.5);
    vec3 reflected=mix(groundAmbient,skyHorizon,reflect(-V,N).y*.5+.5);
    vec3 c=albedo*(1.0-metal*.75)*ambient*.8 + reflected*F0*(.28+.3*(1.0-rough));
    c+=sunColor*nl*visibility*(albedo*(1.0-metal)/3.141593+F*D*G/max(4.0*nv*nl,.001));
    c*=exposure;
    c=mix(vec3(dot(c,vec3(.299,.587,.114))),c,saturation);
    c=clamp((c*(2.51*c+.03))/(c*(2.43*c+.59)+.14),0.0,1.0);
    c=pow(c,vec3(1.0/2.2));
    float distance=length(eyePos-worldPos)*(1.0+fogHeightAmt*exp(-max(worldPos.y,0.0)/12.0));
    float fog=clamp(distance/fogDist,0.0,1.0);
    fragColor=vec4(mix(c,skyHorizon,fog*fog),1);
}
