"""Original game-resolution classic Uzi exterior. Cosmetic, no internal mechanism.
Game meters, +Z barrel. Muzzle (0,.02,.36), firing grip (0,-.09,-.02).
"""
import mesh_tools as t
from mesh_tools import box,tube,path,ring

def build():
    t.GROUP='uzi'
    t.material('Parkerized steel',(.075,.084,.081),.27)
    t.material('Edge steel',(.13,.145,.14),.32)
    t.material('Black polymer',(.027,.033,.03),.085)
    t.material('Recess',(.009,.012,.011),.02)
    t.material('Worn steel',(.24,.25,.235),.40)
    steel='Parkerized steel';edge='Edge steel';poly='Black polymer';dark='Recess'
    box('Stamped receiver',(0,.006,.055),(.061,.078,.31),steel,.004)
    box('Receiver top cover',(0,.049,.052),(.062,.012,.295),edge,.003)
    for x in [-.025,.025]:
        box('Cover seam',(x,.052,.05),(.002,.002,.266),dark,.0005)
        box('Lower receiver seam',(x*1.24,-.02,.04),(.002,.007,.26),edge,.001)
    box('Rear plate',(0,.009,-.105),(.065,.08,.016),steel,.004)
    # Shallow ejection recess, with raised lip and darker recessed interior.
    box('Ejection port',(.0311,.02,.086),(.0018,.027,.077),dark,.002)
    box('Port sill',(.033,.004,.086),(.003,.004,.078),edge,.001)
    box('Bolt glint',(.032,.029,.076),(.001,.004,.058),edge,.001)
    # Pressed strengthening ribs run on both receiver sides.
    for x in [-.031,.031]:
        for z in [-.065,-.012,.156,.184]:
            box('Receiver pressing',(x,.005,z),(.005,.044,.006),edge,.002)
        for z in [-.084,.124]:
            tube('Receiver rivet',(x,.001,z),(x*1.10,.001,z),.0032,edge,12)
    # Barrel collar and visually open muzzle.
    tube('Barrel nut',(0,.02,.207),(0,.02,.229),.021,steel,32)
    for z in [.209,.214,.219,.224]:ring('Nut rib',(0,.02,z),.022,.020,.0018,edge,28)
    tube('Exposed barrel',(0,.02,.226),(0,.02,.352),.0095,steel,24)
    ring('Muzzle crown',(0,.02,.355),.010,.0046,.01,edge,32)
    tube('Bore shadow',(0,.02,.343),(0,.02,.349),.0046,dark,16)
    # Fore-end is split into two textured grip panels, beneath the receiver.
    box('Fore-end',(0,-.047,.151),(.067,.042,.119),poly,.008)
    for x in [-.034,.034]:
        for z in [.104,.116,.128,.140,.152,.164,.176,.188]:
            box('Fore-end grip rib',(x,-.043,z),(.004,.029,.0038),edge,.001)
        for z in [.107,.184]:
            tube('Fore-end screw',(x,-.05,z),(x*1.10,-.05,z),.003,steel,12)
    # Pistol grip enclosing the magazine, safety at its rear.
    box('Grip housing',(0,-.096,-.018),(.048,.132,.064),steel,.006)
    for x in [-.025,.025]:
        box('Grip panel',(x,-.097,-.018),(.009,.102,.057),poly,.006)
        for y in [-.060,-.068,-.076,-.084,-.092,-.10,-.108,-.116,-.124,-.132]:
            box('Grip rib',(x*1.17,y,-.018),(.002,.0028,.045),edge,.0008)
        tube('Grip screw',(x*1.15,-.071,-.018),(x*1.22,-.071,-.018),.0033,steel,12)
    box('Grip safety',(0,-.092,-.053),(.023,.084,.009),poly,.003)
    # U-shaped open trigger guard; no filled slab between trigger and fingers.
    path('Trigger guard',[(0,-.033,.083),(0,-.084,.078),(0,-.101,.063),(0,-.104,.015)],.004,steel,10)
    path('Trigger',[(0,-.035,.035),(0,-.063,.044),(0,-.078,.037)],.003,edge,10)
    box('Magazine',(0,-.209,-.019),(.031,.132,.043),steel,.002)
    for x in [-.0158,.0158]:
        box('Magazine flute',(x,-.211,-.019),(.001,.101,.009),dark,.001)
    box('Magazine floorplate',(0,-.277,-.019),(.037,.007,.049),edge,.0018)
    box('Magazine release',(-.030,-.137,-.001),(.01,.025,.013),steel,.002)
    # Top charging handle leaves the center sight line clear above it.
    box('Charging slide',(0,.057,.075),(.016,.006,.044),steel,.002)
    box('Charging handle',(0,.065,.077),(.039,.011,.022),poly,.003)
    for x in [-.014,-.007,0,.007,.014]:box('Handle ridge',(x,.071,.077),(.002,.002,.018),edge,.0004)
    # Guard ears around front post and an open rear aperture.
    for z in [-.077,.188]:
        box('Sight base',(0,.061,z),(.041,.012,.029),steel,.003)
        for x in [-.019,.019]:
            box('Sight protective ear',(x,.079,z),(.006,.037,.023),steel,.003)
    ring('Rear aperture',(0,.080,-.077),.010,.0065,.0035,edge,24)
    tube('Front post',(0,.067,.188),(0,.080,.188),.0017,edge,10)
    # Folded under-stock: hinges and paired stamped arms tucked under receiver.
    for x in [-.032,.032]:
        tube('Stock hinge',(x,-.013,-.106),(x*1.24,-.013,-.106),.011,edge,20)
        box('Folded stock arm',(x,-.047,-.045),(.009,.016,.14),steel,.002)
        box('Folded stock return',(x,-.065,-.024),(.008,.014,.10),steel,.002)
    box('Folded buttplate',(0,-.071,.025),(.081,.009,.025),edge,.003)
    box('Selector',(-.033,-.038,-.018),(.008,.011,.032),edge,.002)
    for z in [-.041,-.03,-.019]:box('Selector mark',(-.0318,-.02,z),(.001,.004,.002), 'Worn steel',.0002)
    # A few restrained wear lines; avoid uniform silver outlines everywhere.
    for x in [-.03,.03]:box('Cover edge wear',(x,.055,-.033),(.001,.001,.066),'Worn steel',.0003)
