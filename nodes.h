// nodes.h
#pragma once

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
#include "ImNodes.h"
#include <vector>
#include <unordered_map>
// Include your mesh type declaration
#include "io.h"
#include "types.h"
#include <cassert>

/*------------------------------ Core Types ------------------------------*/
// typedef enum NodeType
// {
//     NODE_TYPE_MESH,
//     NODE_TYPE_VEC3,
//     NODE_TYPE_CUSTOM // Add new types here
// } NodeType;

enum component_type
{
    COMPONENT_TYPE_TRANSFORM = 1 << 0,
    COMPONENT_TYPE_MESH = 1 << 1,
    COMPONENT_TYPE_VEC3 = 1 << 2,
};

typedef struct Attribute
{
    int id;              // Unique attribute ID
    component_type type; // Data type
    // void *data;          // Pointer to associated data
    // size_t data_size;    // Size of data buffer
    bool is_input; // Input/output designation
    u32 owner_id;
} Attribute;

struct transform_component
{
    vec3 position;
    vec3 rotation;
    vec3 scale;
};

struct MeshComponent
{
    Mesh *mesh;
    gl_mesh *render_data;
};

struct Vec3Component
{
    vec3 value;
};

#define MAX_NUM_NODES 64
transform_component transform_data[MAX_NUM_NODES];
MeshComponent mesh_data[MAX_NUM_NODES];
Vec3Component vec3_data[MAX_NUM_NODES];

/**
 * @struct node_entity
 * @brief Represents a node entity with unique identification, components, connections, and attributes.
 *
 * This structure is used to define a node entity in a system, including its unique ID,
 * components, input/output connections, editable properties, and associated attributes.
 *
 * @var node_entity::id
 * Unique identifier for the node.
 *
 * @var node_entity::components
 * A 64-bit field representing the components associated with the node.
 *
 * @var node_entity::ins
 * A 64-bit field representing the input connections of the node.
 *
 * @var node_entity::outs
 * A 64-bit field representing the output connections of the node.
 *
 * @var node_entity::editables
 * A 64-bit field representing the editable properties of the node.
 *
 * @var node_entity::attributes
 * A vector of integers representing additional attributes associated with the node.
 *
 * @var node_entity::name
 * A character array (up to 32 characters) representing the display name of the node.
 */
typedef struct node_entity
{
    int id; // Unique node ID
    u64 components;
    u64 ins;
    u64 outs;
    u64 editables;

    std::vector<int> attributes;
    // NodeType type;                  // Node type identifier
    char name[32]; // Display name
    // std::vector<int> attribute_ids; // I/O attributes
    // void *node_data;                // Type-specific data
} node_entity;

typedef struct NodeLink
{
    int id;
    int start_attr_id; // Output attribute
    int end_attr_id;   // Input attribute
} NodeLink;

/**
 * @struct graph_context
 * @brief Represents the context of a graph, containing nodes, links, and attributes.
 *
 * This structure is used to manage and store the components of a graph, including
 * its nodes, links, and attributes. It also maintains counters for generating unique
 * IDs for nodes, attributes, and links.
 *
 * @var graph_context::nodes
 * A vector containing all the nodes in the graph.
 *
 * @var graph_context::links
 * A vector containing all the links (connections) between nodes in the graph.
 *
 * @var graph_context::attributes
 * A vector containing all the attributes associated with the graph.
 *
 * @var graph_context::next_node_id
 * A counter used to generate unique IDs for new nodes.
 *
 * @var graph_context::next_attr_id
 * A counter used to generate unique IDs for new attributes.
 *
 * @var graph_context::next_link_id
 * A counter used to generate unique IDs for new links.
 */
struct graph_context
{
    std::vector<node_entity> nodes;
    std::vector<NodeLink> links;
    std::vector<Attribute> attributes; // All attributes

    u32 next_node_id = 0;
    u32 next_attr_id = 0;
    u32 next_link_id = 0;
};

/*------------------------------ Core Functions ------------------------------*/
// Initialize new attribute and add to lookup
int create_attribute(graph_context *ctx, component_type type, bool is_input, u32 owner_id)
{
    Attribute attr;
    attr.id = ctx->next_attr_id++;
    attr.type = type;
    attr.is_input = is_input;
    attr.owner_id = owner_id;
    ctx->attributes.push_back(attr);
    return attr.id; // &ctx->attr_lookup[attr.id];
}

