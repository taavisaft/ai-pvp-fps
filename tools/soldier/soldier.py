"""Original field soldier: segmented for the existing gameplay pose, Y-up unit parts."""
import math
import mesh_tools as t
from mesh_tools import box,ellipsoid,tube,path,cloth

def build():
    t.material('Olive ripstop',(.17,.195,.12),.025,True)
    t.material('Ranger green nylon',(.135,.16,.105),.025)
    t.material('Webbing',(.22,.23,.155),.03)
    t.material('Seams',(.075,.086,.056),.015)
    t.material('Rubber',(.025,.03,.026),.05)
    t.material('Leather',(.063,.055,.04),.08)
    t.material('Skin',(.43,.285,.19),.05)
    t.material('Lens',(.075,.10,.09),.55)
    t.material('Hardware',(.08,.09,.08),.24)
    t.material('Coyote',(.27,.235,.16),.025)
    t.material('Patch',(.33,.36,.23),.025)
    t.GROUP='head'
    ellipsoid('Balaclava skull',(0,-.03,-.02),(.76,.9,.78),'Seams')
    ellipsoid('Jaw',(0,-.27,.085),(.60,.40,.63),'Seams',8,16)
    ellipsoid('Exposed eye bridge',(0,.035,.345),(.59,.24,.13),'Skin',8,16)
    ellipsoid('Mask nose',(0,-.08,.39),(.16,.22,.12),'Ranger green nylon',8,12)
    for x in [-.25,-.12,0,.12,.25]:
        tube('Mask contour seam',(x,-.28,.335),(x*.7,-.40,.24),.008,'Webbing',6)
    # Helmet shell is a truncated ellipsoid with an open lower rim.
    vs=[];fs=[];n=32;rings=9
    for j in range(rings+1):
        a=.045+1.62*j/rings
        for i in range(n):
            q=2*math.pi*i/n
            # Raised sides leave room for the communication headset.
            y=.14+.365*math.cos(a)+.08*abs(math.cos(q))*(j/rings)**5
            vs.append((.495*math.sin(a)*math.cos(q),y,-.035+.465*math.sin(a)*math.sin(q)))
    for j in range(rings):
        for i in range(n):a=j*n+i;b=j*n+(i+1)%n;fs.append((a,b,b+n,a+n))
    shell=t.mesh('Ballistic helmet',vs,fs,'Olive ripstop',True)
    # Return strip around exposed rim so the silhouette has thickness.
    for i in range(n):tube('Helmet rim',vs[rings*n+i],vs[rings*n+(i+1)%n],.012,'Rubber',6)
    box('NVG bracket',(0,.27,.414),(.18,.17,.046),'Hardware',.014)
    box('NVG recess',(0,.27,.441),(.08,.075,.012),'Rubber',.004)
    for x in [-.41,.41]:
        box('Helmet rail',(x,.15,.08),(.07,.09,.42),'Hardware',.015)
        ellipsoid('Comms ear cup',(x,-.055,-.02),(.16,.29,.26),'Ranger green nylon',8,14)
        box('Ear pad',(x*1.06,-.06,-.02),(.045,.22,.18),'Rubber',.02)
        path('Chin strap',[(x,.10,.23),(x*.82,-.29,.20),(x*.30,-.43,.27)],.018,'Webbing',6)
        for z in [-.03,.08,.19]:box('Rail notch',(x*1.06,.16,z),(.015,.045,.022),'Webbing',.002)
    # Two close-fitting protective lenses and flexible frame.
    for x in [-.175,.175]:
        box('Goggle frame',(x,.06,.385),(.33,.205,.11),'Rubber',.065)
        box('Goggle lens',(x,.065,.448),(.273,.139,.035),'Lens',.045)
    box('Goggle bridge',(0,.085,.455),(.055,.055,.035),'Rubber',.012)
    path('Boom mic',[(-.46,-.13,.04),(-.40,-.26,.29),(-.20,-.26,.45)],.011,'Hardware',8)
    ellipsoid('Mic foam',(-.18,-.26,.45),(.13,.08,.09),'Rubber',6,10)
    t.GROUP='torso'
    cloth('Field jacket',[(-.50,.43,.37,0),(-.39,.46,.39,0),(-.20,.46,.40,0),
          (0,.49,.40,0),(.20,.54,.40,0),(.36,.59,.37,0),(.44,.52,.33,0),(.50,.29,.27,0)],'Olive ripstop',28)
    for x in [-.62,.62]:
        ellipsoid('Shoulder yoke',(x,.36,0),(.40,.32,.70),'Olive ripstop',8,16)
    cloth('Standing jacket collar',[(.44,.28,.28,0),(.55,.24,.26,0),(.63,.22,.25,0)],'Ranger green nylon',20)
    box('Plate carrier front',(0,.045,.385),(.89,.78,.23),'Ranger green nylon',.10)
    box('Rear plate bag',(0,.06,-.40),(.82,.78,.20),'Ranger green nylon',.08)
    for x in [-.36,.36]:
        box('Shoulder strap',(x,.41,.05),(.13,.11,.85),'Webbing',.025)
        box('Strap buckle',(x,.315,.515),(.15,.07,.037),'Hardware',.01)
    for y in [-.25,-.13,-.01,.11,.23]:
        box('MOLLE webbing',(0,y,.51),(.81,.034,.02),'Webbing',.004)
        for x in [-.33,-.165,0,.165,.33]:
            box('MOLLE bartack',(x,y,.525),(.008,.04,.008),'Seams',.001)
    for x in [-.29,0,.29]:
        box('Magazine pouch',(x,-.19,.58),(.255,.43,.19),'Ranger green nylon',.03)
        box('Pouch flap',(x,-.01,.69),(.245,.13,.035),'Webbing',.018)
        tube('Retention pull',(x,-.11,.694),(x,-.18,.694),.013,'Rubber',8)
    box('Chest ID panel',(.10,.29,.524),(.32,.13,.018),'Seams',.008)
    for x in [-.015,.035,.085,.135,.185]:box('ID stitching',(x,.29,.537),(.012,.052,.003),'Patch',.001)
    box('Radio',(-.53,.13,.26),(.18,.32,.22),'Hardware',.025)
    tube('Radio aerial',(-.53,.28,.25),(-.53,.57,.25),.012,'Rubber',8)
    box('Utility pack',(0,-.04,-.56),(.62,.62,.22),'Coyote',.065)
    for x in [-.22,.22]:box('Pack compression strap',(x,-.04,-.69),(.05,.58,.02),'Webbing',.005)
    path('Hydration hose',[(.28,.32,-.47),(.42,.48,-.17),(.38,.37,.36),(.42,.05,.54)],.017,'Rubber',8)
    t.GROUP='pelvis'
    cloth('Trouser seat',[(-.50,.44,.41,0),(-.3,.48,.43,0),(.1,.49,.43,0),(.42,.45,.40,0)],'Olive ripstop',24)
    box('Duty belt',(0,.29,0),(.99,.22,.89),'Seams',.06)
    box('Belt buckle',(0,.29,.466),(.21,.20,.065),'Hardware',.02)
    box('Buckle inset',(0,.29,.503),(.13,.12,.012),'Rubber',.01)
    for x in [-.36,.36]:box('Belt loop',(x,.27,.47),(.07,.27,.025),'Webbing',.009)
    t.GROUP='neck'
    cloth('Neck gaiter',[(-.5,.46,.44,0),(-.2,.44,.43,0),(.15,.40,.40,0),(.5,.38,.37,0)],'Seams',20)
    t.GROUP='arm'
    cloth('Combat sleeve',[(y,.48-.14*(y+.5)+.035*math.sin(y*24),.44-.10*(y+.5),0)
          for y in [-.51,-.43,-.34,-.23,-.13,0,.11,.22,.33,.43,.51]],'Olive ripstop',20)
    ellipsoid('Rounded shoulder',(0,-.45,0),(.95,.27,.84),'Olive ripstop',8,16)
    box('Sleeve reinforcement',(0,-.05,-.38),(.58,.36,.12),'Ranger green nylon',.06)
    cloth('Wrist or joint cuff',[(.40,.36,.35,0),(.46,.38,.37,0),(.51,.36,.35,0)],'Webbing',20)
    t.GROUP='hand'
    ellipsoid('Glove palm',(0,-.04,0),(.82,.83,.64),'Coyote',8,16)
    box('Knuckle guard',(0,.03,-.27),(.68,.30,.12),'Rubber',.065)
    for x in [-.28,-.095,.095,.28]:
        ellipsoid('Curled finger',(x,.25,.15),(.19,.46,.52),'Coyote',8,10)
        box('Finger armour',(x,.32,-.06),(.13,.13,.07),'Leather',.025)
    ellipsoid('Thumb',(.37,-.05,.22),(.23,.56,.32),'Coyote',8,12)
    t.GROUP='leg'
    cloth('Combat trouser',[(y,.46-.12*(y+.5)+.026*math.sin(y*28),.44-.11*(y+.5),.015*math.sin(y*9))
          for y in [-.51,-.43,-.34,-.25,-.14,-.02,.10,.21,.32,.41,.51]],'Olive ripstop',24)
    # Side cargo pocket; remains unobtrusive when reused on the shin.
    box('Cargo pocket',(.34,-.08,0),(.17,.35,.57),'Ranger green nylon',.045)
    box('Cargo flap',(.431,-.22,0),(.03,.10,.53),'Webbing',.01)
    t.GROUP='forearm'
    cloth('Forearm sleeve',[(y,.44-.12*(y+.5)+.025*math.sin(y*27),.41-.09*(y+.5),0)
          for y in [-.53,-.43,-.32,-.21,-.1,.01,.12,.23,.34,.43,.50]],'Olive ripstop',20)
    cloth('Wrist cuff',[(.37,.34,.34,0),(.43,.36,.35,0),(.52,.34,.32,0)],'Webbing',20)
    t.GROUP='shin'
    cloth('Lower trouser',[(y,.44-.15*(y+.5)+.025*math.sin(y*31),.40-.09*(y+.5),0)
          for y in [-.53,-.43,-.32,-.21,-.1,.01,.12,.23,.34,.43,.52]],'Olive ripstop',24)
    # Small knee reinforcement follows the same local facing convention as the thigh.
    ellipsoid('Knee reinforcement',(0,-.34,-.32),(.69,.37,.27),'Ranger green nylon',8,16)
    t.GROUP='foot'
    # +Y along toes; local +Z is sole/down in poseSegment's foot basis.
    ellipsoid('Boot upper',(0,-.24,-.12),(.90,.75,1.15),'Leather',12,20)
    ellipsoid('Rounded leather toe',(0,.30,-.04),(.98,1.04,.73),'Leather',10,24)
    box('Rubber sole',(0,.04,.34),(1.0,1.52,.19),'Rubber',.08)
    for y in [-.60,-.40,-.20,0,.20,.40,.60]:
        for x in [-.44,.44]:box('Tread',(x,y,.38),(.16,.09,.13),'Rubber',.012)
    for y in [-.34,-.22,-.10,.02,.14]:
        path('Boot lace',[(-.23,y,-.43),(.23,y+.075,-.43)],.012,'Webbing',6)
