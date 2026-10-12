"""T198: PBR art direction for every fort surface and furnished interior."""
import json
from pathlib import Path
import unreal as ue
from fort_showcase_import import (DEST,SOURCES,Graph,load,save,TOOLS,MEL,MP,F1,F2,F3,
                                 layer,sample,linear,SIMPLE)

OUT=Path(__file__).resolve().parents[2]/'monke_island/Saved/T198'
EXTRA=['Linen','BlueCloth','RedCloth','Leather','Parchment','Ink','Ceramic','GoldStamp','MoatWater','Leaf']


def make(slot):
    name='M_'+slot.removeprefix('MAT-').removeprefix('MAT_')
    path=DEST+'/Materials/'+name
    mat=load(path) or TOOLS.create_asset(name,DEST+'/Materials',ue.Material,ue.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    g=Graph(mat);pos=g.node(ue.MaterialExpressionWorldPosition);v=g.node(ue.MaterialExpressionVertexColor)
    kind=slot.removeprefix('MAT-Fort_').removeprefix('MAT-Rework_')
    base=g.color((.3,.3,.3));rough=g.scalar(.7);normal=None;strength=.55;metal=g.scalar(0)
    def mix(a,b,f):return g.custom('return lerp(A,B,saturate(F));',F3,A=a,B=b,F=f)
    if kind in SIMPLE:
        recipe=list(SIMPLE[kind]);recipe[1]*=1.5
        if kind=='WoodOak':recipe[2]=(.92,.78,.58);recipe[3]=.8
        if kind=='WoodTamanu':recipe[2]=(.94,.76,.57);recipe[3]=.6
        if kind=='WoodWeathered':recipe[2]=(.77,.74,.64);recipe[3]=.25
        if kind=='RoofTiles':recipe[1]=2.1;recipe[2]=(1.03,.63,.41);recipe[3]=.9
        base,rough,normal=layer(g,slot,*recipe,rot=kind=='Shingles')
    if kind in ('CoralRubble','CoralAshlar'):
        ashlar=kind=='CoralAshlar'
        base,rough,normal=layer(g,slot,'coral_fort_wall_01' if ashlar else 'coral_fort_wall_02',1.05,(1.02,.92,.77),.55)
        if not ashlar:
            plaster,pr,pn=layer(g,slot,'white_plaster_02',1.4,(.94,.86,.72),.35)
            # Texture-driven wear avoids the previous regular sine/checker patch pattern.
            mask=g.custom('float lum=dot(C,float3(.2126,.7152,.0722));return saturate(V.g*1.3)*smoothstep(.12,.38,lum);',F1,C=base,V=v)
            base=mix(base,plaster,mask);rough=mix(rough,pr,mask);normal=mix(normal,pn,mask)
        base=g.custom('return lerp(C,float3(.66,.59,.46),.14);',F3,C=base);strength=.36
    if kind in ('CoralRubble','CoralAshlar','Brick','RoofTiles','WoodWeathered','CoralGravel'):
        base=g.custom('float m=.93+.045*sin(P.x*.0013+P.y*.0007)*sin(P.y*.0021)+.025*sin(P.z*.008+P.x*.003);'
                      'float damp=saturate((90-P.z)/65)*.20;return C*m*lerp(float3(1,1,1),float3(.44,.53,.40),damp);',F3,C=base,P=pos)
    if kind in ('Iron','IronRust','Copper'):
        metal=g.scalar({'Iron':.9,'IronRust':.65,'Copper':.9}[kind])
        if kind=='IronRust':base=g.custom('return lerp(C,float3(.038,.029,.020),.68);',F3,C=base)
        if kind=='Copper':base=g.custom('return lerp(C,float3(.33,.16,.055),.65);',F3,C=base)
        rough=g.custom('return clamp(R.r,.25,.58);',F1,R=rough);strength=.24
    if kind in ('Linen','BlueCloth','RedCloth','Parchment','Leather'):
        base,rough,normal=layer(g,'MAT-Fort_Flag','Fabric019',.32,(1,1,1),0)
        tint={'Linen':'D5C8A6','BlueCloth':'214057','RedCloth':'762F22','Parchment':'C4AE75','Leather':'513221'}[kind]
        base=g.custom('return Tint*(.72+.45*dot(C,float3(.2126,.7152,.0722)));',F3,Tint=g.color(linear(tint)),C=base)
        rough=g.scalar(.68 if kind=='Leather' else .89);strength=.23 if kind in ('Leather','Parchment') else .48
    if kind=='Ink':base=g.color((.015,.008,.004));rough=g.scalar(.82)
    if kind=='Ceramic':
        base=g.custom('float a=.5+.5*sin(P.z*.45);return lerp(float3(.065,.19,.16),float3(.31,.35,.25),smoothstep(.88,.96,a));',F3,P=pos)
        rough=g.scalar(.24)
    if kind=='Leaf':
        base=g.color((.10,.21,.06));rough=g.scalar(.8);mat.set_editor_property('two_sided',True)
    if kind=='GoldStamp' or slot.startswith('MAT_Gold'):
        if slot in SOURCES:base,rough,normal=layer(g,slot,'loot_1k')
        else:base,rough,normal=layer(g,'MAT_Gold_Aged','loot_1k')
        color=(.73,.43,.105) if kind=='GoldStamp' else (.83,.59,.20)
        base=g.custom('return Tint*(.92+.12*dot(C,float3(.2126,.7152,.0722)));',F3,Tint=g.color(color),C=base)
        rough=g.custom('return clamp(.20+R.r*.13,.2,.34);',F1,R=rough);metal=g.scalar(1);strength=.10
        if slot=='MAT_Gold_CoinHeap':
            uv=g.node(ue.MaterialExpressionTextureCoordinate)
            coins=g.custom('float2 p=UV*1.86;p.x+=fmod(floor(p.y),2)*.5;float d=length(frac(p)-.5);return 1-smoothstep(.40,.49,d);',F1,UV=uv)
            base=g.custom('return C*lerp(.22,1,Coins);',F3,C=base,Coins=coins)
    if slot.startswith('MAT_Gem_'):
        tint={'Amethyst':'AE61CF','Emerald':'14834A','Ruby':'BE182E','Sapphire':'1B59C6','Topaz':'DD811C'}[slot.removeprefix('MAT_Gem_')]
        f=g.node(ue.MaterialExpressionFresnel,exponent=3.,base_reflect_fraction=.04)
        base=g.custom('return C*(.8+.6*F);',F3,C=g.color(linear(tint)),F=f)
        rough=g.scalar(.075);g.out(g.scalar(.95),MP.MP_SPECULAR)
    if slot.startswith('MAT_Pearl_'):
        f=g.node(ue.MaterialExpressionFresnel,exponent=2.,base_reflect_fraction=.08)
        tint='292F35' if slot.endswith('Black') else 'DDD3BE'
        base=g.custom('return C+float3(.04,.055,.07)*F;',F3,C=g.color(linear(tint)),F=f)
        rough=g.scalar(.20);g.out(g.scalar(.75),MP.MP_SPECULAR)
    if kind=='LanternHorn':
        base=g.color((.85,.53,.17));g.out(g.color((3.2,1.65,.45)),MP.MP_EMISSIVE_COLOR)
    if kind=='Flag':
        mat.set_editor_property('two_sided',True)
        uv=g.node(ue.MaterialExpressionTextureCoordinate)
        tiled=g.custom('float2 t=UV*float2(5,4);t.x+=fmod(floor(t.y),2)*.5;return frac(t);',F2,UV=uv)
        motif=next(p for p in SOURCES[slot]['images'].values() if 'FleurDeLys' in p)
        tex=sample(g,motif,'diff',tiled)
        mask=g.node(ue.MaterialExpressionComponentMask,r=False,g=False,b=False,a=True);g.wire(tex,mask,'','RGBA')
        base=mix(base,g.color(linear('BA963A')),mask)
    if kind in ('MoatWater','Puddle') or slot=='MAT-Site_Lagoon':
        water_source='MAT-Site_Lagoon'
        base=g.color((.017,.095,.075) if kind=='MoatWater' else (.035,.068,.044))
        if slot=='MAT-Site_Lagoon':base=g.custom('return lerp(float3(.037,.24,.20),float3(.006,.036,.065),saturate(V.r*1.6));',F3,V=v)
        uv=g.custom('return P.xy/270;',F2,P=pos);time=g.node(ue.MaterialExpressionTime)
        uv=g.custom('return UV+float2(T*.012,T*.008);',F2,UV=uv,T=time)
        normal=sample(g,next(iter(SOURCES[water_source]['images'].values())),'normal',uv)
        strength=.25;rough=g.scalar(.09);g.out(g.scalar(.65),MP.MP_SPECULAR)
        mat.set_editor_property('two_sided',True)
        if kind=='MoatWater':
            mat.set_editor_property('blend_mode',ue.BlendMode.BLEND_TRANSLUCENT)
            mat.set_editor_property('translucency_lighting_mode',ue.TranslucencyLightingMode.TLM_SURFACE)
            fade=g.node(ue.MaterialExpressionDepthFade,fade_distance_default=70.,opacity_default=.80)
            g.out(fade,MP.MP_OPACITY)
    # Site uses its authored multi-layer PBR terrain mask. Its saved material
    # is intentionally reused; every building surface is rebuilt above.
    g.out(base,MP.MP_BASE_COLOR);g.out(rough,MP.MP_ROUGHNESS);g.out(metal,MP.MP_METALLIC)
    if normal:g.out(g.custom(f'return normalize(float3(N.xy*{strength},max(.05,N.z)));',F3,N=normal),MP.MP_NORMAL)
    errors=MEL.recompile_material(mat);assert not errors,(slot,list(errors));save(mat)
    return mat


def main():
    mats={}
    for slot in list(SOURCES)+['MAT-Rework_'+s for s in EXTRA]:
        if slot=='MAT-Site_Ground':mats[slot]=load(DEST+'/Materials/M_Site_Ground')
        else:mats[slot]=make(slot)
        assert mats[slot],slot
        ue.log('T198_MATERIAL '+slot)
    (OUT/'materials-result.json').write_text(json.dumps({s:m.get_path_name().split('.')[0] for s,m in mats.items()},indent=2),encoding='utf-8')
    return mats
