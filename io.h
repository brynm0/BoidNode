
#pragma once
#include <cstdio>  // For file operations
#include <cstdint> // For uint8_t, uint32_t
#include <stdlib.h>
#include "string.h"
#include <vector>
#include <sstream>

#pragma pack(push, 1) // Ensure tight packing of the struct
typedef struct vertex
{
    vec4 position; // 3D position of the vertex
    vec4 normal;   // Normal vector for lighting calculations
    vec4 texcoord; // Texture coordinates for texture mapping
} vertex;
#pragma pack(pop) // Restore default packing

// Mesh structure definition using a 4D vector for positions.
// The vertices array will contain 4 floats per vertex: x, y, z, w.
typedef struct Mesh
{
    vertex *vertices;         // Pointer to an array of vertex positions (4 floats per vertex)
    unsigned int vertexCount; // Number of vertices in the mesh

    unsigned int *indices;   // Optional pointer to index data (32-bit indices)
    unsigned int indexCount; // Number of indices in the mesh
} Mesh;
/**
 * Reads an entire file into a dynamically allocated buffer.
 *
 * @param path The path to the file.
 * @param return_size Pointer to a uint32_t where the file size will be stored.
 * @return A pointer to the allocated buffer containing the file content, or nullptr on failure.
 *         The caller is responsible for freeing the allocated memory.
 */
uint32_t *read_file(const char *path, uint32_t *return_size)
{
    if (!path || !return_size)
    {
        return NULL; // Invalid arguments
    }

    FILE *file = fopen(path, "rb"); // Open file in binary mode
    if (!file)
    {
        return NULL; // Failed to open file
    }

    // Seek to the end to determine file size
    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return NULL;
    }

    long file_size = ftell(file); // Get file size
    if (file_size < 0)
    { // Handle ftell failure
        fclose(file);
        return NULL;
    }

    *return_size = (uint32_t)file_size + 1; // Store file size

    rewind(file); // Reset file position to the beginning

    // Allocate memory to hold the file content
    uint8_t *buffer = (uint8_t *)malloc(*return_size);

    if (!buffer)
    {
        fclose(file);
        return NULL; // Memory allocation failed
    }

    memset(buffer, 0, *return_size); // Initialize buffer to zero

    // Read file content into buffer
    size_t read_bytes = fread(buffer, 1, *return_size, file);
    fclose(file); // Close file

    // Ensure entire file was read
    if (read_bytes + 1 != *return_size)
    {
        free(buffer);
        return NULL;
    }

    return (uint32_t *)buffer; // Return pointer to the file content
}

void normalize_line_endings(char *content)
{
    for (char *p = content; *p; ++p)
    {
        if (*p == '\r')
        {
            *p = '\n';
        }
    }
}

// Helper function to compare two vertex structures for equality.
// Returns true if all components (position, normal, texcoord) are equal.
bool vertex_equal(const vertex *a, const vertex *b)
{
    // Compare position components.
    if (a->position.x != b->position.x ||
        a->position.y != b->position.y ||
        a->position.z != b->position.z ||
        a->position.w != b->position.w)
        return false;

    // Compare normal components.
    if (a->normal.x != b->normal.x ||
        a->normal.y != b->normal.y ||
        a->normal.z != b->normal.z ||
        a->normal.w != b->normal.w)
        return false;

    // Compare texture coordinate components.
    if (a->texcoord.x != b->texcoord.x ||
        a->texcoord.y != b->texcoord.y ||
        a->texcoord.z != b->texcoord.z ||
        a->texcoord.w != b->texcoord.w)
        return false;

    return true;
}

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include "io.h" // Contains definitions for vertex, Mesh, read_file, normalize_line_endings

