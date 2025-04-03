
#pragma once
#include <cstdio>  // For file operations
#include <cstdint> // For uint8_t, uint32_t
#include <stdlib.h>
#include "string.h"
#include <vector>
#include <sstream>

// Mesh structure definition using a 4D vector for positions.
// The vertices array will contain 4 floats per vertex: x, y, z, w.
typedef struct Mesh
{
    float *vertices;          // Pointer to an array of vertex positions (4 floats per vertex)
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

// Function to read an .obj file and parse it into a Mesh structure.
// This version supports 4D vertex positions. If the .obj vertex line
// provides only three components, the w component is set to 1.
Mesh read_mesh(const char *path)
{
    // Initialize mesh with null pointers and zero counts.
    Mesh mesh = {nullptr, 0, nullptr, 0};

    // Read the file content.
    uint32_t file_size = 0;
    uint32_t *file_buffer = read_file(path, &file_size);
    if (!file_buffer || file_size == 0)
    {
        fprintf(stderr, "Error: Failed to read file or file is empty: %s\n", path);
        return mesh;
    }

    // Temporary storage using std::vector for vertices and indices.
    // Each vertex will have 4 floats: x, y, z, w.
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    // Cast the file buffer to a char pointer for string processing.
    char *file_content = reinterpret_cast<char *>(file_buffer);
    // normalize_line_endings(file_content);

    // Tokenize the file content by newline.

    // Tokenize the file content by newline.
    std::istringstream file_stream(file_content);
    std::string line;
    while (std::getline(file_stream, line))
    {
        // Skip comments and empty lines.
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        // Process vertex lines. They start with "v ".
        if (line.rfind("v ", 0) == 0) // Check if the line starts with "v "
        {
            float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f; // Default w to 1
            // Try to parse 4 floats from the line.
            // If only 3 floats are provided, w remains 1.
            int count = sscanf(line.c_str() + 2, "%f %f %f %f", &x, &y, &z, &w);
            if (count < 3)
            {
                fprintf(stderr, "Error: Invalid vertex format: %s\n", line.c_str());
                continue;
            }
            // Store the 4 components in the vertices vector.
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            vertices.push_back(w);
        }
        // Process face lines. They start with "f ".
        else if (line.rfind("f ", 0) == 0) // Check if the line starts with "f "
        {
            // Temporary vector to hold the vertex indices of the current face.
            std::vector<unsigned int> face;

            // Tokenize the line by spaces.
            std::istringstream line_stream(line.substr(2)); // Skip the "f " prefix
            std::string token;
            while (line_stream >> token)
            {
                // .obj files may list vertices as "vertex_index/texcoord_index/normal_index"
                // Here, we simply extract the vertex index.
                unsigned int vertex_index = strtoul(token.c_str(), nullptr, 10);
                if (vertex_index == 0)
                {
                    fprintf(stderr, "Error: Invalid face index in line: %s\n", line.c_str());
                    break;
                }
                // Convert the 1-based index from .obj to a 0-based index.
                face.push_back(vertex_index - 1);
            }

            // If the face has at least 3 vertices, triangulate it.
            if (face.size() >= 3)
            {
                // For a face with more than 3 vertices, create a triangle fan.
                for (size_t i = 1; i + 1 < face.size(); i++)
                {
                    indices.push_back(face[0]);
                    indices.push_back(face[i]);
                    indices.push_back(face[i + 1]);
                }
            }
        }
    }

    // Allocate memory for the vertices in the Mesh structure if vertices were found.
    if (!vertices.empty())
    {
        // Calculate the vertex count (each vertex consists of 4 floats).
        mesh.vertexCount = static_cast<unsigned int>(vertices.size() / 4);
        // Allocate dynamic memory for the vertices.
        mesh.vertices = new float[vertices.size()];
        // Copy the vertex data into mesh.vertices.
        memcpy(mesh.vertices, vertices.data(), vertices.size() * sizeof(float));
    }
    else
    {
        fprintf(stderr, "Warning: No vertex data found in file: %s\n", path);
    }

    // Allocate memory for the indices if any indices were collected.
    if (!indices.empty())
    {
        mesh.indexCount = static_cast<unsigned int>(indices.size());
        mesh.indices = new unsigned int[indices.size()];
        memcpy(mesh.indices, indices.data(), indices.size() * sizeof(unsigned int));
    }
    else
    {
        fprintf(stderr, "Warning: No index data found in file: %s\n", path);
    }

    // Free the file buffer allocated by read_file.
    free(file_buffer);

    // Return the constructed mesh.
    return mesh;
}
