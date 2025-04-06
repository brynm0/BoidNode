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

/*------------------------------ Core Types ------------------------------*/
typedef enum NodeType
{
    NODE_TYPE_MESH,
    NODE_TYPE_VEC3,
    NODE_TYPE_CUSTOM // Add new types here
} NodeType;

typedef enum AttrType
{
    ATTR_TYPE_MESH,
    ATTR_TYPE_VEC3,
    ATTR_TYPE_FLOAT,
    ATTR_TYPE_CUSTOM
} AttrType;

typedef struct Attribute
{
    int id;           // Unique attribute ID
    AttrType type;    // Data type
    void *data;       // Pointer to associated data
    size_t data_size; // Size of data buffer
    bool is_input;    // Input/output designation
} Attribute;

typedef struct GenericNode
{
    int id;                         // Unique node ID
    NodeType type;                  // Node type identifier
    char name[32];                  // Display name
    std::vector<int> attribute_ids; // I/O attributes
    void *node_data;                // Type-specific data
    ImVec2 position;                // Node position
} GenericNode;

typedef struct NodeLink
{
    int id;
    int start_attr_id; // Output attribute
    int end_attr_id;   // Input attribute
} NodeLink;

/*------------------------------ Context Data ------------------------------*/
typedef struct graph_context
{
    std::vector<GenericNode> nodes;
    std::vector<NodeLink> links;
    std::unordered_map<int, Attribute> attr_lookup; // Fast attribute access
    u32 next_node_id = 0;
    u32 next_attr_id = 0;
    u32 next_link_id = 0;
} graph_context;

/*------------------------------ Core Functions ------------------------------*/
// Initialize new attribute and add to lookup
int create_attribute(graph_context *ctx, AttrType type, void *data, size_t size, bool is_input)
{
    Attribute attr;
    attr.id = ctx->next_attr_id++;
    attr.type = type;
    attr.data = data;
    attr.data_size = size;
    attr.is_input = is_input;

    ctx->attr_lookup[attr.id] = attr;
    return attr.id; // &ctx->attr_lookup[attr.id];
}

// Generic node creation template
GenericNode *create_node(graph_context *ctx, NodeType type, const char *name, void *node_data)
{
    GenericNode node;
    node.id = ctx->next_node_id++;
    node.type = type;
    node.node_data = node_data;
    strncpy(node.name, name, sizeof(node.name));

    ctx->nodes.push_back(node);
    return &ctx->nodes.back();
}

/*------------------------------ Type-Specific Initializers ------------------------------*/
// Vec3 node initialization
GenericNode *init_vec3_node(graph_context *ctx, vec3 initial_pos)
{
    GenericNode *node = create_node(ctx, NODE_TYPE_VEC3, "Vec3 Node", nullptr);
    // Allocate memory for node-specific data
    vec3 *data = (vec3 *)malloc(sizeof(vec3));
    *data = initial_pos;

    node->node_data = data;

    // Create output attribute
    int out_attr = create_attribute(ctx, ATTR_TYPE_VEC3, data, sizeof(vec3), false);
    node->attribute_ids.push_back(out_attr);
    return node;
}

struct mesh_node_data
{
    Mesh *mesh;
    gl_mesh *render_data;
    v4 position;
    v4 rotation;
    v4 scale;
};

// Mesh node initialization
void init_mesh_node(graph_context *ctx, Mesh *mesh, const char *name)
{
    GenericNode *node = create_node(ctx, NODE_TYPE_MESH, name, nullptr);
    node->node_data = (mesh_node_data *)malloc(sizeof(mesh_node_data));

    mesh_node_data *data = (mesh_node_data *)node->node_data;
    if (mesh)
    {
        data->render_data = gl_render_add_mesh(mesh);
    }

    // Create input attribute
    int in_attr = create_attribute(ctx, ATTR_TYPE_VEC3, &data->position, sizeof(data->position), true);
    int out_attr = create_attribute(ctx, ATTR_TYPE_MESH, &data->mesh, sizeof(data->mesh), false);
    node->attribute_ids.push_back(in_attr);
    node->attribute_ids.push_back(out_attr);
}