void calculate_mesh_normals(Mesh *m)
{
    if (!m || !m->vertices || !m->indices || m->indexCount % 3 != 0)
    {
        fprintf(stderr, "Error: Invalid mesh or indices\n");
        return;
    }

    // Initialize all normals to zero
    for (unsigned int i = 0; i < m->vertexCount; ++i)
    {
        m->vertices[i].normal.x = 0.0f;
        m->vertices[i].normal.y = 0.0f;
        m->vertices[i].normal.z = 0.0f;
        m->vertices[i].normal.w = 0.0f;
    }

    // Process each triangle to compute and accumulate face normals
    for (unsigned int i = 0; i < m->indexCount; i += 3)
    {
        unsigned int i0 = m->indices[i];
        unsigned int i1 = m->indices[i + 1];
        unsigned int i2 = m->indices[i + 2];

        // Check for valid indices
        if (i0 >= m->vertexCount || i1 >= m->vertexCount || i2 >= m->vertexCount)
        {
            fprintf(stderr, "Error: Invalid index in triangle\n");
            continue;
        }

        vec4 v0 = m->vertices[i0].position;
        vec4 v1 = m->vertices[i1].position;
        vec4 v2 = m->vertices[i2].position;

        // Calculate edges
        vec4 edge1 = {v1.x - v0.x, v1.y - v0.y, v1.z - v0.z, 0.0f};
        vec4 edge2 = {v2.x - v0.x, v2.y - v0.y, v2.z - v0.z, 0.0f};

        // Compute cross product (face normal)
        float cx = edge1.y * edge2.z - edge1.z * edge2.y;
        float cy = edge1.z * edge2.x - edge1.x * edge2.z;
        float cz = edge1.x * edge2.y - edge1.y * edge2.x;

        // Normalize the face normal
        float length = sqrt(cx * cx + cy * cy + cz * cz);
        if (length > 0.0f)
        {
            cx /= length;
            cy /= length;
            cz /= length;
        }
        else
        {
            continue; // Skip degenerate triangle
        }

        // Accumulate the normal to each vertex
        m->vertices[i0].normal.x += cx;
        m->vertices[i0].normal.y += cy;
        m->vertices[i0].normal.z += cz;

        m->vertices[i1].normal.x += cx;
        m->vertices[i1].normal.y += cy;
        m->vertices[i1].normal.z += cz;

        m->vertices[i2].normal.x += cx;
        m->vertices[i2].normal.y += cy;
        m->vertices[i2].normal.z += cz;
    }

    // Normalize the accumulated normals for each vertex
    for (unsigned int i = 0; i < m->vertexCount; ++i)
    {
        float nx = m->vertices[i].normal.x;
        float ny = m->vertices[i].normal.y;
        float nz = m->vertices[i].normal.z;
        float len = sqrt(nx * nx + ny * ny + nz * nz);

        if (len > 0.0f)
        {
            m->vertices[i].normal.x = nx / len;
            m->vertices[i].normal.y = ny / len;
            m->vertices[i].normal.z = nz / len;
        }
        else
        {
            // Set to default normal (e.g., up vector) if zero length
            m->vertices[i].normal.x = 0.0f;
            m->vertices[i].normal.y = 0.0f;
            m->vertices[i].normal.z = 0.0f;
        }
        m->vertices[i].normal.w = 0.0f;
    }
}

/**
 * Reads an .obj file and parses it into a Mesh structure.
 * This version loads positions, texture coordinates, and normals,
 * interleaving them into an array of vertex structures.
 *
 * Key points:
 * - Uses only C style file I/O, but leverages std::string and std::istringstream
 *   for robust string tokenization (instead of strtok).
 * - Loads "v " (positions), "vt" (texture coordinates), and "vn" (normals).
 * - Faces ("f ") are triangulated via a triangle fan.
 * - Each face vertex is processed to build a unique vertex (by checking for duplicates).
 * - Error handling and bounds checking are provided.
 *
 * @param path The path to the OBJ file.
 * @return A Mesh structure with allocated vertex and index arrays.
 */
