import sys
import nexus_engine

import time

last_entity_count = -1
selected_entity_id = -1
prev_selected_entity_id = -1

expanded_nodes = set()
last_rebuild_time = 0.0
force_rebuild = False

def rebuild_hierarchy(ui):
    global last_entity_count, last_rebuild_time, force_rebuild
    
    count = nexus_engine.get_scene_entity_count()
    now = time.time()
    
    if not force_rebuild:
        if count == last_entity_count:
            return
        if now - last_rebuild_time < 0.5:
            return

    last_entity_count = count
    last_rebuild_time = now
    force_rebuild = False

    all_ents = nexus_engine.get_all_entities()
    roots = [e for e in all_ents if e.get_parent() == -1]
    
    html = ""
    
    def build_tree(entity, depth):
        nonlocal html
        pad = 10 + depth * 15
        name = entity.name
        
        has_children = entity.has_children()
        is_expanded = entity.id in expanded_nodes
        prefix = "[-] " if (has_children and is_expanded) else ("[+] " if has_children else "    ")
        
        cls = "tree-node selected" if entity.id == selected_entity_id else "tree-node"
        
        html += f'<div class="{cls}" id="entity-{entity.id}" style="padding-left: {pad}dp">{prefix}{name}</div>'
        
        if has_children and is_expanded:
            for child in entity.get_children():
                build_tree(child, depth + 1)
                
    for root in roots:
        build_tree(root, 0)
        
    ui.set_element_rml("hierarchy-tree", html)

def update_selection_highlight(ui):
    global prev_selected_entity_id, selected_entity_id
    
    if prev_selected_entity_id == selected_entity_id:
        return
    
    if prev_selected_entity_id != -1:
        ui.remove_element_class(f"entity-{prev_selected_entity_id}", "selected")
    
    if selected_entity_id != -1:
        ui.add_element_class(f"entity-{selected_entity_id}", "selected")
    
    prev_selected_entity_id = selected_entity_id

def on_update(dt):
    fps = nexus_engine.get_fps()
    api_draws = nexus_engine.get_api_draws()
    visible_meshes = nexus_engine.get_draw_calls()
    triangles = nexus_engine.get_triangles()
    frame_time = nexus_engine.get_frame_time()

    ui = nexus_engine.UIWrapper()
    ui.set_element_rml("prop-fps", f"{fps:.1f}")
    ui.set_element_rml("prop-api-draws", str(api_draws))
    ui.set_element_rml("prop-visible-meshes", str(visible_meshes))
    ui.set_element_rml("prop-triangles", str(triangles))
    ui.set_element_rml("prop-frame-time", f"{frame_time:.2f} ms")

    rebuild_hierarchy(ui)
    update_selection_highlight(ui)
    
    if selected_entity_id != -1:
        ent = nexus_engine.get_entity(selected_entity_id)
        if ent.is_valid():
            ui.set_element_rml("prop-entity-id", str(ent.id))
            ui.set_element_rml("prop-entity-name", ent.name)
            pos = ent.get_position()
            ui.set_element_attribute("prop-pos-x", "value", f"{pos[0]:.4f}")
            ui.set_element_attribute("prop-pos-y", "value", f"{pos[1]:.4f}")
            ui.set_element_attribute("prop-pos-z", "value", f"{pos[2]:.4f}")
            rot = ent.get_rotation()
            ui.set_element_attribute("prop-rot-x", "value", f"{rot[0]:.4f}")
            ui.set_element_attribute("prop-rot-y", "value", f"{rot[1]:.4f}")
            ui.set_element_attribute("prop-rot-z", "value", f"{rot[2]:.4f}")

def on_hierarchy_click(params):
    global selected_entity_id, expanded_nodes, force_rebuild
    
    ent_id_str = params.get("id", "")
    nexus_engine.log_info(f"[Python] Clicked on id: {ent_id_str}")
    
    if ent_id_str.startswith("entity-"):
        try:
            eid = int(ent_id_str.split("-")[1])
            selected_entity_id = eid
            
            if eid in expanded_nodes:
                expanded_nodes.remove(eid)
                nexus_engine.log_info(f"[Python] Collapsed node {eid}")
            else:
                expanded_nodes.add(eid)
                nexus_engine.log_info(f"[Python] Expanded node {eid}")
                
            force_rebuild = True
        except Exception as e:
            nexus_engine.log_info(f"[Python] Error in click: {str(e)}")

def on_prop_change(params):
    global selected_entity_id
    if selected_entity_id == -1: return
    
    ent = nexus_engine.get_entity(selected_entity_id)
    if not ent.is_valid(): return
    
    try:
        val = float(params.get("value", 0))
    except:
        return
    
    pos = list(ent.get_position())
    rot = list(ent.get_rotation())
    
    el_id = params.get("id", "")
    if el_id.startswith("prop-pos"):
        if el_id == "prop-pos-x": pos[0] = val
        elif el_id == "prop-pos-y": pos[1] = val
        elif el_id == "prop-pos-z": pos[2] = val
        ent.set_position(pos[0], pos[1], pos[2])
    elif el_id.startswith("prop-rot"):
        if el_id == "prop-rot-x": rot[0] = val
        elif el_id == "prop-rot-y": rot[1] = val
        elif el_id == "prop-rot-z": rot[2] = val
        ent.set_rotation(rot[0], rot[1], rot[2])

def main():
    nexus_engine.log_info("Python Editor Script Initializing...")
    nexus_engine.set_update_callback(on_update)

    ui = nexus_engine.UIWrapper()
    ui.register_event_callback("hierarchy-tree", "click", on_hierarchy_click)
    ui.register_event_callback("prop-pos-x", "change", on_prop_change)
    ui.register_event_callback("prop-pos-y", "change", on_prop_change)
    ui.register_event_callback("prop-pos-z", "change", on_prop_change)
    ui.register_event_callback("prop-rot-x", "change", on_prop_change)
    ui.register_event_callback("prop-rot-y", "change", on_prop_change)
    ui.register_event_callback("prop-rot-z", "change", on_prop_change)
    
    nexus_engine.log_info("Python Editor Setup Complete.")

main()
