"""Blender helpers: real gun deck, open stairwell, complete fourteen cannon carriages."""
import math
import random
import bmesh
import bpy
from mathutils import Matrix, Vector

# Central forward hatch: away from the main mast and the cabin/merchant circulation.
WELL = (-.60, .60, -5.45, -2.55)
FLOOR = .20
STAIR_LOW = (0, -2.6, FLOOR)
STAIR_HIGH = (0, -5.2, 1.64)

def cut_box(obj, low, high):
    """Cut faces by six planes, independent of the delivered mesh's winding."""
    obj.data=obj.data.copy()
    bm=bmesh.new();bm.from_mesh(obj.data)
    bmesh.ops.transform(bm,matrix=obj.matrix_world,verts=list(bm.verts))
    for axis in range(3):
        normal=Vector(tuple(1 if i==axis else 0 for i in range(3)))
        for limit in (low[axis],high[axis]):
            origin=normal*limit
            bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),
                plane_co=origin,plane_no=normal,dist=.00001,clear_inner=False,clear_outer=False)
    faces=[f for f in bm.faces if all(low[i]-.00001<=f.calc_center_median()[i]<=high[i]+.00001 for i in range(3))]
    assert faces,obj.name
    bmesh.ops.delete(bm,geom=faces,context="FACES")
    bmesh.ops.transform(bm,matrix=obj.matrix_world.inverted(),verts=list(bm.verts))
    bm.to_mesh(obj.data);bm.free();obj.data.update()

def add_hold(structure, collection, assembly):
    from galleon import materials as GM, cannons, hullform as HF, assembly_ucx as UCX
    # Existing Blender material datablocks only, no source creation or disk writes.
    mats = {key: bpy.data.materials.get(GM._name(key)) for key in GM.RECIPES}
    needed = ("deck", "oak", "oak_light", "oak_dark", "iron", "iron_rust", "rope", "pitch", "lamp")
    assert all(mats[key] for key in needed)
    GM.tone = lambda key, rng=None, bias=0: mats[key]
    rng = random.Random(184)
    x0, x1, y0, y1 = WELL
    # Remove the decorative closed grates and any deck beams across the opening.
    for obj in structure:
        if obj.name in ("Galleon_Decks_Planks", "Galleon_Decks_Beams", "Galleon_Decks_Hatches"):
            cut_box(obj, (x0, y0, .8), (x1, y1, 2.2))
    # A real interior needs through-open sabords, without their opaque fake backs.
    removed_niche_faces = 0
    for obj in structure:
        if not obj.name.startswith("Galleon_Hull"):
            continue
        bm = bmesh.new(); bm.from_mesh(obj.data)
        faces = [f for f in bm.faces if "DarkRecess" in obj.data.materials[f.material_index].name
                 and all(.2 < (obj.matrix_world @ v.co).z < 1.5 for v in f.verts)]
        removed_niche_faces += len(faces)
        bmesh.ops.delete(bm, geom=faces, context="FACES")
        bm.to_mesh(obj.data); bm.free()
    # Full cannons: the delivery uses truncated barrels at these same fourteen sockets.
    old_battery = bpy.data.objects["Galleon_Cannons_Battery"]
    structure.remove(old_battery)
    old_battery.name = "T184_PreviousTruncatedBattery"
    guns = GM.Batch("Galleon_Cannons_Battery")
    foundations = []
    cannon_bounds = []
    for gun in cannons.gun_positions():
        if gun["kind"] != "demi_culverin":
            continue
        bm, materials, info = cannons.cannon("demi_culverin", mats, rng, 0.0, segs=20, segs_wheel=14, detail="high", trunc=None)
        muzzle = info["muzzle"]
        origin = gun["p"] + gun["d"] * (cannons.PROTRUDE-muzzle.y)
        origin.z = gun["z"]-muzzle.z
        matrix = cannons._m4(origin, gun["d"])
        points = [matrix @ v.co for v in bm.verts]
        low = Vector(tuple(min(p[i] for p in points) for i in range(3)))
        high = Vector(tuple(max(p[i] for p in points) for i in range(3)))
        cannon_bounds.append((gun["name"], low, high))
        foundations.append((origin.copy(), gun["d"].copy(), gun["name"]))
        cannons.place(guns, bm, materials, matrix)
    battery = guns.finish(collection)
    battery.data.calc_loop_triangles()
    structure.append(battery)
    interior = GM.Batch("Galleon_Hold_Interior")
    extras = []
    def box(label, c, s, material, collision=True):
        interior.box(c, s, mats[material], bevel=.006, long_u=True)
        if collision:
            extras.append((label, None, UCX._box(c, s)))
    # Planked deck inside the lower hull; ends are bulkheaded off from the inaccessible bilge.
    for iy in range(55):
        y = -7.7+(iy+.5)*.26
        width = min(2.95, HF.width_at(y,FLOOR)-.14)
        box("hold_plank", (0, y, FLOOR-.045), (2*width, .253, .09), "deck", False)
    floor_points = [Vector((side*min(2.95,HF.width_at(y,FLOOR)-.14),y,z))
                    for y in (-7.7,-6,-3,0,3,6.6) for side in (-1,1) for z in (FLOOR-.12,FLOOR)]
    extras.append(("hold_floor", None, floor_points))
    for y in (-7.7, 6.6):
        box("hold_end", (0, y, .67), (2*min(2.95,HF.width_at(y,.7)-.14), .10, 1.5), "oak")
    # Cannon shoes follow the low deck level up to the historic muzzle line.
    for origin, direction, name in foundations:
        height = origin.z-FLOOR
        center = origin-direction*.30
        rotation = Matrix.Rotation(math.atan2(direction.y, direction.x)-math.pi/2, 3, "Z")
        interior.box((center.x, center.y, FLOOR+height/2), (1.02, 1.8, height), mats["oak_dark"], R=rotation, bevel=.02)
    # Handcrafted treads, stringers and handrails. Collision is a continuous walkable ramp.
    low, high = Vector(STAIR_LOW), Vector(STAIR_HIGH)
    n = 11
    for k in range(n):
        t = (k+.5)/n
        p = low.lerp(high, t)
        box("hold_tread", (0, p.y, p.z-.035), (1.12, .26, .07), "oak_light", False)
    for side in (-1, 1):
        for offset, radius, material in ((-.08,.045,"oak_dark"),(.65,.025,"oak")):
            a = low+Vector((side*.57,0,offset))
            b = high+Vector((side*.57,0,offset))
            interior.tube([a,b], radius, mats[material], segs=8)
        for t in (.1,.5,.9):
            p = low.lerp(high,t)+Vector((side*.57,0,0))
            interior.tube([p,p+Vector((0,0,.65))],.018,mats["iron_rust"],segs=8)
    ramp = [Vector((x,low.y,z)) for x in (-.57,.57) for z in (FLOOR-.1,low.z)]
    ramp += [Vector((x,high.y,z)) for x in (-.57,.57) for z in (FLOOR-.1,high.z)]
    extras.append(("hold_stairs",None,ramp))
    # Raised curbs protect the sides of the deck opening without sealing its stair entry.
    for side in (-1,1):
        box("hold_hatch_curb", (side*.66, (y0+y1)/2, 1.67), (.10,y1-y0,.14), "oak")
    structure.append(interior.finish(collection))
    return {"battery_triangles":len(battery.data.loop_triangles), "gun_count":len(cannon_bounds),
            "removed_niche_faces":removed_niche_faces, "stair_low":list(low), "stair_high":list(high)}, extras

