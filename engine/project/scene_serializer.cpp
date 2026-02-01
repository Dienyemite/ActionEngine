#include "scene_serializer.h"
#include "serialization/json_format.h"
#include "physics/collision_shapes.h"
#include "core/logging.h"

namespace action {

static constexpr u32 SCENE_VERSION = 1;

SceneSerializer::SceneSerializer(ECS& ecs, Editor& editor)
    : m_ecs(ecs)
    , m_editor(editor)
{
}

bool SceneSerializer::SaveScene(const std::string& file_path) {
    SerialObject scene_obj = SerializeScene();
    
    if (!JsonFormat::SaveToFile(file_path, scene_obj, true)) {
        LOG_ERROR("[SceneSerializer] Failed to save scene to: {}", file_path);
        return false;
    }
    
    LOG_INFO("[SceneSerializer] Saved scene to: {}", file_path);
    return true;
}

bool SceneSerializer::LoadScene(const std::string& file_path) {
    SerialObject obj = JsonFormat::LoadFromFile(file_path);
    
    if (obj.type_name.empty()) {
        LOG_ERROR("[SceneSerializer] Failed to load scene from: {}", file_path);
        return false;
    }
    
    // Clear existing scene first
    ClearScene();
    
    if (!DeserializeScene(obj)) {
        LOG_ERROR("[SceneSerializer] Failed to deserialize scene");
        return false;
    }
    
    LOG_INFO("[SceneSerializer] Loaded scene from: {}", file_path);
    return true;
}

void SceneSerializer::ClearScene() {
    // Clear editor scene tree
    m_editor.GetSceneRoot().children.clear();
    m_editor.ClearSelection();
    
    // Note: We don't destroy all ECS entities here as some might be system entities
    // The caller should handle entity cleanup as needed
}

SerialObject SceneSerializer::SerializeScene() {
    SerialObject obj;
    obj.type_name = "ActionEngineScene";
    obj.Set("version", static_cast<i64>(SCENE_VERSION));
    
    // Serialize editor hierarchy
    SerialObject hierarchy = SerializeEditorNode(m_editor.GetSceneRoot());
    obj.SetObject("hierarchy", hierarchy);
    
    // Serialize all entities with components
    std::vector<SerialObject> entities;
    
    // Iterate through editor nodes to get entities
    std::function<void(const EditorNode&)> collect_entities = [&](const EditorNode& node) {
        if (node.entity != INVALID_ENTITY && m_ecs.IsAlive(node.entity)) {
            entities.push_back(SerializeEntity(node.entity));
        }
        for (const auto& child : node.children) {
            collect_entities(child);
        }
    };
    
    for (const auto& child : m_editor.GetSceneRoot().children) {
        collect_entities(child);
    }
    
    obj.SetArray("entities", entities);
    
    return obj;
}

bool SceneSerializer::DeserializeScene(const SerialObject& obj) {
    if (obj.type_name != "ActionEngineScene") {
        LOG_ERROR("[SceneSerializer] Invalid scene file type: {}", obj.type_name);
        return false;
    }
    
    i64 version = obj.Get<i64>("version", 0);
    if (version > SCENE_VERSION) {
        LOG_WARN("[SceneSerializer] Scene version {} is newer than supported {}", version, SCENE_VERSION);
    }
    
    // Map of old entity IDs to new entity IDs
    std::unordered_map<u32, Entity> entity_map;
    
    // Deserialize entities first
    if (auto* entities = obj.GetArray("entities")) {
        for (const auto& ent_obj : *entities) {
            u32 old_id = static_cast<u32>(ent_obj.Get<i64>("id", 0));
            Entity new_entity = DeserializeEntity(ent_obj);
            if (new_entity != INVALID_ENTITY) {
                entity_map[old_id] = new_entity;
            }
        }
    }
    
    // Deserialize hierarchy and link to entities
    if (auto* hierarchy = obj.GetObject("hierarchy")) {
        EditorNode root_copy = DeserializeEditorNode(*hierarchy);
        
        // Update entity references using the map
        std::function<void(EditorNode&)> update_refs = [&](EditorNode& node) {
            if (auto it = entity_map.find(node.entity); it != entity_map.end()) {
                node.entity = it->second;
            }
            for (auto& child : node.children) {
                update_refs(child);
            }
        };
        
        for (auto& child : root_copy.children) {
            update_refs(child);
            m_editor.GetSceneRoot().children.push_back(std::move(child));
        }
    }
    
    return true;
}

// ===== Component Serialization =====

SerialObject SceneSerializer::SerializeTransform(const TransformComponent& t) {
    SerialObject obj;
    obj.type_name = "TransformComponent";
    
    SerialObject pos, rot, scl;
    pos.Set("x", static_cast<f64>(t.position.x));
    pos.Set("y", static_cast<f64>(t.position.y));
    pos.Set("z", static_cast<f64>(t.position.z));
    obj.SetObject("position", pos);
    
    rot.Set("x", static_cast<f64>(t.rotation.x));
    rot.Set("y", static_cast<f64>(t.rotation.y));
    rot.Set("z", static_cast<f64>(t.rotation.z));
    rot.Set("w", static_cast<f64>(t.rotation.w));
    obj.SetObject("rotation", rot);
    
    scl.Set("x", static_cast<f64>(t.scale.x));
    scl.Set("y", static_cast<f64>(t.scale.y));
    scl.Set("z", static_cast<f64>(t.scale.z));
    obj.SetObject("scale", scl);
    
    return obj;
}

SerialObject SceneSerializer::SerializeRender(const RenderComponent& r) {
    SerialObject obj;
    obj.type_name = "RenderComponent";
    
    obj.Set("mesh_index", static_cast<i64>(r.mesh.index));
    obj.Set("mesh_generation", static_cast<i64>(r.mesh.generation));
    obj.Set("material_index", static_cast<i64>(r.material.index));
    obj.Set("material_generation", static_cast<i64>(r.material.generation));
    obj.Set("lod_level", static_cast<i64>(r.lod_level));
    obj.Set("visible", r.visible);
    obj.Set("cast_shadow", r.cast_shadow);
    
    return obj;
}

SerialObject SceneSerializer::SerializeCollider(const ColliderComponent& c) {
    SerialObject obj;
    obj.type_name = "ColliderComponent";
    
    obj.Set("type", static_cast<i64>(static_cast<u8>(c.type)));
    obj.Set("layer", static_cast<i64>(static_cast<u32>(c.layer)));
    obj.Set("mask", static_cast<i64>(static_cast<u32>(c.mask)));
    
    SerialObject offset;
    offset.Set("x", static_cast<f64>(c.offset.x));
    offset.Set("y", static_cast<f64>(c.offset.y));
    offset.Set("z", static_cast<f64>(c.offset.z));
    obj.SetObject("offset", offset);
    
    obj.Set("radius", static_cast<f64>(c.radius));
    obj.Set("height", static_cast<f64>(c.height));
    
    SerialObject half_ext;
    half_ext.Set("x", static_cast<f64>(c.half_extents.x));
    half_ext.Set("y", static_cast<f64>(c.half_extents.y));
    half_ext.Set("z", static_cast<f64>(c.half_extents.z));
    obj.SetObject("half_extents", half_ext);
    
    obj.Set("is_trigger", c.is_trigger);
    obj.Set("is_static", c.is_static);
    
    return obj;
}

SerialObject SceneSerializer::SerializeRigidbody(const RigidbodyComponent& rb) {
    SerialObject obj;
    obj.type_name = "RigidbodyComponent";
    
    obj.Set("mass", static_cast<f64>(rb.mass));
    obj.Set("friction", static_cast<f64>(rb.friction));
    obj.Set("restitution", static_cast<f64>(rb.restitution));
    obj.Set("linear_damping", static_cast<f64>(rb.linear_damping));
    obj.Set("angular_damping", static_cast<f64>(rb.angular_damping));
    obj.Set("gravity_factor", static_cast<f64>(rb.gravity_factor));
    obj.Set("is_kinematic", rb.is_kinematic);
    obj.Set("use_ccd", rb.use_ccd);
    
    return obj;
}

SerialObject SceneSerializer::SerializeTag(const TagComponent& tag) {
    SerialObject obj;
    obj.type_name = "TagComponent";
    
    obj.Set("name", tag.name);
    obj.Set("tags", static_cast<i64>(tag.tags));
    
    return obj;
}

SerialObject SceneSerializer::SerializeVelocity(const VelocityComponent& vel) {
    SerialObject obj;
    obj.type_name = "VelocityComponent";
    
    SerialObject lin, ang;
    lin.Set("x", static_cast<f64>(vel.linear.x));
    lin.Set("y", static_cast<f64>(vel.linear.y));
    lin.Set("z", static_cast<f64>(vel.linear.z));
    obj.SetObject("linear", lin);
    
    ang.Set("x", static_cast<f64>(vel.angular.x));
    ang.Set("y", static_cast<f64>(vel.angular.y));
    ang.Set("z", static_cast<f64>(vel.angular.z));
    obj.SetObject("angular", ang);
    
    return obj;
}

// ===== Component Deserialization =====

void SceneSerializer::DeserializeTransform(Entity entity, const SerialObject& obj) {
    TransformComponent t;
    
    if (auto* pos = obj.GetObject("position")) {
        t.position.x = static_cast<float>(pos->Get<f64>("x", 0.0));
        t.position.y = static_cast<float>(pos->Get<f64>("y", 0.0));
        t.position.z = static_cast<float>(pos->Get<f64>("z", 0.0));
    }
    
    if (auto* rot = obj.GetObject("rotation")) {
        t.rotation.x = static_cast<float>(rot->Get<f64>("x", 0.0));
        t.rotation.y = static_cast<float>(rot->Get<f64>("y", 0.0));
        t.rotation.z = static_cast<float>(rot->Get<f64>("z", 0.0));
        t.rotation.w = static_cast<float>(rot->Get<f64>("w", 1.0));
    }
    
    if (auto* scl = obj.GetObject("scale")) {
        t.scale.x = static_cast<float>(scl->Get<f64>("x", 1.0));
        t.scale.y = static_cast<float>(scl->Get<f64>("y", 1.0));
        t.scale.z = static_cast<float>(scl->Get<f64>("z", 1.0));
    }
    
    m_ecs.AddComponent<TransformComponent>(entity, t);
}

void SceneSerializer::DeserializeRender(Entity entity, const SerialObject& obj) {
    RenderComponent r;
    
    r.mesh.index = static_cast<u32>(obj.Get<i64>("mesh_index", UINT32_MAX));
    r.mesh.generation = static_cast<u32>(obj.Get<i64>("mesh_generation", 0));
    r.material.index = static_cast<u32>(obj.Get<i64>("material_index", UINT32_MAX));
    r.material.generation = static_cast<u32>(obj.Get<i64>("material_generation", 0));
    r.lod_level = static_cast<u8>(obj.Get<i64>("lod_level", 0));
    r.visible = obj.Get<bool>("visible", true);
    r.cast_shadow = obj.Get<bool>("cast_shadow", true);
    
    m_ecs.AddComponent<RenderComponent>(entity, r);
}

void SceneSerializer::DeserializeCollider(Entity entity, const SerialObject& obj) {
    ColliderComponent c;
    
    c.type = static_cast<ColliderType>(static_cast<u8>(obj.Get<i64>("type", 0)));
    c.layer = static_cast<CollisionLayer>(static_cast<u32>(obj.Get<i64>("layer", 1)));
    c.mask = static_cast<CollisionLayer>(static_cast<u32>(obj.Get<i64>("mask", 0xFFFFFFFF)));
    
    if (auto* offset = obj.GetObject("offset")) {
        c.offset.x = static_cast<float>(offset->Get<f64>("x", 0.0));
        c.offset.y = static_cast<float>(offset->Get<f64>("y", 0.0));
        c.offset.z = static_cast<float>(offset->Get<f64>("z", 0.0));
    }
    
    c.radius = static_cast<float>(obj.Get<f64>("radius", 0.5));
    c.height = static_cast<float>(obj.Get<f64>("height", 2.0));
    
    if (auto* half_ext = obj.GetObject("half_extents")) {
        c.half_extents.x = static_cast<float>(half_ext->Get<f64>("x", 0.5));
        c.half_extents.y = static_cast<float>(half_ext->Get<f64>("y", 0.5));
        c.half_extents.z = static_cast<float>(half_ext->Get<f64>("z", 0.5));
    }
    
    c.is_trigger = obj.Get<bool>("is_trigger", false);
    c.is_static = obj.Get<bool>("is_static", false);
    
    m_ecs.AddComponent<ColliderComponent>(entity, c);
}

void SceneSerializer::DeserializeRigidbody(Entity entity, const SerialObject& obj) {
    RigidbodyComponent rb;
    
    rb.mass = static_cast<float>(obj.Get<f64>("mass", 1.0));
    rb.friction = static_cast<float>(obj.Get<f64>("friction", 0.5));
    rb.restitution = static_cast<float>(obj.Get<f64>("restitution", 0.3));
    rb.linear_damping = static_cast<float>(obj.Get<f64>("linear_damping", 0.05));
    rb.angular_damping = static_cast<float>(obj.Get<f64>("angular_damping", 0.05));
    rb.gravity_factor = static_cast<float>(obj.Get<f64>("gravity_factor", 1.0));
    rb.is_kinematic = obj.Get<bool>("is_kinematic", false);
    rb.use_ccd = obj.Get<bool>("use_ccd", false);
    
    m_ecs.AddComponent<RigidbodyComponent>(entity, rb);
}

void SceneSerializer::DeserializeTag(Entity entity, const SerialObject& obj) {
    TagComponent tag;
    
    tag.name = obj.Get<std::string>("name", "Entity");
    tag.tags = static_cast<u32>(obj.Get<i64>("tags", 0));
    
    m_ecs.AddComponent<TagComponent>(entity, tag);
}

void SceneSerializer::DeserializeVelocity(Entity entity, const SerialObject& obj) {
    VelocityComponent vel;
    
    if (auto* lin = obj.GetObject("linear")) {
        vel.linear.x = static_cast<float>(lin->Get<f64>("x", 0.0));
        vel.linear.y = static_cast<float>(lin->Get<f64>("y", 0.0));
        vel.linear.z = static_cast<float>(lin->Get<f64>("z", 0.0));
    }
    
    if (auto* ang = obj.GetObject("angular")) {
        vel.angular.x = static_cast<float>(ang->Get<f64>("x", 0.0));
        vel.angular.y = static_cast<float>(ang->Get<f64>("y", 0.0));
        vel.angular.z = static_cast<float>(ang->Get<f64>("z", 0.0));
    }
    
    m_ecs.AddComponent<VelocityComponent>(entity, vel);
}

// ===== Editor Node Serialization =====

SerialObject SceneSerializer::SerializeEditorNode(const EditorNode& node) {
    SerialObject obj;
    obj.type_name = "EditorNode";
    
    obj.Set("id", static_cast<i64>(node.id));
    obj.Set("entity", static_cast<i64>(node.entity));
    obj.Set("name", node.name);
    obj.Set("type", node.type);
    obj.Set("visible", node.visible);
    
    // Position/rotation/scale
    SerialObject pos, rot, scl, col;
    pos.Set("x", static_cast<f64>(node.position.x));
    pos.Set("y", static_cast<f64>(node.position.y));
    pos.Set("z", static_cast<f64>(node.position.z));
    obj.SetObject("position", pos);
    
    rot.Set("x", static_cast<f64>(node.rotation.x));
    rot.Set("y", static_cast<f64>(node.rotation.y));
    rot.Set("z", static_cast<f64>(node.rotation.z));
    obj.SetObject("rotation", rot);
    
    scl.Set("x", static_cast<f64>(node.scale.x));
    scl.Set("y", static_cast<f64>(node.scale.y));
    scl.Set("z", static_cast<f64>(node.scale.z));
    obj.SetObject("scale", scl);
    
    col.Set("r", static_cast<f64>(node.color.x));
    col.Set("g", static_cast<f64>(node.color.y));
    col.Set("b", static_cast<f64>(node.color.z));
    obj.SetObject("color", col);
    
    // Children
    std::vector<SerialObject> children;
    for (const auto& child : node.children) {
        children.push_back(SerializeEditorNode(child));
    }
    obj.SetArray("children", children);
    
    return obj;
}

EditorNode SceneSerializer::DeserializeEditorNode(const SerialObject& obj) {
    EditorNode node;
    
    node.id = static_cast<u32>(obj.Get<i64>("id", 0));
    node.entity = static_cast<Entity>(obj.Get<i64>("entity", INVALID_ENTITY));
    node.name = obj.Get<std::string>("name", "Node");
    node.type = obj.Get<std::string>("type", "Node3D");
    node.visible = obj.Get<bool>("visible", true);
    
    if (auto* pos = obj.GetObject("position")) {
        node.position.x = static_cast<float>(pos->Get<f64>("x", 0.0));
        node.position.y = static_cast<float>(pos->Get<f64>("y", 0.0));
        node.position.z = static_cast<float>(pos->Get<f64>("z", 0.0));
    }
    
    if (auto* rot = obj.GetObject("rotation")) {
        node.rotation.x = static_cast<float>(rot->Get<f64>("x", 0.0));
        node.rotation.y = static_cast<float>(rot->Get<f64>("y", 0.0));
        node.rotation.z = static_cast<float>(rot->Get<f64>("z", 0.0));
    }
    
    if (auto* scl = obj.GetObject("scale")) {
        node.scale.x = static_cast<float>(scl->Get<f64>("x", 1.0));
        node.scale.y = static_cast<float>(scl->Get<f64>("y", 1.0));
        node.scale.z = static_cast<float>(scl->Get<f64>("z", 1.0));
    }
    
    if (auto* col = obj.GetObject("color")) {
        node.color.x = static_cast<float>(col->Get<f64>("r", 0.8));
        node.color.y = static_cast<float>(col->Get<f64>("g", 0.8));
        node.color.z = static_cast<float>(col->Get<f64>("b", 0.8));
    }
    
    // Children
    if (auto* children = obj.GetArray("children")) {
        for (const auto& child_obj : *children) {
            node.children.push_back(DeserializeEditorNode(child_obj));
        }
    }
    
    return node;
}

// ===== Entity Serialization =====

SerialObject SceneSerializer::SerializeEntity(Entity entity) {
    SerialObject obj;
    obj.type_name = "Entity";
    obj.Set("id", static_cast<i64>(entity));
    
    // Serialize each component if present
    std::vector<SerialObject> components;
    
    if (auto* t = m_ecs.GetComponent<TransformComponent>(entity)) {
        components.push_back(SerializeTransform(*t));
    }
    
    if (auto* r = m_ecs.GetComponent<RenderComponent>(entity)) {
        components.push_back(SerializeRender(*r));
    }
    
    if (auto* c = m_ecs.GetComponent<ColliderComponent>(entity)) {
        components.push_back(SerializeCollider(*c));
    }
    
    if (auto* rb = m_ecs.GetComponent<RigidbodyComponent>(entity)) {
        components.push_back(SerializeRigidbody(*rb));
    }
    
    if (auto* tag = m_ecs.GetComponent<TagComponent>(entity)) {
        components.push_back(SerializeTag(*tag));
    }
    
    if (auto* vel = m_ecs.GetComponent<VelocityComponent>(entity)) {
        components.push_back(SerializeVelocity(*vel));
    }
    
    obj.SetArray("components", components);
    
    return obj;
}

Entity SceneSerializer::DeserializeEntity(const SerialObject& obj) {
    Entity entity = m_ecs.CreateEntity();
    
    if (auto* components = obj.GetArray("components")) {
        for (const auto& comp : *components) {
            const std::string& type = comp.type_name;
            
            if (type == "TransformComponent") {
                DeserializeTransform(entity, comp);
            } else if (type == "RenderComponent") {
                DeserializeRender(entity, comp);
            } else if (type == "ColliderComponent") {
                DeserializeCollider(entity, comp);
            } else if (type == "RigidbodyComponent") {
                DeserializeRigidbody(entity, comp);
            } else if (type == "TagComponent") {
                DeserializeTag(entity, comp);
            } else if (type == "VelocityComponent") {
                DeserializeVelocity(entity, comp);
            }
        }
    }
    
    return entity;
}

} // namespace action