/**
 * Reads an .obj file and parses it into a Mesh structure.
 * This version loads positions, texture coordinates, and normals,
 * interleaving them into an array of vertex structures.
 *
 * Key points:
 * - Uses std::string and std::istringstream for robust tokenization.
 * - Loads "v " (positions), "vt" (texture coordinates), and "vn" (normals).
 * - Faces ("f ") are triangulated via a triangle fan.
 * - Each face vertex is processed to build a unique vertex (by checking for duplicates).
 * - Error handling and bounds checking are provided.
 *
 * @param path The path to the OBJ file.
 * @return A Mesh structure with allocated vertex and index arrays.
 */
Mesh read_mesh(const char *path)
{
    // Initialize mesh with null pointers and zero counts.
    Mesh mesh = {nullptr, 0, nullptr, 0};

    // Read the file content into a dynamically allocated buffer.
    uint32_t file_size = 0;
    uint32_t *file_buffer = read_file(path, &file_size);
    if (!file_buffer || file_size == 0)
    {
        fprintf(stderr, "Error: Failed to read file or file is empty: %s\n", path);
        return mesh;
    }

    // Cast the file buffer to a char pointer for string processing.
    char *file_content = reinterpret_cast<char *>(file_buffer);
    // Normalize line endings to '\n' to simplify tokenization.
    normalize_line_endings(file_content);

    // Construct an std::string from the file content.
    std::string fileStr(file_content);

    // Temporary storage for positions, texture coordinates, and normals.
    std::vector<vec4> positions;
    std::vector<vec4> texcoords;
    std::vector<vec4> normals;

    // Temporary storage for the final interleaved vertex data and index list.
    std::vector<vertex> final_vertices;
    std::vector<unsigned int> final_indices;

    bool has_some_normals = false;

    // Use std::istringstream to iterate over lines.
    std::istringstream fileStream(fileStr);
    std::string line;
    while (std::getline(fileStream, line))
    {
        // Skip empty lines or comments.
        if (line.empty() || line[0] == '#')
            continue;

        // Process vertex position lines ("v ").
        if (line.rfind("v ", 0) == 0)
        {
            float x, y, z, w = 1.0f; // Default w component to 1.
            std::istringstream ss(line.substr(2));
            if (!(ss >> x >> y >> z))
            {
                fprintf(stderr, "Error: Invalid vertex format: %s\n", line.c_str());
                continue;
            }
            ss >> w; // If w is not provided, it remains 1.
            vec4 pos = {x, y, z, w};
            positions.push_back(pos);
        }
        // Process texture coordinate lines ("vt").
        else if (line.rfind("vt", 0) == 0)
        {
            float u, v, w_val = 0.0f; // Optional third coordinate.
            std::istringstream ss(line.substr(2));
            if (!(ss >> u >> v))
            {
                fprintf(stderr, "Error: Invalid texture coordinate format: %s\n", line.c_str());
                continue;
            }
            ss >> w_val;
            vec4 tex = {u, v, w_val, 0.0f};
            texcoords.push_back(tex);
        }
        // Process normal lines ("vn").
        else if (line.rfind("vn", 0) == 0)
        {
            has_some_normals = true;
            float nx, ny, nz;
            std::istringstream ss(line.substr(2));
            if (!(ss >> nx >> ny >> nz))
            {
                fprintf(stderr, "Error: Invalid normal format: %s\n", line.c_str());
                continue;
            }
            vec4 norm = {nx, ny, nz, 0.0f};
            normals.push_back(norm);
        }
        // Process face lines ("f ").
        else if (line.rfind("f ", 0) == 0)
        {
            // Temporary vectors for indices for each face.
            std::vector<int> face_v;
            std::vector<int> face_vt;
            std::vector<int> face_vn;

            // Create a stream to tokenize the face line (skip "f ").
            std::istringstream ss(line.substr(2));
            std::string token;
            while (ss >> token)
            {
                int v_index = 0, vt_index = -1, vn_index = -1;
                size_t pos1 = token.find('/');
                if (pos1 != std::string::npos)
                {
                    // Vertex index.
                    v_index = std::atoi(token.substr(0, pos1).c_str());
                    size_t pos2 = token.find('/', pos1 + 1);
                    if (pos2 != std::string::npos)
                    {
                        std::string vt_str = token.substr(pos1 + 1, pos2 - pos1 - 1);
                        if (!vt_str.empty())
                            vt_index = std::atoi(vt_str.c_str());
                        std::string vn_str = token.substr(pos2 + 1);
                        if (!vn_str.empty())
                            vn_index = std::atoi(vn_str.c_str());
                    }
                    else
                    {
                        std::string vt_str = token.substr(pos1 + 1);
                        if (!vt_str.empty())
                            vt_index = std::atoi(vt_str.c_str());
                    }
                }
                else
                {
                    v_index = std::atoi(token.c_str());
                }

                if (v_index <= 0)
                {
                    fprintf(stderr, "Error: Invalid face vertex index in token: %s\n", token.c_str());
                    continue;
                }
                // OBJ indices are 1-based; convert to 0-based.
                face_v.push_back(v_index - 1);
                face_vt.push_back(vt_index > 0 ? vt_index - 1 : -1);
                face_vn.push_back(vn_index > 0 ? vn_index - 1 : -1);
            }

            if (face_v.size() < 3)
            {
                fprintf(stderr, "Error: Face with less than 3 vertices.\n");
                continue;
            }
            // Triangulate the face using a triangle fan.
            for (size_t i = 1; i + 1 < face_v.size(); i++)
            {
                for (int j = 0; j < 3; j++)
                {
                    int idx = (j == 0) ? 0 : (j == 1 ? i : i + 1);
                    vertex vtx;
                    // Set position.
                    if ((unsigned)face_v[idx] >= positions.size())
                    {
                        fprintf(stderr, "Error: Position index out of bounds in face.\n");
                        continue;
                    }
                    vtx.position = positions[face_v[idx]];

                    // Set normal (second field in the vertex struct).
                    if (face_vn[idx] >= 0 && (unsigned)face_vn[idx] < normals.size())
                        vtx.normal = normals[face_vn[idx]];
                    else
                        vtx.normal = {0, 0, 0, 0};

                    // Set texcoord (third field in the vertex struct).
                    if (face_vt[idx] >= 0 && (unsigned)face_vt[idx] < texcoords.size())
                        vtx.texcoord = texcoords[face_vt[idx]];
                    else
                        vtx.texcoord = {0, 0, 0, 0};

                    // Deduplicate: check if an identical vertex exists.
                    unsigned int final_index = 0;
                    bool found = false;
                    for (unsigned int k = 0; k < final_vertices.size(); k++)
                    {
                        if (vertex_equal(&final_vertices[k], &vtx))
                        {
                            final_index = k;
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        final_index = final_vertices.size();
                        final_vertices.push_back(vtx);
                    }
                    final_indices.push_back(final_index);
                }
            }
        }
    }

    // Allocate and copy vertex data into the Mesh structure.
    if (!final_vertices.empty())
    {
        mesh.vertexCount = final_vertices.size();
        mesh.vertices = new vertex[mesh.vertexCount];
        memcpy(mesh.vertices, final_vertices.data(), mesh.vertexCount * sizeof(vertex));
    }
    else
    {
        fprintf(stderr, "Warning: No vertex data found in file: %s\n", path);
    }

    // Allocate and copy index data into the Mesh structure.
    if (!final_indices.empty())
    {
        mesh.indexCount = final_indices.size();
        mesh.indices = new unsigned int[mesh.indexCount];
        memcpy(mesh.indices, final_indices.data(), mesh.indexCount * sizeof(unsigned int));
    }
    else
    {
        fprintf(stderr, "Warning: No index data found in file: %s\n", path);
    }

    // Free the file buffer allocated by read_file.
    free(file_buffer);

    if (!has_some_normals)
    {
        // If no normals were found, calculate them based on the mesh data.
        calculate_mesh_normals(&mesh);
    }

    return mesh;
}