def hollow_collision(specs, extras):
    from galleon import hullform as HF, assembly_ucx as UCX
    result = []
    x0,x1,y0,y1=WELL
    for label, _, points in specs:
        if label.startswith("hull_"):
            # Keep the submerged lower hull and replace the upper solid core by two side walls.
            clipped=[Vector((p.x,p.y,min(p.z,FLOOR-.10))) for p in points]
            result.append((label+"_bilge",None,clipped))
            ya,yb=min(p.y for p in points),max(p.y for p in points)
            for side in (-1,1):
                shell=[]
                for y in (ya,yb):
                    for z in (FLOOR-.10,1.5):
                        w=max(.05,HF.width_at(y,z))
                        for inset in (0,.10):
                            shell.append(Vector((side*(w-inset),y,z)))
                result.append((label+"_side"+str(side),None,shell))
        elif label=="deck_main":
            # A convex deck cannot represent a hole: four slabs surround the stairwell.
            xmin,xmax=min(p.x for p in points),max(p.x for p in points)
            ymin,ymax=min(p.y for p in points),max(p.y for p in points)
            zmin,zmax=min(p.z for p in points),max(p.z for p in points)
            for suffix,xa,xb,ya,yb in (("P",xmin,x0,ymin,ymax),("S",x1,xmax,ymin,ymax),
                                      ("fore",x0,x1,ymin,y0),("aft",x0,x1,y1,ymax)):
                result.append((label+suffix,None,UCX._box(((xa+xb)/2,(ya+yb)/2,(zmin+zmax)/2),(xb-xa,yb-ya,zmax-zmin))))
        elif label.startswith("hatch_"):
            continue
        else:
            result.append((label,None,points))
    return result+extras