u32 init_node(graph_context *ctx, u64 components, const char *name, u64 in, u64 out, u64 editables)
{
    assert(ctx->next_node_id < MAX_NUM_NODES);
    node_entity ent = {};
    ent.components = components;
    ent.ins = in;
    ent.outs = out;
    ent.editables = editables;
    ent.id = ctx->next_node_id++;
    strncpy(ent.name, name, sizeof(ent.name));

    if (components & COMPONENT_TYPE_TRANSFORM)
    {
        if (in & COMPONENT_TYPE_TRANSFORM)
        {
            int attr_id = create_attribute(ctx, COMPONENT_TYPE_TRANSFORM, true, ent.id);
            ent.attributes.push_back(attr_id);
        }
        if (out & COMPONENT_TYPE_TRANSFORM)
        {
            int attr_id = create_attribute(ctx, COMPONENT_TYPE_TRANSFORM, false, ent.id);
            ent.attributes.push_back(attr_id);
        }
        transform_data[ent.id].position = {0, 0, 0};
        transform_data[ent.id].rotation = {0, 0, 0};
        transform_data[ent.id].scale = {1, 1, 1};
    }
    if (components & COMPONENT_TYPE_MESH)
    {
        if (in & COMPONENT_TYPE_MESH)
        {
            int attr_id = create_attribute(ctx, COMPONENT_TYPE_MESH, true, ent.id);
            ent.attributes.push_back(attr_id);
        }
        if (out & COMPONENT_TYPE_MESH)
        {
            int attr_id = create_attribute(ctx, COMPONENT_TYPE_MESH, false, ent.id);
            ent.attributes.push_back(attr_id);
        }
        mesh_data[ent.id].mesh = nullptr;
        mesh_data[ent.id].render_data = nullptr;
    }
    if (components & COMPONENT_TYPE_VEC3)
    {
        if (in & COMPONENT_TYPE_VEC3)
        {
            int attr_id = create_attribute(ctx, COMPONENT_TYPE_VEC3, true, ent.id);
            ent.attributes.push_back(attr_id);
        }
        if (out & COMPONENT_TYPE_VEC3)
        {
            int attr_id = create_attribute(ctx, COMPONENT_TYPE_VEC3, false, ent.id);
            ent.attributes.push_back(attr_id);
        }
        vec3_data[ent.id].value = {0, 0, 0};
    }
    ctx->nodes.push_back(ent);
    return ent.id;
}

/*------------------------------ Type-Specific Initializers ------------------------------*/
// Vec3 node initialization
u32 init_vec3_node(graph_context *ctx, vec3 initial_pos)
{
    u64 components = COMPONENT_TYPE_VEC3;
    u64 ins = 0;
    u64 outs = COMPONENT_TYPE_VEC3;
    u64 editables = COMPONENT_TYPE_VEC3;
    u32 node = init_node(ctx, components, "Vec3 Node", ins, outs, editables);
    vec3_data[node].value = initial_pos;
    return node;
}

// Mesh node initialization
u32 init_mesh_node(graph_context *ctx, Mesh *mesh, const char *name)
{
    u64 components = COMPONENT_TYPE_MESH | COMPONENT_TYPE_TRANSFORM;
    u64 ins = COMPONENT_TYPE_TRANSFORM;
    u64 outs = COMPONENT_TYPE_MESH;

    u32 node = init_node(ctx, components, name, ins, outs, components);

    if (mesh)
    {
        mesh_data[node].mesh = mesh;
        mesh_data[node].render_data = gl_render_add_mesh(mesh);
    }
    return node;
}

void copy_attrib_data(Attribute *dst, Attribute *src)
{

    // Initial data transfer
    if (src->type & COMPONENT_TYPE_TRANSFORM)
    {
        memcpy(&transform_data[dst->owner_id], &transform_data[src->owner_id], sizeof(transform_component));
    }
    if (src->type & COMPONENT_TYPE_MESH)
    {
        memcpy(&mesh_data[dst->owner_id], &mesh_data[src->owner_id], sizeof(MeshComponent));
    }
    if ((src->type & COMPONENT_TYPE_VEC3) && (dst->type & COMPONENT_TYPE_TRANSFORM))
    {
        memcpy(&transform_data[dst->owner_id], &vec3_data[src->owner_id], sizeof(vec3_data[src->owner_id]));
    }
    else if (src->type & COMPONENT_TYPE_VEC3)
    {
        memcpy(&vec3_data[dst->owner_id], &vec3_data[src->owner_id], sizeof(Vec3Component));
    }
}

bool are_attribute_types_compatible(Attribute *src, Attribute *dst)
{
    if (src->type == dst->type)
    {
        return true;
    }
    else if ((src->type & COMPONENT_TYPE_VEC3) && (dst->type & COMPONENT_TYPE_TRANSFORM))
    {
        return true;
    }
    return false;
}

/*------------------------------ Link Handling ------------------------------*/
bool create_link(graph_context *ctx, int output_attr, int input_attr)
{
    Attribute *dst = &ctx->attributes[input_attr];
    Attribute *src = &ctx->attributes[output_attr];

    // Type compatibility check
    if (!src || !dst || src->is_input || !dst->is_input || !are_attribute_types_compatible(src, dst))
    {
        // Error: Invalid connection
        return false;
    }

    // Create link
    NodeLink link;
    link.id = ctx->next_link_id++;
    link.start_attr_id = output_attr;
    link.end_attr_id = input_attr;
    ctx->links.push_back(link);

    copy_attrib_data(dst, src);

    return true;
}

