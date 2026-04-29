import sys
import nexus_engine

last_entity_count = -1
selected_entity_id = -1
prev_selected_entity_id = -1

def rebuild_hierarchy(ui):
    global last_entity_count
    
    count = nexus_engine.get_scene_entity_count()
    if count == last_entity_count:
        return
    last_entity_count = count

    all_ents = nexus_engine.get_all_entities()
    roots = [e for e in all_ents if e.get_parent() == -1]
    
    html = ""
    
    def build_tree(entity, depth):
        nonlocal html
        pad = 10 + depth * 15
        name = entity.name
        
        has_children = entity.has_children()
        prefix = "[-] " if has_children else "    "
        
        cls = "tree-node selected" if entity.id == selected_entity_id else "tree-node"
        
        html += f'<div class="{cls}" id="entity-{entity.id}" style="padding-left: {pad}dp">{prefix}{name}</div>'
        
        if has_children:
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
    draw_calls = nexus_engine.get_draw_calls()
    triangles = nexus_engine.get_triangles()
    frame_time = nexus_engine.get_frame_time()

    ui = nexus_engine.UIWrapper()
    ui.set_element_rml("prop-fps", f"{fps:.1f}")
    ui.set_element_rml("prop-draw-calls", str(draw_calls))
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

def on_hierarchy_click(params):
    global selected_entity_id
    
    ent_id_str = params.get("id", "")
    if ent_id_str.startswith("entity-"):
        try:
            selected_entity_id = int(ent_id_str.split("-")[1])
        except:
            pass

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
    
    el_id = params.get("id", "")
    if el_id == "prop-pos-x": pos[0] = val
    elif el_id == "prop-pos-y": pos[1] = val
    elif el_id == "prop-pos-z": pos[2] = val
    
    ent.set_position(pos[0], pos[1], pos[2])

def main():
    nexus_engine.log_info("Python Editor Script Initializing...")
    nexus_engine.set_update_callback(on_update)

    ui = nexus_engine.UIWrapper()
    ui.register_event_callback("hierarchy-tree", "click", on_hierarchy_click)
    ui.register_event_callback("prop-pos-x", "change", on_prop_change)
    ui.register_event_callback("prop-pos-y", "change", on_prop_change)
    ui.register_event_callback("prop-pos-z", "change", on_prop_change)
    
    nexus_engine.log_info("Python Editor Setup Complete.")

main()
