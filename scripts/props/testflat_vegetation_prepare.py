"""T208: local PCG tables using the actual Raraka distribution and meshes."""
import csv
import json
import math
import random
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'monke_island/Saved/T208';OUT.mkdir(parents=True,exist_ok=True)
DEST='/Game/MonkeIsland/Environment/FortFrench/Revisions/T208/Data'
SITES=[(-44000,-10500,2600),(-16000,10500,2600)]
PLANT_PREFIXES=('SM_CoconutPalm_','SM_Naupaka_','SM_Pandanus_','SM_MikiMiki_','SM_Tahinu_','SM_Fern_','SM_Grass_','SM_Ipomoea_')
RNG=random.Random(208)


def category(name):
    if name.startswith('SM_CoconutPalm_'):
        return 'young_palm' if any(s in name for s in ('Young','Juvenile','Sprout')) else 'palm'
    if 'Pandanus' in name:return 'pandanus'
    if name.startswith(('SM_Fern','SM_Grass','SM_Ipomoea')):return 'ground'
    return 'shrub'


def main():
    report={'task':'T-208','sites':SITES,'tables':{},'replace_fort_labels':[],'fort_rows':[]}
    pool=[]
    def write(name,rows,physics,zone):
        path=OUT/(name+'.csv')
        with path.open('w',newline='',encoding='utf-8') as f:
            writer=csv.DictWriter(f,fieldnames=['---','Tag','DevComment']);writer.writeheader();writer.writerows(rows)
        report['tables'][name]={'file':str(path),'asset':DEST+'/'+name,'rows':len(rows),'physics':physics,'zone':zone}
    for physics in ('Hard','Soft'):
        source=ROOT/f'monke_island/Saved/T183/zone/DT_FRG_TestFlat_{physics}_00.csv'
        with source.open(newline='',encoding='utf-8') as f:rows=list(csv.DictReader(f))
        kept=[];removed=[]
        for row in rows:
            pos,rot,scale=row['DevComment'].split('|');x,y,z=map(float,pos.split(','))
            name=row['Tag'].split('.')[-1]
            if name.startswith(PLANT_PREFIXES) and not any(s in name for s in ('Fallen','DeadSnag')):
                pool.append({'row':row,'name':name,'category':category(name),'depth':z,'physics':physics})
            if name.startswith(PLANT_PREFIXES) and any(math.hypot(x-cx,y-cy)<radius for cx,cy,radius in SITES):removed.append(row)
            else:kept.append(row)
        name=f'DT_TestFlat_ClearSites_{physics}'
        write(name,kept,physics,'testflat');report['tables'][name]['removed']=len(removed)
        (OUT/(name+'-removed.json')).write_text(json.dumps(removed,indent=2))
    rows_by_physics={'Hard':[],'Soft':[]}
    layout=json.loads((ROOT/'art/export/Fort_T193/layout.json').read_text())
    for i,row in enumerate(layout):
        if row['kind']!='vegetation' or not row['mesh'].startswith(PLANT_PREFIXES):continue
        candidates=[p for p in pool if p['category']==category(row['mesh'])]
        picked=RNG.choice(candidates);source=picked['row']
        x,y,z=row['location_cm'];x-=66000;z+=115+picked['depth']
        yaw=math.degrees(2*math.atan2(row['quaternion_xyzw'][2],row['quaternion_xyzw'][3]))
        scale=source['DevComment'].split('|')[2]
        dest={'---':f'Fort_{i:04d}','Tag':source['Tag'],'DevComment':f'{x:.2f},{y:.2f},{z:.2f}|0,{yaw:.2f},0|{scale}'}
        rows_by_physics[picked['physics']].append(dest)
        label=f'Fort_T193_{i:03d}_{row["mesh"].removeprefix("SM_")}'
        report['replace_fort_labels'].append(label)
        report['fort_rows'].append({'old':row['mesh'],'new':picked['name'],'location':[x,y,z],'scale':scale})
    # The courtyard tubs receive real Raraka naupaka rather than the simple
    # foliage formerly modeled into the furniture mesh.
    leaf=next(p for p in pool if p['name']=='SM_Naupaka_A')['row']['Tag']
    for i,(x,y) in enumerate([(-6.3,6.4),(6.3,6.4),(11,3.7),(-10,-8.5)]):
        rows_by_physics['Soft'].append({'---':f'Planter_{i}','Tag':leaf,'DevComment':f'{-66000+x*100},{-y*100},149|0,{i*87},0|0.23,0.23,0.23'})
    for physics,rows in rows_by_physics.items():write('DT_Fort_Raraka_'+physics,rows,physics,'fort')
    (OUT/'prepare-result.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print({name:dict(rows=info['rows'],removed=info.get('removed',0)) for name,info in report['tables'].items()})
    print('fort plants replaced',len(report['replace_fort_labels']))


if __name__=='__main__':main()
