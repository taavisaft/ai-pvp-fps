"""python3 tools/uzi/textures.py — requires Pillow; no runtime dependency."""
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[2]
source=root/'_RESOURCES/uzi/textures'
out=root/'models/textures/weapons/uzi';out.mkdir(parents=True,exist_ok=True)
for name,size in [('BaseColor',2048),('Normal',1024)]:
    image=Image.open(source/f'uzi_kuszoid_{name}.png').convert('RGB')
    image.resize((size,size),Image.Resampling.LANCZOS).save(out/f'{name.lower()}.png')
rough=Image.open(source/'uzi_kuszoid_Roughness.png').convert('L').resize((1024,1024),Image.Resampling.LANCZOS)
metal=Image.open(source/'uzi_kuszoid_Metallic.png').convert('L').resize((1024,1024),Image.Resampling.LANCZOS)
Image.merge('RGB',(rough,metal,Image.new('L',(1024,1024),255))).save(out/'surface.png')
