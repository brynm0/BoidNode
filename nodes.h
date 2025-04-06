// nodes.h
#pragma once

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
#include "ImNodes.h"
#include <vector>

// Include your mesh type declaration
#include "io.h"
#include "types.h"

static u32 id_counter = 0; // Global counter for unique node ids
static u32 attr_counter = 0;

// A simple structure to store a mesh node’s data

struct mesh_node
{
    int id; // Unique node id
    gl_mesh *render_data;
    Mesh *mesh;    // Pointer to a mesh object
    v4 location;   // Translation values (x, y, z)
    v4 rotation;   // Rotation values (x, y, z)
    v4 scale;      // Scale values (x, y, z)
    char name[32]; // Name of the node (optional)
    int in_attr;
};

// Define a simple C-style struct for a vec3 node
struct vec3_node
{
    int id;         // Unique node id
    float value[3]; // The 3-component vector (x, y, z)
    int out_attr;   // Output attribute id (for linking)
};

// Structure to store link data between nodes.
struct NodeLink
{
    int id;         // Unique link id (ensure uniqueness in your application)
    int start_attr; // The starting attribute id (output)
    int start_node;
    int end_attr; // The ending attribute id (input)
    int end_node;
};
enum AttrType
{
    ATTR_TYPE_MESH,
    ATTR_TYPE_VEC3,
};

struct attrib
{
    AttrType type;
    int id;       // Unique attribute id
    void *memory; // Pointer to the data in memory
    u32 memory_len_bytes;
};

struct graph_context_data
{
    std::vector<mesh_node> mesh_nodes;
    std::vector<vec3_node> vec3_nodes;
    std::vector<NodeLink> links;
    std::vector<attrib> attrib_table;
    u32 link_id_counter;
};

graph_context_data g_nodeEditorContextData = {}; // Global context data for the node editor

int get_new_attrib(AttrType type, void *memory, u32 memory_len_bytes)
{
    attrib new_attrib = {};
    new_attrib.type = type;
    new_attrib.memory = memory;
    new_attrib.memory_len_bytes = memory_len_bytes;
    new_attrib.id = attr_counter++; // Assign a unique id to the new attribute

    g_nodeEditorContextData.attrib_table.push_back(new_attrib);
    return new_attrib.id; // Return the index of the new attribute
}

// // Global link counter for unique link ids.
// static int link_id_counter = 0;

// Function to create and initialize a new vec3_node
inline vec3_node get_vec3_node()
{
    vec3_node node = {};
    node.id = id_counter++; // Use the global id counter for a unique id.
    // Default values for the vector can be changed as needed.
    node.value[0] = 0.0f;
    node.value[1] = 0.0f;
    node.value[2] = 0.0f;
    // Set the initial grid position in the node editor.
    ImNodes::SetNodeGridSpacePos(node.id, ImVec2(0, 0));
    return node;
}

inline void register_new_vec3_node()
{
    vec3_node node = get_vec3_node();
    g_nodeEditorContextData.vec3_nodes.push_back(node);
    vec3_node *added = &g_nodeEditorContextData.vec3_nodes.back();

    added->out_attr = get_new_attrib(ATTR_TYPE_VEC3, &added->value, sizeof(added->value));
}

inline void draw_vec3_node(vec3_node *node)
{
    // v2 dispPos = g_nodeEditorZoom * node->original_pos;
    //  ImNodes::SetNodeGridSpacePos(node->id, ImVec2(dispPos.x, dispPos.y));

    ImNodes::BeginNode(node->id);

    ImNodes::BeginNodeTitleBar();
    ImGui::Text("Vec3");
    ImNodes::EndNodeTitleBar();

    // static int output_attr_id = get_new_attrib(ATTR_TYPE_VEC3, &node->value, sizeof(node->value));
    ImNodes::BeginOutputAttribute(node->out_attr);
    ImGui::Text("Output");
    ImNodes::EndOutputAttribute();

    // Set a custom width for the input widgets
    const float input_width = 120.0f; // Adjust this value as needed
    ImGui::PushItemWidth(input_width);
    ImGui::InputFloat("X", &node->value[0]);
    ImGui::InputFloat("Y", &node->value[1]);
    ImGui::InputFloat("Z", &node->value[2]);
    ImGui::PopItemWidth();

    ImNodes::EndNode();

    // ImVec2 currentPos = ImNodes::GetNodeGridSpacePos(node->id);
    // node->original_pos = {currentPos.x / g_nodeEditorZoom, currentPos.y / g_nodeEditorZoom};
}

