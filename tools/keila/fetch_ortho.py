import os
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "_RESOURCES", "keila", "raw", "ortho")
CENTER_E, CENTER_N = 524102.52, 6574626.33
WMS = ("https://kaart.maaamet.ee/wms/fotokaart?service=WMS&version=1.1.1&request=GetMap"
       "&layers=EESTIFOTO&styles=&srs=EPSG:3301&format=image/jpeg&width=2048&height=2048&bbox=%f,%f,%f,%f")


def fetch(name, e0, n0, size):
    path = os.path.join(OUT, name)
    if os.path.exists(path):
        return
    with urllib.request.urlopen(WMS % (e0, n0, e0 + size, n0 + size), timeout=120) as r:
        data = r.read()
    with open(path, "wb") as f:
        f.write(data)
    print(name, len(data))


os.makedirs(OUT, exist_ok=True)
for row in range(2):
    for col in range(2):
        fetch("near_%d_%d.jpg" % (col, row), CENTER_E - 512 + col * 512, CENTER_N - 512 + row * 512, 512)
fetch("far.jpg", CENTER_E - 1024, CENTER_N - 1024, 2048)