/*------------------------------ Link Handling ------------------------------*/
bool create_link(graph_context *ctx, int output_attr, int input_attr)
{
    Attribute *dst = &ctx->attr_lookup[input_attr];
    Attribute *src = &ctx->attr_lookup[output_attr];

    // Type compatibility check
    if (!src || !dst || src->type != dst->type || src->is_input || !dst->is_input)
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

    // Initial data transfer
    memcpy(dst->data, src->data, src->data_size);
    return true;
}

/*------------------------------ Update System ------------------------------*/
void update_links(graph_context *ctx)
{
    for (auto &link : ctx->links)
    {
        Attribute *src = &ctx->attr_lookup[link.start_attr_id];
        Attribute *dst = &ctx->attr_lookup[link.end_attr_id];

        if (src && dst)
        {
            memcpy(dst->data, src->data, src->data_size);
        }
    }
}

/*------------------------------ Helper Functions ------------------------------*/
// Example implementation of get_node_by_id
GenericNode *get_node_by_id(graph_context *ctx, int node_id)
{
    for (auto &node : ctx->nodes)
    {
        if (node.id == node_id)
            return &node;
    }
    return nullptr;
}

/*------------------------------ Link Processing ------------------------------*/
// Unified function to handle both new link creation and data updates
inline void process_and_store_new_links(graph_context *ctx)
{
    // First update existing connections
    update_links(ctx);

    // Then process new links
    int start_node_id, start_attr_id, end_node_id, end_attr_id;
    if (ImNodes::IsLinkCreated(
            &start_node_id,
            &start_attr_id,
            &end_node_id,
            &end_attr_id))
    {
        // Get actual attribute IDs from node/attribute indices
        GenericNode *start_node = get_node_by_id(ctx, start_node_id);
        GenericNode *end_node = get_node_by_id(ctx, end_node_id);

        if (!start_node || !end_node)
        {
            fprintf(stderr, "Invalid node connection attempt");
        }

        // Attempt to create validated link
        if (!create_link(ctx, start_attr_id, end_attr_id))
        {

            fprintf(stderr, "Failed to create link between attributes %d and %d",
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
    ctx.attr_lookup.reserve(50);

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
struct NodeCreationRequest
{
    NodeType type;
    ImVec2 position;
};

NodeCreationRequest ShowCanvasContextMenu()
{
    NodeCreationRequest request = {NODE_TYPE_CUSTOM, ImVec2(0, 0)};

    if (ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        ImGui::OpenPopup("CanvasContextMenu");
        request.position = ImGui::GetMousePos();
    }

    if (ImGui::BeginPopup("CanvasContextMenu"))
    {
        {
            if (ImGui::MenuItem("Mesh Node"))
                request.type = NODE_TYPE_MESH;
            request.position = ImGui::GetMousePos();
        }
        if (ImGui::MenuItem("Vec3 Node"))
        {
            request.type = NODE_TYPE_VEC3;
            request.position = ImGui::GetMousePos();
        }
        ImGui::EndPopup();
    }

    return request;
}

mat4 get_model_matrix(vec3 position)
{
    mat4 result;

    // Initialize to identity matrix
    memset(&result, 0, sizeof(mat4));
    result.m[0] = {1.0f, 0.0f, 0.0f, 0.0f};
    result.m[1] = {0.0f, 1.0f, 0.0f, 0.0f};
    result.m[2] = {0.0f, 0.0f, 1.0f, 0.0f};
    result.m[3] = {position.x, position.y, position.z, 1.0f};

    return result;
}

/*------------------------------ Node Drawing ------------------------------*/
void draw_generic_node(graph_context *ctx, GenericNode *node)
{
    ImNodes::BeginNode(node->id);

    // Title bar with type-specific styling
    ImNodes::BeginNodeTitleBar();
    ImGui::TextUnformatted(node->name);
    ImNodes::EndNodeTitleBar();

    // Draw attributes based on type
    switch (node->type)
    {
    case NODE_TYPE_MESH:
    {
        mesh_node_data *data = static_cast<mesh_node_data *>(node->node_data);

        // Input attributes
        for (auto &attr_id : node->attribute_ids)
        {
            Attribute *attr = &ctx->attr_lookup[attr_id];
            if (attr->is_input)
            {
                ImNodes::BeginInputAttribute(attr->id);
                ImGui::Text("Input");
                ImNodes::EndInputAttribute();
            }
            else
            {
                ImNodes::BeginOutputAttribute(attr->id);
                ImGui::Text("Output");
                ImNodes::EndOutputAttribute();
            }
        }

        vec3 pos = {data->position.x, data->position.y, data->position.z};
        data->render_data->model_matrix = get_model_matrix(pos);

        // Transformation controls
        ImGui::PushItemWidth(100.0f);
        ImGui::InputFloat3("Location", &data->position.x);
        ImGui::InputFloat3("Rotation", &data->rotation.x);
        ImGui::InputFloat3("Scale", &data->scale.x);
        ImGui::PopItemWidth();

        break;
    }

    case NODE_TYPE_VEC3:
    {
        vec3 *values = static_cast<vec3 *>(node->node_data);

        for (int i = 0; i < node->attribute_ids.size(); i++)
        {
            Attribute *attr = &ctx->attr_lookup[node->attribute_ids[i]];
            if (attr->is_input)
            {
                ImNodes::BeginInputAttribute(attr->id);
                ImGui::Text("Input");
                ImNodes::EndInputAttribute();
            }
            else
            {
                ImNodes::BeginOutputAttribute(attr->id);
                ImGui::Text("Output");
                ImNodes::EndOutputAttribute();
            }
        }

        // Output attribute first
        // ImNodes::BeginOutputAttribute(node->attributes[0].id);
        // ImGui::Text("Output");
        // ImNodes::EndOutputAttribute();

        // Value controls
        ImGui::PushItemWidth(120.0f);
        ImGui::InputFloat("X", &values->x);
        ImGui::InputFloat("Y", &values->y);
        ImGui::InputFloat("Z", &values->z);
        ImGui::PopItemWidth();
        break;
    }

    default:
        ImGui::Text("Unknown Node Type");
        break;
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
    }

    // Draw all links
    for (auto &link : ctx->links)
    {
        ImNodes::Link(link.id, link.start_attr_id, link.end_attr_id);
    }

    // Handle context menu
    NodeCreationRequest request = ShowCanvasContextMenu();

    ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_TopLeft);
    ImNodes::EndNodeEditor();

    // Handle zoom
    if (ImNodes::IsEditorHovered() && ImGui::GetIO().MouseWheel != 0)
    {
        float zoom = ImNodes::EditorContextGetZoom() + ImGui::GetIO().MouseWheel * 0.1f;
        ImNodes::EditorContextSetZoom(zoom, ImGui::GetMousePos());
    }

    if (request.type != NODE_TYPE_CUSTOM)
    {
        // GenericNode *newNode = create_node(ctx, request.type, "", nullptr);
        // newNode->position = request.position;

        // Initialize type-specific data
        switch (request.type)
        {
        case NODE_TYPE_MESH:
        {
            init_mesh_node(ctx, nullptr, "Mesh Node"); // Replace with actual mesh data
            break;
        }

        case NODE_TYPE_VEC3:
        {
            GenericNode *new_node = init_vec3_node(ctx, {0.0f, 0.0f, 0.0f}); // Default position
            ImNodes::SetNodeScreenSpacePos(new_node->id,

                                           request.position);

            break;
        }
        }
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