mesh_node get_mesh_node(Mesh *mesh, const char *name)
{
    mesh_node node = {};
    node.id = id_counter++;
    node.mesh = mesh;
    node.location = {0, 0, 0, 0};
    node.rotation = {0, 0, 0, 0};
    node.scale = {1, 1, 1, 1};
    strcpy(node.name, name); // Copy the name into the node structure
    ImNodes::SetNodeGridSpacePos(node.id, {0, 0});

    return node;
}

static void register_new_mesh_node(Mesh *mesh, const char *name)
{
    mesh_node node = get_mesh_node(mesh, name);

    node.render_data = gl_render_add_mesh(mesh);
    // Store the new node in the global context data.
    g_nodeEditorContextData.mesh_nodes.push_back(node);
    mesh_node *added = &g_nodeEditorContextData.mesh_nodes.back();

    added->in_attr = get_new_attrib(ATTR_TYPE_VEC3, &added->location, sizeof(added->location));

    // You can also add it to a global vector or any other structure as needed.
}

// ---------------------------------------------------------------------------
// Modified draw functions for nodes that apply the zoom transformation.
// These functions set the node grid position using the stored original_pos and current zoom.
inline void draw_mesh_node(mesh_node *node)
{
    // Update the displayed position: original_pos scaled by current zoom.
    // v2 dispPos = node->original_pos * g_nodeEditorZoom;
    // ImNodes::SetNodeGridSpacePos(node->id, ImVec2(dispPos.x, dispPos.y));
    // Set the global scaling factor
    // ImGui::GetIO().FontGlobalScale = g_nodeEditorZoom;
    ImNodes::BeginNode(node->id);

    ImNodes::BeginNodeTitleBar();
    ImGui::Text("Mesh");
    ImNodes::EndNodeTitleBar();

    // Input attribute for vec3 connection: (node id * 10 + 2)
    // static int vec3_input_attr_id = get_new_attrib(ATTR_TYPE_VEC3, &node->location, sizeof(node->location));
    ImNodes::BeginInputAttribute(node->in_attr);
    ImGui::Text("Location Input");
    ImNodes::EndInputAttribute();

    const float input_width = 30.0f; // Adjust this value as needed
    ImGui::PushItemWidth(input_width);
    // Draw transform editors.
    ImGui::InputFloat3("Location", &node->location.x);
    ImGui::InputFloat3("Rotation", &node->rotation.x);
    ImGui::InputFloat3("Scale", &node->scale.x);
    ImGui::PopItemWidth();

    // Output attribute for mesh-to-mesh linking: (node id * 10 + 3)
    int mesh_output_attr_id = node->id * 10 + 3;
    ImNodes::BeginOutputAttribute(mesh_output_attr_id);
    ImGui::Text("Mesh Output");
    ImNodes::EndOutputAttribute();

    if (node->mesh)
    {
        ImGui::Text("Mesh: %s", node->name);
    }

    ImNodes::EndNode();

    // After drawing, update the node's original_pos in case it was moved by the user.
    // ImVec2 currentPos = ImNodes::GetNodeGridSpacePos(node->id);
    // node->original_pos = {currentPos.x / g_nodeEditorZoom, currentPos.y / g_nodeEditorZoom};
}

// Search for a mesh_node by id in a given vector. Returns nullptr if not found.
inline mesh_node *getMeshNodeById(std::vector<mesh_node> &nodes, int id)
{
    for (int i = 0; i < nodes.size(); i++)
    {
        mesh_node *node = &nodes[i];
        if (node->id == id)
        {
            return node;
        }
    }
    return nullptr; // Not found: error handling in caller.
}