/*------------------------------ Update System ------------------------------*/
void update_links(graph_context *ctx)
{
    for (auto &link : ctx->links)
    {
        Attribute *src = &ctx->attributes[link.start_attr_id];
        Attribute *dst = &ctx->attributes[link.end_attr_id];

        if (src && dst)
        {
            copy_attrib_data(dst, src);
        }
    }
}

/*------------------------------ Link Processing ------------------------------*/
// Unified function to handle both new link creation and data updates
inline void process_and_store_new_links(graph_context *ctx)
{
    // First update existing connections
    // update_links(ctx);

    // Then process new links
    int start_node_id = -1, start_attr_id = -1, end_node_id = -1, end_attr_id = -1;
    if (ImNodes::IsLinkCreated(
            &start_node_id,
            &start_attr_id,
            &end_node_id,
            &end_attr_id))
    {

        // Attempt to create validated link
        if (!create_link(ctx, start_attr_id, end_attr_id))
        {

            fprintf(stderr, "Failed to create link between attributes %d and %d\n\n",
                    start_attr_id, end_attr_id);
        }
    }

    // Optional: Second update to propagate new connection data
    update_links(ctx);
}

graph_context init_im_nodes()
{
    ImNodes::CreateContext();

    graph_context ctx = {}; // Initialize context data
    ctx.nodes.reserve(20);
    ctx.links.reserve(20);
    ctx.attributes.reserve(50);

    // Reset ID counters
    ctx.next_node_id = 0;
    ctx.next_attr_id = 0;
    ctx.next_link_id = 0;

    return ctx;
}

struct nodes_to_create
{
    bool mesh_node;
    bool vec3_node;
    ImVec2 position;
};

/*------------------------------ Context Menu ------------------------------*/
struct node_creation_request
{
    u64 components;
    u64 ins;
    u64 outs;
    u64 editables;
    ImVec2 position;
};

node_creation_request show_canvas_context_menu()
{
    node_creation_request request = {};

    if (ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        ImGui::OpenPopup("CanvasContextMenu");
        request.position = ImGui::GetMousePos();
    }

    if (ImGui::BeginPopup("CanvasContextMenu"))
    {

        if (ImGui::MenuItem("Mesh Node"))
        {
            request.components = COMPONENT_TYPE_MESH | COMPONENT_TYPE_TRANSFORM;
            request.ins = COMPONENT_TYPE_TRANSFORM;
            // request.editables = COMPONENT_TYPE_TRANSFORM;
            request.outs = 0;
            request.position = ImGui::GetMousePos();
        }
        if (ImGui::MenuItem("Vec3 Node"))
        {
            request.components = COMPONENT_TYPE_VEC3;
            request.position = ImGui::GetMousePos();
            request.outs = COMPONENT_TYPE_VEC3;
            request.ins = 0;
            request.editables = COMPONENT_TYPE_VEC3;
        }
        if (ImGui::MenuItem("Transform Node"))
        {
            request.components = COMPONENT_TYPE_TRANSFORM;
            request.position = ImGui::GetMousePos();
            request.outs = COMPONENT_TYPE_TRANSFORM;
            request.editables = COMPONENT_TYPE_TRANSFORM;
        }
        ImGui::EndPopup();
    }

    return request;
}

int get_node_attr_id(graph_context *ctx, node_entity *node, component_type type, bool is_input)
{
    for (int i = 0; i < node->attributes.size(); i++)
    {
        u32 attr_id = node->attributes[i];
        Attribute *attr = &ctx->attributes[attr_id];
        if (attr->type == type && attr->is_input == is_input)
        {
            return attr->id;
        }
    }
    return -1; // Not found
}