// Search for a vec3_node by id in a given vector. Returns nullptr if not found.
inline vec3_node *getVec3NodeById(std::vector<vec3_node> &nodes, int id)
{
    for (int i = 0; i < nodes.size(); i++)
    {
        vec3_node *node = &nodes[i];
        if (node->id == id)
        {
            return node;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Example: Process New Link and Update Links Vector
// ---------------------------------------------------------------------------

// This function demonstrates how you might update your links vector when a new link is created.
// It checks if a new link was created and then updates the target node’s data.
// Additionally, it adds the new link to the links vector for future drawing.
inline void process_and_store_new_link()
{
    int start_attr = 0, end_attr = 0;
    int node_start = 0, node_end = 0;
    // Check if a new link was just created.
    if (ImNodes::IsLinkCreated(
            &node_start,
            &start_attr,
            &node_end,
            &end_attr))
    {
        attrib start_attr_data = g_nodeEditorContextData.attrib_table[start_attr];
        attrib end_attr_data = g_nodeEditorContextData.attrib_table[end_attr];
        // Case 1: vec3_node (output) -> mesh_node (input) connection.
        if (start_attr_data.type == ATTR_TYPE_VEC3 && end_attr_data.type == ATTR_TYPE_VEC3)
        {
            memcpy(end_attr_data.memory, start_attr_data.memory, start_attr_data.memory_len_bytes);
        }
        // Case 2: mesh_node (output) -> mesh_node (input) connection.
        else if (start_attr_data.type == ATTR_TYPE_MESH && end_attr_data.type == ATTR_TYPE_MESH)
        {
            int source_node_id = start_attr / 10;
            int target_node_id = end_attr / 10;

            mesh_node *source = getMeshNodeById(g_nodeEditorContextData.mesh_nodes, source_node_id);
            mesh_node *target = getMeshNodeById(g_nodeEditorContextData.mesh_nodes, target_node_id);

            if (source && target)
            {
                // Copy the mesh pointer from the source node to the target node.
                target->mesh = source->mesh;
            }
        }
        // You can add additional connection types or error handling as needed.

        // Store the new link in the links vector for persistent drawing.
        NodeLink newLink;
        newLink.id = g_nodeEditorContextData.link_id_counter++; // Ensure each link has a unique id.
        newLink.start_attr = start_attr;
        newLink.end_attr = end_attr;
        newLink.end_node = node_end;
        newLink.start_node = node_start;
        g_nodeEditorContextData.links.push_back(newLink);
    }
}

// Update all existing links by transferring data from the upstream node to the target node.
// This function should be called before processing new link creations.
inline void update_links()
{
    // Iterate over each existing link.
    for (const auto &link : g_nodeEditorContextData.links)
    {
        // Our attribute id scheme:
        // For a node with id 'n':
        //   vec3 node output attribute = n * 10 + 1
        //   mesh node input attribute  = n * 10 + 2
        //   mesh node output attribute = n * 10 + 3

        attrib start_attr_data = g_nodeEditorContextData.attrib_table[link.start_attr];
        attrib end_attr_data = g_nodeEditorContextData.attrib_table[link.end_attr];
        // Case 1: vec3_node (output) -> mesh_node (input) connection.
        if (start_attr_data.type == ATTR_TYPE_VEC3 && end_attr_data.type == ATTR_TYPE_VEC3)
        {
            memcpy(end_attr_data.memory, start_attr_data.memory, start_attr_data.memory_len_bytes);
        }
        // Case 2: mesh_node (output) -> mesh_node (input) connection.
        else if (start_attr_data.type == ATTR_TYPE_MESH && end_attr_data.type == ATTR_TYPE_MESH)
        {
            int source_node_id = link.start_attr / 10;
            int target_node_id = link.end_attr / 10;

            mesh_node *source = getMeshNodeById(g_nodeEditorContextData.mesh_nodes, source_node_id);
            mesh_node *target = getMeshNodeById(g_nodeEditorContextData.mesh_nodes, target_node_id);

            if (source && target)
            {
                // Copy the mesh pointer from the source node to the target node.
                target->mesh = source->mesh;
            }
        }
        // If the link does not match known patterns, we simply ignore it.
        // In a more complex application, you may want to log an error or handle it differently.
    }
}

static graph_context_data *init_im_nodes()
{
    id_counter = 0; // Reset the global id counter
    ImNodes::CreateContext();
    g_nodeEditorContextData = {};
    g_nodeEditorContextData.mesh_nodes.reserve(10);   // Reserve space for 10 mesh nodes
    g_nodeEditorContextData.vec3_nodes.reserve(10);   // Reserve space for 10 vec3 nodes
    g_nodeEditorContextData.links.reserve(10);        // Reserve space for 10 links
    g_nodeEditorContextData.attrib_table.reserve(10); // Reserve space for 10 attributes
    return &g_nodeEditorContextData;
}

struct nodes_to_create
{
    bool mesh_node;
    bool vec3_node;
    ImVec2 position;
};

nodes_to_create ShowCanvasContextMenu()
{

    nodes_to_create to_create = {};
    static ImVec2 context_menu_pos; // Stores the grid position where the context menu was opened

    // Check if the editor is hovered and right-clicked
    if (ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        ImGui::OpenPopup("CanvasContextMenu");
        // Capture the mouse position at the time of right-click and convert to grid space
        ImVec2 mouse_pos = ImGui::GetMousePos();
        context_menu_pos = mouse_pos; //
    }

    // Display the popup menu
    if (ImGui::BeginPopup("CanvasContextMenu"))
    {
        ImGui::Selectable("Mesh Node", (bool *)&to_create.mesh_node);
        ImGui::Selectable("Vec3 Node", (bool *)&to_create.vec3_node);
        if (to_create.mesh_node || to_create.vec3_node)
        {
            printf("Create new node\n");
        }

        to_create.position = context_menu_pos;

        ImGui::EndPopup();
    }
    return to_create;
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

// ---------------------------------------------------------------------------
// Extended draw_node_editor that draws all nodes and links.
inline void draw_node_editor(std::vector<mesh_node> &meshNodes,
                             std::vector<vec3_node> &vec3Nodes,
                             std::vector<NodeLink> &links)
{
    // Begin the node editor context.
    ImNodes::BeginNodeEditor();

    for (int i = 0; i < meshNodes.size(); i++)
    {

        mesh_node *node = &meshNodes[i];
        vec3 pos = {node->location.x, node->location.y, node->location.z};
        node->render_data->model_matrix = get_model_matrix(pos);
        draw_mesh_node(node);
    }

    for (int i = 0; i < vec3Nodes.size(); i++)
    {
        vec3_node *node = &vec3Nodes[i];
        draw_vec3_node(node);
    }
    for (int link_index = 0; link_index < links.size(); link_index++)
    {
        NodeLink &link = links[link_index];
        ImNodes::Link(link.id, link.start_attr, link.end_attr);
    }

    // static char name[32] = "Label1";
    // char buf[64];
    // sprintf(buf, "Button: %s###Button", name); // ### operator override ID ignoring the preceding label
    // if (ImGui::BeginPopupContextItem())
    // {
    //     ImGui::Text("Edit name:");
    //     ImGui::InputText("##edit", name, IM_ARRAYSIZE(name));
    //     if (ImGui::Button("Close"))
    //         ImGui::CloseCurrentPopup();
    //     ImGui::EndPopup();
    // }

    nodes_to_create to_create = ShowCanvasContextMenu();

    ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_TopLeft, NULL, NULL);
    ImNodes::EndNodeEditor();

    if (ImNodes::IsEditorHovered() && ImGui::GetIO().MouseWheel != 0)
    {
        float zoom = ImNodes::EditorContextGetZoom() + ImGui::GetIO().MouseWheel * 0.1f;
        ImNodes::EditorContextSetZoom(zoom, ImGui::GetMousePos());
    }
    if (to_create.mesh_node)
    {
        register_new_mesh_node(nullptr, "name"); // Uses global 'name' variable
        if (!g_nodeEditorContextData.mesh_nodes.empty())
        {
            mesh_node &newNode = g_nodeEditorContextData.mesh_nodes.back();
            ImNodes::SetNodeScreenSpacePos(newNode.id, to_create.position);
        }
    }
    if (to_create.vec3_node)
    {
        register_new_vec3_node();
        if (!g_nodeEditorContextData.vec3_nodes.empty())
        {
            vec3_node &newNode = g_nodeEditorContextData.vec3_nodes.back();
            ImNodes::SetNodeScreenSpacePos(newNode.id, to_create.position);
        }
    }
}