/*------------------------------ Node Drawing ------------------------------*/
void draw_generic_node(graph_context *ctx, node_entity *node)
{
    ImNodes::BeginNode(node->id);

    // Title bar with type-specific styling
    ImNodes::BeginNodeTitleBar();
    ImGui::TextUnformatted(node->name);
    ImNodes::EndNodeTitleBar();

    if (node->components & COMPONENT_TYPE_TRANSFORM)
    {
        if (node->editables & COMPONENT_TYPE_TRANSFORM)
        {
            // Transformation controls
            ImGui::PushItemWidth(300.0f);
            ImGui::InputFloat3("Location", &transform_data[node->id].position.x);
            ImGui::InputFloat3("Rotation", &transform_data[node->id].rotation.x);
            ImGui::InputFloat3("Scale", &transform_data[node->id].scale.x);
            ImGui::PopItemWidth();
        }
        if (node->ins & COMPONENT_TYPE_TRANSFORM)
        {
            int id = get_node_attr_id(ctx, node, COMPONENT_TYPE_TRANSFORM, true);
            ImNodes::BeginInputAttribute(id);
            ImGui::Text("Input Transform");
            ImNodes::EndInputAttribute();
        }
        if (node->outs & COMPONENT_TYPE_TRANSFORM)
        {

            int id = get_node_attr_id(ctx, node, COMPONENT_TYPE_TRANSFORM, false);
            ImNodes::BeginOutputAttribute(id);
            ImGui::Text("Output Transform");
            ImNodes::EndOutputAttribute();
        }
    }
    if (node->components & COMPONENT_TYPE_MESH)
    {
        if (node->ins & COMPONENT_TYPE_MESH)
        {

            int id = get_node_attr_id(ctx, node, COMPONENT_TYPE_MESH, true);
            ImNodes::BeginInputAttribute(id);
            ImGui::Text("Input Mesh");
            ImNodes::EndInputAttribute();
        }
        if (node->outs & COMPONENT_TYPE_MESH)
        {
            int id = get_node_attr_id(ctx, node, COMPONENT_TYPE_MESH, false);
            ImNodes::BeginOutputAttribute(id);
            ImGui::Text("Output Mesh");
            ImNodes::EndOutputAttribute();
        }
        assert((node->editables & COMPONENT_TYPE_MESH)); // Meshes can never be editable
    }
    if (node->components & COMPONENT_TYPE_VEC3)
    {
        if (node->ins & COMPONENT_TYPE_VEC3)
        {

            int id = get_node_attr_id(ctx, node, COMPONENT_TYPE_VEC3, true);
            ImNodes::BeginInputAttribute(id);
            ImGui::Text("Input Vec3");
            ImNodes::EndInputAttribute();
        }
        if (node->outs & COMPONENT_TYPE_VEC3)
        {
            int id = get_node_attr_id(ctx, node, COMPONENT_TYPE_VEC3, false);
            ImNodes::BeginOutputAttribute(id);
            ImGui::Text("Output Vec3");
            ImNodes::EndOutputAttribute();
        }
        if (node->editables & COMPONENT_TYPE_VEC3)
        {
            ImGui::PushItemWidth(300.0f);
            ImGui::InputFloat3("Vec3 Value", &vec3_data[node->id].value.x);
            ImGui::PopItemWidth();
        }
    }
    ImNodes::EndNode();
}

/*------------------------------ Editor Drawing ------------------------------*/
void draw_node_editor(graph_context *ctx)
{
    ImNodes::BeginNodeEditor();

    // Draw all nodes
    for (auto &node : ctx->nodes)
    {
        draw_generic_node(ctx, &node);

        if ((node.components & COMPONENT_TYPE_MESH) && (node.components & COMPONENT_TYPE_TRANSFORM))
        {
            mesh_data[node.id].render_data->model_matrix =
                get_model_matrix(transform_data[node.id].position,
                                 transform_data[node.id].rotation,
                                 transform_data[node.id].scale);
        }
        else if (node.components & COMPONENT_TYPE_MESH)
        {
            mesh_data[node.id].render_data->model_matrix = mat4_identity();
        }
    }

    // Draw all links
    for (auto &link : ctx->links)
    {
        ImNodes::Link(link.id, link.start_attr_id, link.end_attr_id);
    }

    // Handle context menu
    node_creation_request request = show_canvas_context_menu();

    ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_TopLeft);
    ImNodes::EndNodeEditor();

    // Handle zoom
    if (ImNodes::IsEditorHovered() && ImGui::GetIO().MouseWheel != 0)
    {
        float zoom = ImNodes::EditorContextGetZoom() + ImGui::GetIO().MouseWheel * 0.1f;
        ImNodes::EditorContextSetZoom(zoom, ImGui::GetMousePos());
    }

    if (request.components)
    {
        init_node(ctx, request.components, "New Node", request.ins, request.outs, request.editables);
    }
}

// TODO: Entity Component System
// TODO: An entity for each node type:

// typedef struct TransformComponent
// {
//     ImVec2 position;
//     // Other common transform data if needed.
// } TransformComponent;

// typedef struct MeshComponent
// {
//     Mesh *mesh;
//     v4 rotation;
//     v4 scale;
// } MeshComponent;

// typedef struct DisplayMeshComponent
// {
//     Mesh *mesh;
//     gl_mesh *render_data;
//     v4 position;
//     v4 rotation;
// } DisplayMeshComponent;

// typedef struct Vec3Component
// {
//     vec3 value;
// } Vec3Component;

// TODO: Separate functions for the drawing of nodes to the drawing of DisplayMeshComponents (for example)