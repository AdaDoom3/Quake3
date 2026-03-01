// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
//                                                                 Q 3 . C
//          
//                                        Vulkan-based Id Tech inspired hybrid raytracing game engine     
//
// §1.  Settings              
// §2.  Types                 
// §3.  Context               
// §4.  Math                  
// §5.  Memory                
// §6.  Textures              
// §7.  Models                
// §8.  Scene                 
// §9.  Acceleration Structures
// §10. Physics               
// §11. Pipeline              
// §12. Shaders               
// §13. Render                
// §14. Main                  
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
//                                                       S P E C I F I C A T I O N
//                 
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Language Extensions
#include <iso646.h>

// Media Layer/Graphics
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

// Standard Libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>
  
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §1. Settings
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Engine identifer and information strings
const char *ENGINE_NAME    = "q3";
const char *ENGINE_VERSION = "0.1.0";

// Vulkan limits and versioning
#define VULKAN_API_VERSION       VK_API_VERSION_1_3 // Minimum Vulkan API level (synchronization2, dynamic rendering)
#define SWAPCHAIN_MAX_IMAGES     8                  // Upper bound for swapchain image handle array
#define DESCRIPTOR_TEXTURE_SLOTS 256                // Maximum entries in the bindless texture array (binding 11)
#define RAY_RECURSION_DEPTH      2                  // Primary ray + shadow ray

// Enable the Khronos validation layer for debug builds
const uint  VALIDATION_LAYER_COUNT = 1;
const char *VALIDATION_LAYERS[]    = {"VK_LAYER_KHRONOS_validation"};

// Required device extensions: swapchain, acceleration structure, ray tracing pipeline, deferred host ops
const uint DEVICE_EXTENSION_COUNT = 4;
const char *DEVICE_EXTENSIONS[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                   VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                                   VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                                   VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME};

// Windowing and viewport settings
#define DEFAULT_WIDTH  1280    // Initial window width in pixels
#define DEFAULT_HEIGHT 720     // Initial window height in pixels
#define FIELD_OF_VIEW  90.f    // Vertical field-of-view in degrees
#define NEAR_CLIP      0.1f    // Near clip plane distance
#define FAR_CLIP       10000.f // Far clip plane distance
#define MAX_DELTA_TIME 0.05f   // Clamp to 20 fps minimum (prevents physics tunneling)

// Asset paths
#define ASSET_ROOT  "assets/"  // Root directory for all game assets
#define SHADER_ROOT "shaders/" // Root directory for pre-compiled SPIR-V shaders

// Default BSP map to load when no command-line argument is given
const char *DEFAULT_MAP = "oa_dm1.bsp";

// Paths to pre-compiled SPIR-V shader modules (compiled offline from the GLSL blocks in §12)
#define SHADER_PATH_RAY_GENERATION  SHADER_ROOT "raygen.spv"
#define SHADER_PATH_CLOSEST_HIT     SHADER_ROOT "closesthit.spv"
#define SHADER_PATH_PRIMARY_MISS    SHADER_ROOT "miss.spv"
#define SHADER_PATH_SHADOW_MISS     SHADER_ROOT "shadow_miss.spv"
#define SHADER_PATH_PHYSICS_COMPUTE SHADER_ROOT "physics.spv"

// Paths to the weapon model's diffuse textures (body and sight)
#define WEAPON_TEXTURE_COUNT 2
const char *WEAPON_TEXTURE_PATHS[] = {ASSET_ROOT "models/weapons2/machinegun/mgun.tga",
                                      ASSET_ROOT "models/weapons2/machinegun/sight.tga"};

// Paths to the three-part machinegun weapon model (body, barrel, hand)
#define WEAPON_BODY_PATH   ASSET_ROOT "models/weapons2/machinegun/machinegun.md3"
#define WEAPON_BARREL_PATH ASSET_ROOT "models/weapons2/machinegun/machinegun_barrel.md3"
#define WEAPON_HAND_PATH   ASSET_ROOT "models/weapons2/machinegun/machinegun_hand.md3"

// Player physics constants designed to mirror the Quake 3 movement parameters.  The GPU physics compute shader (§10) references these same
// values as specialization constants compiled into the SPIR-V module.
#define GRAVITY             800.f  // Downward acceleration in units per second squared
#define GROUND_FRICTION     6.f    // Friction coefficient applied while on walkable ground
#define STOP_SPEED          100.f  // Speed below which friction uses this as the control speed
#define GROUND_ACCELERATE   10.f   // Acceleration rate while grounded (units/s per frame)
#define AIR_ACCELERATE      1.f    // Acceleration rate while airborne (enables strafe-jumping)
#define MAXIMUM_SPEED       320.f  // Maximum wish speed from input (units/s)
#define JUMP_VELOCITY       270.f  // Instantaneous upward velocity applied on jump
#define STEP_SIZE           18.f   // Maximum stair step height the player can walk up
#define MINIMUM_WALK_NORMAL 0.7f   // Minimum Y-component of a surface normal to count as walkable floor
#define OVERBOUNCE          1.001f // Slight overshoot factor when clipping velocity off surfaces
#define MAXIMUM_CLIP_PLANES 5      // Maximum simultaneous contact planes during slide-move resolution
#define DEFAULT_VIEW_HEIGHT 26.f   // Camera height offset from player origin when standing
#define CROUCH_VIEW_HEIGHT  12.f   // Camera height offset from player origin when crouching

// Player bounding box half-extents (x, y, z) used by the capsule collider
const float PLAYER_HALF_EXTENTS[3] = {15.f, 32.f, 15.f};

// Capsule spine half-length: half_height minus radius.  For a 32-unit tall, 15-unit radius capsule
// the spine is 32 - 15 = 17 units.
#define PLAYER_CAPSULE_SPINE 17.f

// Player bounding box minimum corner (symmetric on X/Z, asymmetric on Y for feet)
const vec3 PLAYER_MINIMUMS = {-15, -24, -15};

// Convex Hull Limits
#define HULL_MAX_VERTS     256 // Per-hull vertex cap (matches GPU array size in Gpu_Hull)
#define HULL_MAX_ADJ       16  // Maximum adjacency entries per vertex (for hill-climb support)
#define HULL_MAX_FACES     512 // Quickhull internal face cap during construction
#define HULL_MAX_ENTITIES  32  // Maximum simultaneous hull collider instances

// Weapon Animation Parameters
#define WEAPON_MODEL_SCALE  0.7f // Viewmodel scale factor for first-person perspective
#define WEAPON_FIRE_SPEED   10.f // Fire animation playback rate (frames per second)
#define WEAPON_FIRE_FRAMES  6.f  // Total fire animation duration in frames
#define WEAPON_BOB_RATE_V   3.5f // Vertical idle bob frequency (radians/second)
#define WEAPON_BOB_RATE_H   1.7f // Horizontal idle bob frequency (radians/second)
#define WEAPON_BOB_AMP_V    0.4f // Vertical idle bob amplitude (units)
#define WEAPON_BOB_AMP_H    0.2f // Horizontal idle bob amplitude (units)
#define WEAPON_RECOIL_AMP  -1.2f // Recoil kick magnitude (negative = pull back)
#define WEAPON_RECOIL_DECAY 5.f  // Recoil exponential decay rate  

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §2. Types
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Three-component floating-point vector for positions, directions, and velocities
typedef struct {float x, y, z;} vec3;

// Four-component floating-point vector for homogeneous coordinates and RGBA colors
typedef struct {float x, y, z, w;} vec4;

// A 4×4 column-major matrix for view, projection, and model transforms
typedef struct {float E[16];} mat4;

// Sampled keyboard and mouse state for a single frame
typedef struct {
  int   Forward, Back, Left, Right, Jump, Fire, Crouch; // Binary key states: 1 if held, 0 otherwise
  float Delta_X, Delta_Y;                               // Mouse displacement in pixels since last frame
} Input;

// GPU-resident buffer with its backing memory and optional device address
typedef struct {
  VkBuffer        Buffer;  // Vulkan buffer handle
  VkDeviceMemory  Memory;  // Device memory allocation backing the buffer
  VkDeviceAddress Address; // Buffer device address for shader access (zero if not requested)
  uint64_t        Size;    // Allocation size in bytes
} Gpu_Buffer;

// GPU-resident image with its backing memory, view, and format metadata
typedef struct {
  VkImage        Image;  // Vulkan image handle
  VkDeviceMemory Memory; // Device memory allocation backing the image
  VkImageView    View;   // Image view used for sampling or storage access
  VkFormat       Format; // Pixel format of the image
} Gpu_Image;

// Ray tracing acceleration structure with its backing buffer and device address
typedef struct {
  VkAccelerationStructureKHR Handle;  // Opaque acceleration structure handle
  Gpu_Buffer                 Buffer;  // GPU buffer holding the acceleration structure data
  VkDeviceAddress            Address; // Device address for referencing from shaders and TLAS builds
} Acceleration_Structure;

// Per-frame camera state uploaded to the GPU as a uniform buffer
typedef struct {
  vec3  Position, Velocity;               // World-space eye position and movement velocity
  float Yaw, Pitch;                       // Euler angles in radians for horizontal and vertical look
  mat4  Inverse_View, Inverse_Projection; // Inverse matrices for reconstructing world rays from screen coordinates
  uint  Frame;                            // Monotonically increasing frame counter for temporal effects
} Camera;

// Interleaved vertex layout matching the GPU shader input (std430, 48 bytes per vertex)
typedef struct {
  float Position   [3], Padding_A;       // World-space XYZ position; padding aligns to 16 bytes
  float Texture_Uv [2], Lightmap_Uv [2]; // Diffuse texture coordinates and lightmap atlas coordinates
  float Normal     [3], Padding_B;       // Surface normal; padding aligns to 16 bytes
} Vertex;

// Aggregate scene geometry and material data loaded from a BSP
typedef struct {
  Vertex  *Vertices;  uint Vertex_Count;    // Vertex array and its element count
  uint    *Indices;   uint Index_Count;     // Index array (triangles) and its element count
  vec4    *Materials; uint Material_Count;  // Per-surface RGBA material tints and their count
  uint    *Texture_Ids;                     // Per-triangle texture index into the texture array
  char   (*Texture_Names) [64];             // Shader/texture name strings from the BSP (64-char max each)
  uint     Triangle_Count;                  // Total triangles (Index_Count / 3)
  uint8_t *Lightmap_Atlas;                  // Packed lightmap atlas in RGBA8 format
  uint     Lightmap_Width, Lightmap_Height; // Atlas dimensions in pixels
} Scene;   

// Single spawn point parsed from the BSP entity lump
typedef struct {vec3 Origin; float Angle;} Spawn; // World-space origin and facing angle in degrees

// BSP lump directory entry: byte offset and length of a data lump within the file
typedef struct {int Offset, Length;} BSP_Lump;

// BSP file header: magic number, version, and the 17-entry lump directory
typedef struct {
  uint     Magic, Version; // Magic (0x50534249 = "IBSP") and format version (46 for Quake 3)
  BSP_Lump Lumps [BSP_LUMP_COUNT]; // Directory of data lumps indexed by BSP_ENTITIES..BSP_LIGHTMAPS
} BSP_Header;

// BSP vertex as stored on disk: position, two UV sets, normal, and vertex color
typedef struct {
  float   Position        [3]; // World XYZ
  float   Texture_Coords  [2]; // Diffuse UVs 
  float   Lightmap_Coords [2]; // Lightmap UVs
  float   Normal          [3]; // Unit surface normal
  uint8_t Color           [4]; // Vertex color (RGBA, 8 bits per channel)
} BSP_Vertex;

// BSP face/surface descriptor: references into vertex, index, and lightmap data
typedef struct {
  int   Shader_Index;                            // Material shader
  int   Fog_Volume;                              // Fog reference
  int   Type;                                    // Surface type (planar/patch/mesh)
  int   First_Vertex,       Vertex_Count;        // Starting vertex index and count in the global vertex array
  int   First_Index,        Index_Count;         // Starting element index and count in the global index array
  int   Lightmap_Index;                          // Lightmap page index (-1 if none)
  int   Lightmap_X,         Lightmap_Y;          // Top-left corner of this face's lightmap within its page
  int   Lightmap_Width,     Lightmap_Height;     // Dimensions of the lightmap region in texels
  float Lightmap_Origin[3], Lightmap_Vectors[9]; // World-space lightmap placement (origin + 2 basis vectors + normal)
  int   Patch_Width,        Patch_Height;        // Control point grid dimensions (only valid for patch surfaces)
} BSP_Face;

// BSP shader entry: maps a surface material name to content and surface flags
typedef struct {
  char Name[64];        // Shader path (e.g. "textures/gothic_wall/wall01")
  int  Flags, Contents; // Surface flags (e.g. translucent) and content flags (e.g. solid, water)
} BSP_Shader;

// MD3 surface header: describes one mesh within an MD3 model file
typedef struct {
  int  Magic;                                   // Surface magic identifier (always IDP3)
  char Name[64];                                // Null-terminated surface name
  int  Flags;                                   // Surface flags (unused in Quake 3)
  int  Number_Of_Frames, Number_Of_Shaders;     // Animation frame count and attached shader count
  int  Number_Of_Vertices, Number_Of_Triangles; // Per-frame vertex count and triangle count
  int  Triangles_Offset, Shaders_Offset;        // Byte offsets from surface start to triangle and shader data
  int  Texture_Coordinates_Offset;              // Byte offset to the per-vertex texture coordinate array
  int  Vertices_Offset, End_Offset;             // Byte offset to compressed vertex frames and to the next surface
} MD3_Surface;

// MD3 tag: a named attachment point with position and orientation for linking model parts
typedef struct {
  char  Name[64];  // Null-terminated tag name (e.g. "tag_barrel", "tag_weapon")
  float Origin[3]; // World-space position of the attachment point
  float Axis[9];   // A 3×3 rotation matrix (row-major) defining the tag's local coordinate frame
} MD3_Tag;

// Parsed weapon model assembled from multiple MD3 surfaces (body, barrel, hand)
typedef struct {
  Vertex *Vertices;     uint Vertex_Count;     // Merged vertex array from all surfaces
  uint    *Indices;     uint Index_Count;      // Merged index array from all surfaces
  uint    *Texture_Ids; uint Triangle_Count;   // Per-triangle texture index and total triangle count
  float   Tag_Barrel[12];                      // Barrel attachment transform: origin[3] + axis[9]
  float   Tag_Weapon[MD3_MAX_ANIM_FRAMES][12]; // Per-frame weapon tag transforms (up to 30 animation frames)
  uint    Animation_Frame_Count;               // Number of valid frames in the Tag_Weapon array
  char    Texture_Names[MD3_MAX_SURFACES][64]; // Texture path for each surface (body, barrel, hand)
  uint    Surface_Count;                       // Number of surfaces composing this weapon (typically 3)
} Weapon_Model;

// Runtime weapon state combining the model data with per-frame animation and GPU resources
typedef struct {
  Weapon_Model           Model;                // Parsed model geometry and attachment tags
  Vertex                *Transformed_Vertices; // Scratch buffer for CPU-side per-frame vertex transformation
  int                    Is_Firing;            // Non-zero while the fire button is held
  float                  Fire_Time, Bob_Time;  // Recoil decay timer and idle bob phase accumulator
  Gpu_Buffer             Vertex_Buffer, Index_Buffer, Texture_Id_Buffer; // GPU buffers for weapon geometry
  Acceleration_Structure Bottom_Level;         // BLAS for the weapon (rebuilt each frame)
  Gpu_Buffer             Bottom_Level_Scratch; // Scratch buffer reused across BLAS rebuilds
  uint                   Texture_Base_Index;   // Starting index into the global texture array for weapon textures
} Weapon_Instance;

// Player movement state: position, velocity, orientation, and ground contact information.
// This is the CPU-side mirror of Gpu_Player; Physics_Dispatch reads back into this after
// the compute shader finishes.
typedef struct {
  vec3  Position;      // World-space position of the player's bounding box origin
  vec3  Velocity;      // Current velocity in units per second
  float Yaw, Pitch;    // Look direction: yaw (horizontal) and pitch (vertical) in radians
  int   On_Ground;     // Non-zero if the player is standing on a walkable surface
  int   Jump_Held;     // Non-zero if the jump key was held last frame (prevents auto-bunny-hopping)
  vec3  Ground_Normal; // Surface normal of the ground plane the player is standing on
  int   Ground_Plane;  // Non-zero if the ground trace hit a valid plane (not an edge or brush start)
  int   Ducked;        // Non-zero if the player is crouching
  float View_Height;   // Camera height offset from Position.y (smoothly interpolated)
} Player;

// Quickhull internal types used during hull construction
typedef struct {int A, B, C; int Dead;} QH_Face;
typedef struct {int V0, V1, Face;}      QH_Edge;

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §3. Context
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Central rendering context holding all Vulkan state, GPU resources, and synchronization objects
SDL_Window      *Window;             // SDL window for presentation and input
int              Width  = DEFAULT_WIDTH;  // Window width in pixels
int              Height = DEFAULT_HEIGHT; // Window height in pixels
VkInstance       Instance;           // Vulkan instance with validation layers
VkSurfaceKHR     Surface;            // Window surface for presentation
VkPhysicalDevice Physical_Device;    // Selected GPU with ray tracing support
VkDevice         Device;             // Logical device created from the physical device
VkQueue          Queue;              // Universal queue for graphics, compute, and transfer
uint             Queue_Family_Index; // Index of the queue family supporting all operations

// Swapchain state
VkSwapchainKHR Swapchain;                               // Presentation swapchain
VkImage        Swapchain_Images [SWAPCHAIN_MAX_IMAGES]; // Swapchain image handles (up to 8 for triple+ buffering)
VkImageView    Swapchain_Views  [SWAPCHAIN_MAX_IMAGES]; // Image views corresponding to each swapchain image
uint           Swapchain_Image_Count;                   // Actual number of swapchain images acquired
VkFormat       Swapchain_Format;                        // Surface format of the swapchain (e.g. B8G8R8A8_SRGB)
VkExtent2D     Swapchain_Extent;                        // Swapchain resolution in pixels

// Command recording and CPU-GPU synchronization
VkCommandPool   Command_Pool;              // Command pool for allocating command buffers
VkCommandBuffer Command_Buffer;            // Single reusable command buffer for all GPU work
VkFence         Fence;                     // CPU-GPU synchronization fence for frame serialization
VkSemaphore     Semaphore_Image_Available; // Signals when a swapchain image is ready
VkSemaphore     Semaphore_Render_Finished; // Signals when rendering is complete for presentation

// Ray tracing extension state
VkPhysicalDeviceRayTracingPipelinePropertiesKHR Raytracing_Properties; // Shader binding table (SBT) alignment and handle sizes

// X-macro for declaring Vulkan function pointer variables from their spec names
#define DECLARE_VK(vk, alias) PFN_##vk alias;

// X-macro for loading Vulkan function pointers from the logical device at runtime
#define LOAD_VK(vk, alias) alias = (PFN_##vk) vkGetDeviceProcAddr (Device, #vk);

// Master list of ray tracing extension functions loaded via vkGetDeviceProcAddr.
// Each entry maps the Vulkan spec name to a shorter alias used throughout this file.
#define VULKAN_FUNCTIONS(_) \
  _(vkCreateAccelerationStructureKHR,           vkCreateAccelerationStructure)           /* Creates a BLAS or TLAS */ \
  _(vkDestroyAccelerationStructureKHR,          vkDestroyAccelerationStructure)          /* Destroys an acceleration structure */ \
  _(vkGetAccelerationStructureBuildSizesKHR,    vkGetAccelerationStructureBuildSizes)    /* Queries scratch and result buf sizes */ \
  _(vkCmdBuildAccelerationStructuresKHR,        vkCmdBuildAccelerationStructures)        /* Records a build or update command */ \
  _(vkGetAccelerationStructureDeviceAddressKHR, vkGetAccelerationStructureDeviceAddress) /* Retrieves the device address */ \
  _(vkCreateRayTracingPipelinesKHR,             vkCreateRayTracingPipelines)             /* Creates a ray tracing pipeline */ \
  _(vkGetRayTracingShaderGroupHandlesKHR,       vkGetRayTracingShaderGroupHandles)       /* Retrieves shader group handles (SBT) */ \
  _(vkCmdTraceRaysKHR,                          vkCmdTraceRays)                          /* Records a ray dispatch command */
VULKAN_FUNCTIONS (DECLARE_VK)

// Assertion to validate Vulkan return values; prints the error code, file, and line number then exits
#define VK_CHECK(Call) do { /* Dummy loop to contain macro */ \
  VkResult _Result = (Call); \
  if (_Result) {fprintf (stderr, "[vulkan] error %d at %s:%d\n", _Result, __FILE__, __LINE__); exit (1); \
} while (0) // End macro dummy loop

// GPU storage images and scene data buffers
Gpu_Image  Raytracing_Storage_Image;                     // Storage image written by ray generation shader
Gpu_Buffer Camera_Uniform_Buffer;                        // Uniform buffer for the Camera struct
Gpu_Buffer Vertex_Buffer, Index_Buffer, Material_Buffer; // Scene geometry and material data on GPU
Gpu_Buffer Texture_Id_Buffer;                            // Per-triangle texture index buffer

// Diffuse texture array
VkImage        *Texture_Images;   // Array of diffuse texture images
VkDeviceMemory *Texture_Memories; // Backing memory for each texture image
VkImageView    *Texture_Views;    // Image views for shader sampling of each texture
VkSampler       Texture_Sampler;  // Shared sampler with linear filtering and repeat wrap
uint            Texture_Count;    // Total number of texture slots allocated
uint            Textures_Loaded;  // Number of textures successfully loaded from disk

// Lightmap atlas
VkImage        Lightmap_Image;   // Packed lightmap atlas image
VkDeviceMemory Lightmap_Memory;  // Backing memory for the lightmap image
VkImageView    Lightmap_View;    // Image view for lightmap sampling
VkSampler      Lightmap_Sampler; // Sampler for lightmap lookups (linear, clamp-to-edge)

// Acceleration structures
Acceleration_Structure Bottom_Level, Top_Level; // BLAS for world geometry and TLAS combining all instances

// Host-visible instance buffer for writing TLAS instance descriptors each frame
Gpu_Buffer Top_Level_Instance_Buffer;

// Persistent scratch memory reused across per-frame TLAS rebuilds
Gpu_Buffer Top_Level_Scratch_Buffer;

// Ray tracing pipeline and shader binding table
VkPipelineLayout Pipeline_Layout;             // Pipeline layout with descriptor set bindings
VkPipeline       Pipeline;                    // Ray tracing pipeline (rgen, rchit, rmiss, shadow rmiss)
Gpu_Buffer       Shader_Binding_Table_Buffer; // Buffer holding the shader binding table

// Shader binding table regions (one per shader stage)
VkStridedDeviceAddressRegionKHR Shader_Binding_Ray_Generation; // SBT region for the ray generation shader
VkStridedDeviceAddressRegionKHR Shader_Binding_Miss;           // SBT region for miss shaders
VkStridedDeviceAddressRegionKHR Shader_Binding_Hit;            // SBT region for closest-hit shaders
VkStridedDeviceAddressRegionKHR Shader_Binding_Callable;       // SBT region for callable shaders (unused, zeroed)

// Descriptor set
VkDescriptorSetLayout Descriptor_Set_Layout; // Layout describing all 12 descriptor bindings
VkDescriptorPool      Descriptor_Pool;       // Pool from which the single descriptor set is allocated
VkDescriptorSet       Descriptor_Set;        // Descriptor set binding all resources to the pipeline

// GPU physics pipeline state
VkPipeline            Physics_Pipeline;          // Compute pipeline for physics simulation
VkPipelineLayout      Physics_Pipeline_Layout;   // Pipeline layout with push constants for Gpu_Input
VkDescriptorSetLayout Physics_Descriptor_Layout; // Layout: TLAS + vertex + index + player + hull (5 bindings)
VkDescriptorPool      Physics_Descriptor_Pool;   // Pool for the physics descriptor set
VkDescriptorSet       Physics_Descriptor_Set;    // Descriptor set binding physics resources
Gpu_Buffer            Player_State_Buffer;       // SSBO holding the Gpu_Player state (read-write each frame)
Gpu_Buffer            Hull_Storage_Buffer;       // SSBO holding Gpu_Hull vertex + adjacency data (binding 4)

// Application state
int   Quit;       // Non-zero when the application should exit
float Delta_Time; // Time elapsed since the previous frame in seconds

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §4. Math
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Basic vector operations
vec3  Make     (float x, float y, float z) {return (vec3){x, y, z};}
vec3  Scale    (vec3 Vector, float Scalar) {return Make (Vector.x * Scalar, Vector.y * Scalar, Vector.z * Scalar);}
vec3  Add      (vec3 Left,   vec3  Right)  {return Make (Left.x + Right.x, Left.y + Right.y, Left.z + Right.z);}
vec3  Subtract (vec3 Left,   vec3  Right)  {return Make (Left.x - Right.x, Left.y - Right.y, Left.z - Right.z);}
float Dot      (vec3 Left,   vec3  Right)  {return Left.x * Right.x + Left.y * Right.y + Left.z * Right.z;}
vec3  Cross    (vec3 Left,   vec3  Right)  {return Make (/*x =>*/ Left.y * Right.z - Left.z * Right.y,
                                                         /*y =>*/ Left.z * Right.x - Left.x * Right.z,
                                                         /*z =>*/ Left.x * Right.y - Left.y * Right.x);}
vec3 Normalize (vec3 Vector) {float Length = sqrtf (Dot (Vector, Vector));
                              return Length > 1e-6f ? Scale (Vector, 1.f / Length) : Vector;}

// Construct a reversed-depth perspective matrix from vertical field-of-view in degrees, aspect ratio, and near/far clip distances. The Y
// axis is flipped for Vulkan conventions.
mat4 Perspective (float Fovy_Degrees, float Aspect, float Near, float Far);

// Build a view matrix from a world-space position and Euler yaw/pitch angles. The forward vector points along yaw with pitch elevation;
// the up vector is derived from the cross product of the right and forward vectors.
mat4 View (vec3 Position, float Yaw, float Pitch);

// Invert an orthogonal matrix (rotation + translation only) by transposing the 3×3 rotation block and recomputing the translation as the
// negated rotated original translation.
mat4 Inverse_Orthogonal (mat4 Source);

// Analytically invert a perspective projection matrix by exploiting its known sparse structure. Only the non-zero elements are inverted;
// all others remain zero.
mat4 Inverse_Projection (mat4 Projection);

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §5. Memory
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Search the physical device's memory heaps for a memory type index that satisfies both the type bitmask (from memory requirements) and
// the desired property flags (host-visible, device-local, etc).
uint Find_Memory_Type (uint Type_Bits, VkMemoryPropertyFlags Desired_Properties);

// Allocate a GPU buffer with the given size, usage flags, and memory properties. If the usage includes shader device address, the
// allocation is flagged accordingly and the device address is queried.
Gpu_Buffer Buffer_Allocate (uint64_t Size, VkBufferUsageFlags Usage, VkMemoryPropertyFlags Memory_Flags);

// Map the buffer's device memory into host address space, copy the source data, then unmap
void Buffer_Upload (Gpu_Buffer Destination, const void *Data, uint64_t Size);

// Upload data to device-local memory via a host-visible staging buffer. A one-shot command buffer
// performs the copy, then the staging buffer is freed. The resulting buffer has the requested usage
// flags plus transfer-destination and shader-device-address.
Gpu_Buffer Buffer_Stage_Upload (VkCommandBuffer Command_Buffer, VkQueue Queue,
                                const void *Data, uint64_t Size, VkBufferUsageFlags Usage);

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §6. Textures
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Load a TGA image file and decode it into RGBA8 pixel data. Supports uncompressed 8/24/32-bit images (type 2/3) and RLE-compressed
// true-color images (type 10). The output is always bottom-to-top, RGBA, 8 bits per channel.
uint8_t *TGA_Load (const char *Path, uint *Out_Width, uint *Out_Height);

// Upload raw RGBA pixel data to a device-local texture image via staging buffer,
// transitioning the image layout from undefined through transfer-destination to shader-read-only.
// The caller specifies the Vulkan format (SRGB vs UNORM).
void Texture_Upload_With_Format (VkCommandBuffer Command_Buffer, VkQueue Queue,
                                 const uint8_t *Pixels, uint Width, uint Height, VkFormat Format,
                                 VkImage *Out_Image, VkDeviceMemory *Out_Memory, VkImageView *Out_View);

// Convenience wrapper that uploads a texture as SRGB
void Texture_Upload (VkCommandBuffer Command_Buffer, VkQueue Queue,
                     const uint8_t *Pixels, uint Width, uint Height,
                     VkImage *Out_Image, VkDeviceMemory *Out_Memory, VkImageView *Out_View);

// Create a device-local 2D image suitable for use as a ray tracing storage target. The image is RGBA8 UNORM with storage and
// transfer-source usage bits.
Gpu_Image Image_Storage_Create (uint Width, uint Height);

// Insert a pipeline barrier that transitions an image between layouts, specifying the source and destination access masks and pipeline
// stages for proper synchronization. 
void Image_Layout_Barrier (VkCommandBuffer      Command_Buffer, VkImage              Image,
                           VkImageLayout        Old_Layout,     VkImageLayout        New_Layout,
                           VkAccessFlags        Source_Access,  VkAccessFlags        Destination_Access,
                           VkPipelineStageFlags Source_Stage,   VkPipelineStageFlags Destination_Stage);

// Create a sampler with linear filtering and repeating address mode on all axes
VkSampler Sampler_Create_Repeating (void);

// Create a sampler with linear filtering and clamp-to-edge on all axes
VkSampler Sampler_Create_Clamping (void);

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §7. Models
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Quake 3 animated model (MD3) format
#define MD3_MAGIC           0x33504449u // "IDP3" as a 32-bit little-endian integer
#define MD3_MAX_SURFACES    3           // Maximum surfaces per weapon part (body, barrel, hand)
#define MD3_MAX_ANIM_FRAMES 30          // Maximum animation frames extracted from tag_weapon

// Parse a single MD3 surface's geometry (vertices, indices, texture coordinates) into the growing output arrays. An optional 12-float
// transform (origin + 3×3 axis matrix) can pre-transform vertices and normals. Quake 3 coordinate swizzle (x,y,z)->(x,z,-y) is
// applied.
void MD3_Parse_Surface (const uint8_t *Surface_Data,
                        Vertex **Inout_Vertices,    uint *Inout_Vertex_Count,
                        uint   **Inout_Indices,     uint *Inout_Index_Count,
                        uint   **Inout_Texture_Ids, uint *Inout_Triangle_Count,
                        uint Assigned_Texture_Index, const float *Transform);

// Load the three-part machinegun weapon model (body, barrel, hand) from MD3 files.
// The barrel is pre-transformed by tag_barrel; animation frames are extracted from tag_weapon in the hand model.
Weapon_Model Weapon_Model_Load (void);

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §8. Scene
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Quake 3 BSP format
#define BSP_MAGIC            0x50534249u // "IBSP" as a 32-bit little-endian integer
#define BSP_VERSION          46          // Quake 3 BSP format version number
#define BSP_ENTITIES         0           // Lump index: entity definitions (key-value text)
#define BSP_SHADERS          1           // Lump index: shader/material name table
#define BSP_PLANES           2           // Lump index: splitting planes for BSP tree and brushes
#define BSP_NODES            3           // Lump index: interior BSP tree nodes
#define BSP_LEAFS            4           // Lump index: BSP tree leaf nodes
#define BSP_LEAF_SURFACES    5           // Lump index: per-leaf surface index lists
#define BSP_LEAF_BRUSHES     6           // Lump index: per-leaf brush index lists
#define BSP_BRUSHES          8           // Lump index: convex brush volumes for collision
#define BSP_BRUSH_SIDES      9           // Lump index: bounding planes for each brush
#define BSP_VERTICES         10          // Lump index: vertex positions, UVs, normals, colors
#define BSP_INDICES          11          // Lump index: triangle mesh element indices
#define BSP_FACES            13          // Lump index: face/surface descriptors
#define BSP_LIGHTMAPS        14          // Lump index: 128×128 RGB lightmap pages
#define BSP_LUMP_COUNT       17          // Total number of lumps in the BSP directory
#define SURFACE_TYPE_PLANAR  1           // Face type: flat polygon rendered from indices
#define SURFACE_TYPE_PATCH   2           // Face type: Bézier patch (tessellated at load time)
#define SURFACE_TYPE_MESH    3           // Face type: triangle mesh (e.g. models in BSP)
#define TESSELLATION_LEVEL   5           // Number of subdivisions per Bézier patch edge
#define LIGHTMAP_PAGE_SIZE   128         // Width and height in texels of each lightmap page

// Convert a BSP vertex from Quake 3's Z-up coordinate system to our Y-up system: (x,y,z) becomes (x,z,-y).
Vertex Convert_BSP_Vertex (const BSP_Vertex *Source);

// Evaluate a quadratic Bézier curve at parameter t given three control points.
vec3 Bezier_Evaluate (vec3 Control_A, vec3 Control_B, vec3 Control_C, float Parameter);

// Tessellate a Bézier patch surface from its control grid into triangles. The patch is subdivided
// into a grid of sub-patches (each defined by a 3×3 control point block), and each sub-patch is
// evaluated at TESSELLATION_LEVEL intervals to produce a smooth triangle mesh.
uint BSP_Tessellate_Patch (const BSP_Vertex *Control_Grid, int Patch_Width, int Patch_Height,
                           Vertex **Inout_Vertices, uint *Inout_Vertex_Count,
                           uint **Inout_Indices, uint *Inout_Index_Count);

// Parse the BSP entity lump to find the first info_player_deathmatch spawn point.
// Returns the origin (swizzled to Y-up) and facing angle.
Spawn BSP_Find_Spawn (const uint8_t *File_Data, const BSP_Header *Header);

// Load a complete scene from a Quake 3 BSP file. This parses vertices, indices, faces (planar,
// mesh, and patch types), shader references, and lightmap pages (packed into a single atlas).
// Collision detection is handled entirely on the GPU via ray tracing against the TLAS, so no
// CPU-side collision map is built — the BSP tree structure is only used for geometry extraction.
Scene Scene_Load_From_BSP (const char *Path, Spawn *Out_Spawn);

// Load textures for every material in the scene. Attempts to load TGA files from the assets
// directory; materials without a texture file fall back to a 1×1 solid-color pixel derived
// from the hashed shader name. Also uploads the lightmap atlas and per-triangle texture IDs.
void Scene_Load_Textures (const Scene *Scene_Data);

// Load the weapon model's TGA textures and append them to the global texture array.
void Weapon_Load_Textures (Weapon_Instance *Weapon);

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §9. Acceleration Structures
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Build the world geometry's bottom-level acceleration structure (BLAS).  Uploads the scene
// vertex, index, and material buffers to the GPU, then constructs a single BLAS geometry
// entry covering all triangles. Uses PREFER_FAST_TRACE since the world is static.
Acceleration_Structure Build_World_Bottom_Level (const Scene *Scene_Data);

// Initialize the weapon's BLAS with host-visible vertex buffer (for per-frame updates)
// and ALLOW_UPDATE flag for fast rebuilds. Scratch memory is kept alive for reuse.
void Weapon_Bottom_Level_Initialize (Weapon_Instance *Weapon);

// Rebuild the weapon BLAS from scratch after CPU vertex transformation.
// Re-uploads the vertex buffer and performs a full (non-update) rebuild.
void Weapon_Bottom_Level_Rebuild (Weapon_Instance *Weapon);

// Pre-allocate the top-level acceleration structure (TLAS) for up to Maximum_Instances
// instance entries. The instance buffer, scratch buffer, and TLAS object are created once
// and reused across frames. The TLAS is rebuilt (not updated) each frame.
void Top_Level_Initialize (uint Maximum_Instances);

// Rebuild the TLAS each frame with the world BLAS as instance 0 (mask 0xFF) and optionally
// the weapon BLAS as instance 1 (mask 0x01, so shadow rays can skip it).
void Top_Level_Rebuild (Acceleration_Structure *World, Acceleration_Structure *Weapon);

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §10. Physics
//
//   GPU-only physics via ray tracing against the TLAS.  The compute shader traces rays from the player's expanded shape against world
//   geometry, resolves contacts via a slide-move algorithm, and writes back the updated Gpu_Player state.
//
//   Six collider shapes, each defining a support function s(d̂) : S² → ℝ³:
//     SPHERE      s(d̂) = d̂ · r                                  Projectiles, pickups
//     CAPSULE     s(d̂) = d̂ · r + (0, sign(d̂.y) · spine, 0)     Player, NPCs
//     AABB        s(d̂) = sign(d̂) ⊙ extents                      Crates, elevators
//     CYLINDER    s(d̂) = (d̂.xz/‖d̂.xz‖ · r, sign(d̂.y) · h, 0)  Barrels, columns
//     ELLIPSOID   s(d̂) = normalize(d̂ ⊘ axes) ⊙ axes · ‖...‖    Vehicles
//     HULL        s(d̂) = argmax(v · d̂) over vertex set          Arbitrary convex models
//
//   Convex hull support uses hill-climbing with adjacency for O(√n) amortized queries on hulls with ≥64 vertices, falling back to O(n)
//   brute-force for smaller hulls.
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ── Collider Shape Enumeration ───────────────────────────────────────────────────────────────────
//
// Six collider shapes, each defining a support function s(d̂) : S² → ℝ³ from unit directions to
// surface offsets.  The GPU physics compute shader switches on this enum to select the appropriate
// Minkowski support mapping.

enum Collider_Shape {SHAPE_SPHERE,    // Projectiles:       s(d̂) = d̂ · r                                   
                     SHAPE_CAPSULE,   // Player, NPCs:      s(d̂) = d̂ · r + (0, sign(d̂.y) · spine, 0)      
                     SHAPE_AABB,      // Crates, elevators: s(d̂) = sign(d̂) ⊙ extents                     
                     SHAPE_CYLINDER,  // Barrels, columns:  s(d̂) = (d̂.xz/‖d̂.xz‖ · r, sign(d̂.y) · h, 0)   
                     SHAPE_ELLIPSOID, // Vehicles:          s(d̂) = normalize(d̂ ⊘ axes) ⊙ axes · ‖...‖     
                     SHAPE_HULL};     // Arbitrary models   s(d̂) = argmax(v · d̂) over vertex set      

// GPU-resident player state uploaded to the physics compute shader (std430, 112 bytes).
// The compute shader reads and writes this buffer each frame; the host reads it back
// afterward to update the Camera and weapon transform.
typedef struct {
  float Position     [3]; float Pad_A;
  float Velocity     [3]; float Pad_B;
  float Yaw, Pitch;
  int   On_Ground, Jump_Held;
  float Ground_Normal[3]; float Pad_C;
  int   Ground_Plane, Ducked;
  float View_Height, Stuck_Time;
  float Speed_Last;  int Shape;        // Previous frame speed; active collider shape (enum Collider_Shape)
  float Extents      [3]; float Pad_D; // Half-extents / radii / semi-axes (shape-dependent)
  float Spine;       float Pad_E [3];  // Capsule spine half-length (hh - radius), 0 for non-capsules
} Gpu_Player;

// Per-frame input delivered to the physics compute shader via push constants (48 bytes)
typedef struct {
  int   Forward, Back, Left, Right;
  int   Jump, Fire, Crouch, Pad;
  float Delta_X, Delta_Y, Dt, Pad2;
} Gpu_Input;

// CPU-side convex hull produced by the Quickhull algorithm.  Stores vertex positions and per-vertex
// adjacency for hill-climbing support queries.
typedef struct {
  vec3  Vertices  [HULL_MAX_VERTS];               // Hull vertex positions in local space
  int   Adjacency [HULL_MAX_VERTS][HULL_MAX_ADJ]; // Per-vertex neighbor indices (-1 terminated)
  uint  Vertex_Count;                             // Number of hull vertices
  vec3  Centroid;                                 // Geometric center (for local-space offset)
  float Bounding_Radius;                          // Tight bounding sphere radius from centroid
} Convex_Hull;

// GPU-packed hull data uploaded to the physics compute shader's storage buffer (binding 4).
// The shader uses this for SHAPE_HULL support queries via hill-climbing with adjacency.
typedef struct {
  float Vertices  [HULL_MAX_VERTS][4];            // xyz + padding per vertex (std430 vec4 array)
  int   Adjacency [HULL_MAX_VERTS][HULL_MAX_ADJ]; // Neighbor indices, -1 terminated
  int   Count;                                    // Vertex count
  float Radius;                                   // Bounding sphere radius
  float Centroid  [3];                            // Local-space centroid
  int   Pad;
} Gpu_Hull;

// Signed distance from point P to the plane of triangle (A, B, C) — used by Quickhull
float QH_Dist (vec3 P, vec3 A, vec3 B, vec3 C);

// Build a convex hull from a point cloud using the Quickhull algorithm.  Returns the hull
// with deduplicated vertices and per-vertex adjacency tables for GPU hill-climbing support.
Convex_Hull Quickhull (const vec3 *Points, uint Count);

// Convenience wrapper: extract vec3 positions from a Vertex array and build the convex hull
Convex_Hull Hull_From_Vertices (const Vertex *Vertices, uint Count);

// Pack a CPU-side Convex_Hull into Gpu_Hull format and upload to the hull storage buffer (binding 4)
void Hull_Upload (const Convex_Hull *Hull);

// Create the GPU physics compute pipeline with 5 descriptor bindings:
//   binding 0: TLAS (acceleration structure)
//   binding 1: world vertex buffer (storage)
//   binding 2: world index buffer (storage)
//   binding 3: Gpu_Player state (storage, read-write)
//   binding 4: Gpu_Hull data (storage, read-only)
// Push constants deliver the Gpu_Input struct (48 bytes) per frame.
void Physics_Pipeline_Create ();

// Initialize the Gpu_Player state buffer from a CPU-side Player, allocate the hull storage
// buffer (with a 1-vertex dummy if no hull has been uploaded yet), create the descriptor pool
// and set, and bind all physics resources.
void Physics_Resources_Create (const Player *Initial_State);

// Dispatch the physics compute shader for one frame: push the current input, execute a single
// workgroup, wait for completion, then read back the updated Gpu_Player state into a CPU-side
// Player struct.  The compute shader handles mouse look, acceleration, gravity, friction,
// jump, crouch, and slide-move collision resolution against the TLAS.
Player Physics_Dispatch (Input In, float Delta_Time);

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §11. Pipeline
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Create the ray tracing pipeline with four shader stages: ray generation, primary miss, shadow miss, and closest-hit. The descriptor set
// layout defines 12 bindings covering the TLAS, storage image, camera uniform, vertex/index/material/texture-id buffers, lightmap sampler,
//weapon buffers, and a variable-count texture array.
void Raytracing_Pipeline_Create ();

// Build the shader binding table (SBT) by querying shader group handles from the pipeline and laying them out in an aligned buffer. Each
// group gets one entry at the required stride.
void Shader_Binding_Table_Create ();

// Allocate the descriptor pool and set, then write all 12 descriptor bindings for the ray tracing pipeline (TLAS, storage image, camera,
// world geometry, weapon geometry, lightmap, and the variable-count texture array).
void Descriptor_Set_Create (Weapon_Instance *Weapon);

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §12. Shaders
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Read a SPIR-V binary file from disk and wrap it in a Vulkan shader module.
VkShaderModule Shader_Module_Load (const char *Path);

// Closest-hit shader (rchit).  Interpolates vertex attributes at the hit point using barycentric coordinates, samples the albedo from the
// bindless texture array, applies lightmap-based illumination for BSP geometry or simple directional lighting for the weapon model, traces
// a shadow ray toward the sun, and returns the final color.
// glsl rchit Closest_Hit;

// Primary miss shader (rmiss).  Called when a ray from the ray generation shader misses all geometry. Returns a procedural sky gradient
// interpolated from a pale horizon color to a deeper blue at the zenith, based on the ray's vertical component.
// glsl rmiss Primary_Miss;

// Shadow miss shader (rmiss, index 1).  Called when a shadow ray reaches the sun without hitting any occluder. Sets the shadow factor to
// 1.0 to indicate full illumination; if the ray had hit geometry, the closest-hit shader would not be invoked (due to
// SkipClosestHitShader flag) and the factor remains at 0.0.
// glsl rmiss Shadow_Miss;

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §13. Render
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Upload the camera uniform buffer with the inverse view and projection matrices computed from
// the current player position, yaw, pitch, field-of-view, and aspect ratio.
void Camera_Upload (Camera *State, float Field_Of_View, uint Weapon_Texture_Base);

// Update the weapon viewmodel's vertex positions each frame based on the camera orientation,
// idle bob animation, and recoil animation from firing.  The transformed vertices are written
// to the CPU scratch buffer and subsequently uploaded to the GPU for BLAS rebuild.
void Weapon_Update (Weapon_Instance *Weapon, const Camera *Camera_Data, float Delta_Time, int Fire);

// Sample the current keyboard and mouse state from SDL, returning the frame's input snapshot.
// Also processes SDL_QUIT and ESCAPE key events to set the global Quit flag.
Input Poll_Input (void);

// Record and submit one frame of ray tracing: bind the pipeline and descriptors, dispatch
// traceRaysKHR for every pixel, blit the storage image to the swapchain, and present.
void Raytracing_Frame (void);

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §14. Main
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Vulkan initialization helpers (called in sequence from main)
void Vulkan_Create_Instance ();
void Vulkan_Pick_Physical_Device ();
void Vulkan_Create_Logical_Device ();
void Vulkan_Create_Swapchain ();
void Vulkan_Create_Synchronization ();
void Vulkan_Transition_Storage_Image ();

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
//                                                            B  O  D  Y
//                 
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ════════════
//   Identity 
// ════════════

mat4 Identity (void) {mat4 Result = {0}; Result.E[0]  = 1; Result.E[5]  = 1;
                                         Result.E[10] = 1; Result.E[15] = 1; return Result;}

// ═══════════════
//   Perspective 
// ═══════════════

mat4 Perspective (float Fovy_Degrees, float Aspect, float Near, float Far) {
  float Focal_Length = 1.f / tanf (Fovy_Degrees * (float)M_PI / 360.f);

  // Populate the matrix elements for reversed-depth Vulkan projection
  mat4 Result = {0};
  Result.E[0]  = Focal_Length / Aspect;
  Result.E[5]  = -Focal_Length;
  Result.E[10] = Far / (Near - Far);
  Result.E[11] = -1;
  Result.E[14] = Near * Far / (Near - Far);
  return Result;
}

// ════════
//   View 
// ════════

mat4 View (vec3 Position, float Yaw, float Pitch) {
  float Cosine_Yaw   = cosf (Yaw);
  float Sine_Yaw     = sinf (Yaw);
  float Cosine_Pitch = cosf (Pitch);
  float Sine_Pitch   = sinf (Pitch);

  // Derive the camera's orthonormal basis from yaw and pitch
  vec3 Forward = Make (Sine_Yaw * Cosine_Pitch, -Sine_Pitch, -Cosine_Yaw * Cosine_Pitch);
  vec3 Right   = Normalize (Cross (Forward, Make (0, 1, 0)));
  vec3 Up      = Cross (Right, Forward);

  // Populate the rotation portion of the view matrix (transposed basis)
  mat4 Result = {0};
  Result.E[0]  = Right.x;
  Result.E[4]  = Right.y;
  Result.E[8]  = Right.z;
  Result.E[1]  = Up.x;
  Result.E[5]  = Up.y;
  Result.E[9]  = Up.z;
  Result.E[2]  = -Forward.x;
  Result.E[6]  = -Forward.y;
  Result.E[10] = -Forward.z;

  // Compute the translation component as the negated dot of each basis vector with the position
  Result.E[12] = -(Result.E[0] * Position.x + Result.E[4] * Position.y + Result.E[8]  * Position.z);
  Result.E[13] = -(Result.E[1] * Position.x + Result.E[5] * Position.y + Result.E[9]  * Position.z);
  Result.E[14] = -(Result.E[2] * Position.x + Result.E[6] * Position.y + Result.E[10] * Position.z);
  Result.E[15] = 1;
  return Result;
}

// ══════════════════════
//   Inverse_Orthogonal 
// ══════════════════════

mat4 Inverse_Orthogonal (mat4 Source) {
  mat4 Result = {0};

  // Transpose the upper-left 3×3 rotation block
  for (int Row = 0; Row < 3; Row++)
    for (int Column = 0; Column < 3; Column++)
      Result.E[Row * 4 + Column] = Source.E[Column * 4 + Row];

  // Recompute translation as the negated product of the transposed rotation and the original translation
  Result.E[12] = -(Result.E[0] * Source.E[12] + Result.E[4] * Source.E[13] + Result.E[8]  * Source.E[14]);
  Result.E[13] = -(Result.E[1] * Source.E[12] + Result.E[5] * Source.E[13] + Result.E[9]  * Source.E[14]);
  Result.E[14] = -(Result.E[2] * Source.E[12] + Result.E[6] * Source.E[13] + Result.E[10] * Source.E[14]);
  Result.E[15] = 1;
  return Result;
}

// ══════════════════════
//   Inverse_Projection 
// ══════════════════════

mat4 Inverse_Projection (mat4 Projection) {
  mat4 Result = {0};
  Result.E[0]  = 1.f / Projection.E[0];
  Result.E[5]  = 1.f / Projection.E[5];
  Result.E[11] = 1.f / Projection.E[14];
  Result.E[14] = 1.f / Projection.E[11];
  Result.E[15] = -Projection.E[10] / (Projection.E[11] * Projection.E[14]);
  return Result;
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §5. Memory
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ═════════════════
//   Buffer_Upload 
// ═════════════════

void Buffer_Upload (Gpu_Buffer Destination, const void *Data, uint64_t Size) {
  void *Mapped;
  VK_CHECK (vkMapMemory (Device, Destination.Memory, 0, Size, 0, &Mapped));
  memcpy (Mapped, Data, Size);
  vkUnmapMemory (Device, Destination.Memory);
}

// ════════════════════
//   Find_Memory_Type 
// ════════════════════

uint Find_Memory_Type (uint Type_Bits, VkMemoryPropertyFlags Desired_Properties) {
  VkPhysicalDeviceMemoryProperties Memory_Properties;
  vkGetPhysicalDeviceMemoryProperties (Physical_Device, &Memory_Properties);

  // Test each memory type against the required bits and desired property flags
  for (uint Index = 0; Index < Memory_Properties.memoryTypeCount; Index++) {
    if ((Type_Bits >> Index & 1) and (Memory_Properties.memoryTypes[Index].propertyFlags & Desired_Properties) == Desired_Properties)
      return Index;
  }

  // No matching memory type found (should be unreachable on a conformant driver)
  assert (0 and "no matching memory type");
  return 0;
}

// ═══════════════════
//   Buffer_Allocate 
// ═══════════════════

Gpu_Buffer Buffer_Allocate (uint64_t Size, VkBufferUsageFlags Usage, VkMemoryPropertyFlags Memory_Flags) {
  Gpu_Buffer Result = {.Size = Size};

  // Create the buffer object with the requested size and usage
  VK_CHECK (vkCreateBuffer (/*device      =>*/ Device,
                            /*pCreateInfo =>*/ &(VkBufferCreateInfo){
                              .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                              .size  = Size,
                              .usage = Usage},
                            /*pAllocator  =>*/ NULL,
                            /*pBuffer     =>*/ &Result.Buffer));

  // Query how much memory this buffer actually requires and which memory types are compatible
  VkMemoryRequirements Memory_Requirements;
  vkGetBufferMemoryRequirements (Device, Result.Buffer, &Memory_Requirements);

  // If the buffer needs a device address, pass the device-address allocation flag
  VkMemoryAllocateFlagsInfo Allocate_Flags = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
    .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
  };

  // Allocate device memory from the appropriate heap
  VK_CHECK (vkAllocateMemory (/*device        =>*/ Device,
                              /*pAllocateInfo =>*/ &(VkMemoryAllocateInfo){
                                .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                .pNext           = (Usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &Allocate_Flags : NULL,
                                .allocationSize  = Memory_Requirements.size,
                                .memoryTypeIndex = Find_Memory_Type (Memory_Requirements.memoryTypeBits, Memory_Flags)},
                              /*pAllocator    =>*/ NULL,
                              /*pMemory       =>*/ &Result.Memory));

  // Bind the allocated memory to the buffer
  VK_CHECK (vkBindBufferMemory (Device, Result.Buffer, Result.Memory, 0));

  // Retrieve the 64-bit device address if this buffer will be referenced from shaders
  if (Usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    Result.Address = vkGetBufferDeviceAddress (/*device =>*/ Device,
                                               /*pInfo  =>*/ &(VkBufferDeviceAddressInfo){
                                                 .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                                 .buffer = Result.Buffer});
  return Result;
}

// ═══════════════════════
//   Buffer_Stage_Upload 
// ═══════════════════════

Gpu_Buffer Buffer_Stage_Upload (VkCommandBuffer    Command_Buffer,
                                VkQueue            Queue,
                                const void *Data, uint64_t Size,
                                VkBufferUsageFlags Usage) {

  // Allocate a host-visible staging buffer and fill it with the source data
  Gpu_Buffer Staging = Buffer_Allocate (/*Size         =>*/ Size,
                                        /*Usage        =>*/ VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                          | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  Buffer_Upload (Staging, Data, Size);

  // Allocate the final device-local buffer that shaders will access
  Gpu_Buffer Destination = Buffer_Allocate (/*Size         =>*/ Size,
                                            /*Usage        =>*/ Usage
                                                              | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                                              | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                            /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Record and submit a one-shot command buffer to copy staging to destination
  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (/*commandBuffer =>*/ Command_Buffer,
                                  /*pBeginInfo    =>*/ &(VkCommandBufferBeginInfo){
                                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));

  // Record the copy command from staging to destination
  vkCmdCopyBuffer (Command_Buffer, Staging.Buffer, Destination.Buffer, 1, &(VkBufferCopy){.size = Size});

  // End recording, submit the command buffer, and wait for the transfer to finish
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));
  VK_CHECK (vkQueueSubmit (/*queue       =>*/ Queue,
                           /*submitCount =>*/ 1,
                           /*pSubmits    =>*/ &(VkSubmitInfo){
                             .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .commandBufferCount = 1,
                             .pCommandBuffers    = &Command_Buffer},
                           /*fence       =>*/ VK_NULL_HANDLE));
  VK_CHECK (vkQueueWaitIdle (Queue));

  // Release the temporary staging buffer now that the transfer is complete
  vkDestroyBuffer (Device, Staging.Buffer, NULL);
  vkFreeMemory    (Device, Staging.Memory, NULL);
  return Destination;
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §6. Textures
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ════════════════════════
//   Image_Storage_Create
// ════════════════════════

Gpu_Image Image_Storage_Create (uint Width, uint Height) {
  Gpu_Image Result = {.Format = VK_FORMAT_R8G8B8A8_UNORM};

  // Create the image object with storage and transfer-source usage
  VK_CHECK (vkCreateImage (/*device      =>*/ Device,
                           /*pCreateInfo =>*/ &(VkImageCreateInfo){
                             .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                             .imageType     = VK_IMAGE_TYPE_2D,
                             .format        = Result.Format,
                             .extent        = {Width, Height, 1},
                             .mipLevels     = 1,
                             .arrayLayers   = 1,
                             .samples       = VK_SAMPLE_COUNT_1_BIT,
                             .tiling        = VK_IMAGE_TILING_OPTIMAL,
                             .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                             .usage         = VK_IMAGE_USAGE_STORAGE_BIT
                                            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT},
                           /*pAllocator  =>*/ NULL,
                           /*pImage      =>*/ &Result.Image));

  // Query memory requirements and allocate device-local memory for the image
  VkMemoryRequirements Memory_Requirements;
  vkGetImageMemoryRequirements (Device, Result.Image, &Memory_Requirements);

  // Allocate device-local memory and bind it to the image
  VK_CHECK (vkAllocateMemory (/*device        =>*/ Device,
                              /*pAllocateInfo =>*/ &(VkMemoryAllocateInfo){
                                .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                .allocationSize  = Memory_Requirements.size,
                                .memoryTypeIndex =
                                  Find_Memory_Type (Memory_Requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)},
                              /*pAllocator    =>*/ NULL,
                              /*pMemory       =>*/ &Result.Memory));

  VK_CHECK (vkBindImageMemory (Device, Result.Image, Result.Memory, 0));

  // Create an image view so shaders can reference this image
  VK_CHECK (vkCreateImageView (/*device      =>*/ Device,
                               /*pCreateInfo =>*/ &(VkImageViewCreateInfo){
                                 .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                 .image            = Result.Image,
                                 .viewType         = VK_IMAGE_VIEW_TYPE_2D,
                                 .format           = Result.Format,
                                 .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}},
                               /*pAllocator  =>*/ NULL,
                               /*pView       =>*/ &Result.View));
  return Result;
}

// ════════════════════════
//   Image_Layout_Barrier
// ════════════════════════

void Image_Layout_Barrier (VkCommandBuffer      Command_Buffer, VkImage              Image,
                           VkImageLayout        Old_Layout,     VkImageLayout        New_Layout,
                           VkAccessFlags        Source_Access,  VkAccessFlags        Destination_Access,
                           VkPipelineStageFlags Source_Stage,   VkPipelineStageFlags Destination_Stage) {
  vkCmdPipelineBarrier (/*commandBuffer            =>*/ Command_Buffer,
                        /*srcStageMask             =>*/ Source_Stage,
                        /*dstStageMask             =>*/ Destination_Stage,
                        /*dependencyFlags          =>*/ 0,
                        /*memoryBarrierCount       =>*/ 0,
                        /*pMemoryBarriers          =>*/ NULL,
                        /*bufferMemoryBarrierCount =>*/ 0,
                        /*pBufferMemoryBarriers    =>*/ NULL,
                        /*imageMemoryBarrierCount  =>*/ 1,
                        /*pImageMemoryBarriers     =>*/ &(VkImageMemoryBarrier){
                          .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                          .srcAccessMask       = Source_Access,
                          .dstAccessMask       = Destination_Access,
                          .oldLayout           = Old_Layout,
                          .newLayout           = New_Layout,
                          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                          .image               = Image,
                          .subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }});
}

// ══════════════════
//   Texture_Upload 
// ══════════════════

void Texture_Upload (VkCommandBuffer Command_Buffer, VkQueue Queue,
                     const uint8_t *Pixels, uint Width, uint Height,
                     VkImage *Out_Image, VkDeviceMemory *Out_Memory, VkImageView *Out_View) {
  Texture_Upload_With_Format (/*Command_Buffer  =>*/ Command_Buffer,
                              /*Queue           =>*/ Queue,
                              /*Pixels          =>*/ Pixels,
                              /*Width           =>*/ Width,
                              /*Height          =>*/ Height,
                              /*Format          =>*/ VK_FORMAT_R8G8B8A8_SRGB,
                              /*Out_Image       =>*/ Out_Image,
                              /*Out_Memory      =>*/ Out_Memory,
                              /*Out_View        =>*/ Out_View);
}

// ══════════════════════════════
//   Texture_Upload_With_Format
// ══════════════════════════════

void Texture_Upload_With_Format (VkCommandBuffer Command_Buffer, VkQueue Queue,
                                 const uint8_t *Pixels, uint Width, uint Height, VkFormat Format,
                                 VkImage *Out_Image, VkDeviceMemory *Out_Memory, VkImageView *Out_View) {

  // Create the texture image with sampled and transfer-destination usage
  VkImage Image;
  VK_CHECK (vkCreateImage (/*device      =>*/ Device,
                           /*pCreateInfo =>*/ &(VkImageCreateInfo){
                             .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                             .imageType     = VK_IMAGE_TYPE_2D,
                             .format        = Format,
                             .extent        = {Width, Height, 1},
                             .mipLevels     = 1,
                             .arrayLayers   = 1,
                             .samples       = VK_SAMPLE_COUNT_1_BIT,
                             .tiling        = VK_IMAGE_TILING_OPTIMAL,
                             .usage         = VK_IMAGE_USAGE_SAMPLED_BIT
                                            | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                             .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED},
                           /*pAllocator  =>*/ NULL,
                           /*pImage      =>*/ &Image));

  // Allocate and bind device-local memory for the texture
  VkMemoryRequirements Memory_Requirements;
  vkGetImageMemoryRequirements (Device, Image, &Memory_Requirements);

  VkDeviceMemory Memory;
  VK_CHECK (vkAllocateMemory (/*device        =>*/ Device,
                              /*pAllocateInfo =>*/ &(VkMemoryAllocateInfo){
                                .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                .allocationSize  = Memory_Requirements.size,
                                .memoryTypeIndex = Find_Memory_Type (Memory_Requirements.memoryTypeBits,
                                                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)},
                              /*pAllocator    =>*/ NULL,
                              /*pMemory       =>*/ &Memory));
  VK_CHECK (vkBindImageMemory (Device, Image, Memory, 0));

  // Stage the pixel data through a host-visible buffer
  uint64_t Byte_Size = (uint64_t)Width * Height * 4;
  Gpu_Buffer Staging = Buffer_Allocate (/*Size         =>*/ Byte_Size,
                                        /*Usage        =>*/ VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                          | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  Buffer_Upload (Staging, Pixels, Byte_Size);

  // Record a command buffer that transitions the image and copies the staging data into it
  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (/*commandBuffer =>*/ Command_Buffer,
                                  /*pBeginInfo    =>*/ &(VkCommandBufferBeginInfo){
                                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));

  // Transition from undefined to transfer-destination for the copy
  Image_Layout_Barrier (/*Command_Buffer     =>*/ Command_Buffer,
                        /*Image              =>*/ Image,
                        /*Old_Layout         =>*/ VK_IMAGE_LAYOUT_UNDEFINED,
                        /*New_Layout         =>*/ VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        /*Source_Access      =>*/ 0,
                        /*Destination_Access =>*/ VK_ACCESS_TRANSFER_WRITE_BIT,
                        /*Source_Stage       =>*/ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        /*Destination_Stage  =>*/ VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Copy the staging buffer contents into the image
  vkCmdCopyBufferToImage (/*commandBuffer  =>*/ Command_Buffer,
                          /*srcBuffer      =>*/ Staging.Buffer,
                          /*dstImage       =>*/ Image,
                          /*dstImageLayout =>*/ VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          /*regionCount    =>*/ 1,
                          /*pRegions       =>*/ &(VkBufferImageCopy){
                            .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                            .imageExtent      = {Width, Height, 1}});

  // Transition from transfer-destination to shader-read-only for sampling in ray tracing shaders
  Image_Layout_Barrier (/*Command_Buffer     =>*/ Command_Buffer,
                        /*Image              =>*/ Image,
                        /*Old_Layout         =>*/ VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        /*New_Layout         =>*/ VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        /*Source_Access      =>*/ VK_ACCESS_TRANSFER_WRITE_BIT,
                        /*Destination_Access =>*/ VK_ACCESS_SHADER_READ_BIT,
                        /*Source_Stage       =>*/ VK_PIPELINE_STAGE_TRANSFER_BIT,
                        /*Destination_Stage  =>*/ VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

  // Submit the command buffer and wait for the transfer to complete
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));
  VK_CHECK (vkQueueSubmit (/*queue       =>*/ Queue,
                           /*submitCount =>*/ 1,
                           /*pSubmits    =>*/ &(VkSubmitInfo){
                             .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .commandBufferCount = 1,
                             .pCommandBuffers    = &Command_Buffer},
                           /*fence       =>*/ VK_NULL_HANDLE));
  VK_CHECK (vkQueueWaitIdle (Queue));

  // Release the staging buffer
  vkDestroyBuffer (Device, Staging.Buffer, NULL);
  vkFreeMemory    (Device, Staging.Memory, NULL);

  // Create an image view for shader access
  VkImageView Image_View;
  VK_CHECK (vkCreateImageView (/*device      =>*/ Device,
                               /*pCreateInfo =>*/ &(VkImageViewCreateInfo){
                                 .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                 .image            = Image,
                                 .viewType         = VK_IMAGE_VIEW_TYPE_2D,
                                 .format           = Format,
                                 .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}},
                               /*pAllocator  =>*/ NULL,
                               /*pView       =>*/ &Image_View));

  // Set result output
  *Out_Image  = Image;
  *Out_Memory = Memory;
  *Out_View   = Image_View;
}

// ════════════════════════════
//   Sampler_Create_Repeating 
// ════════════════════════════

VkSampler Sampler_Create_Repeating (void) {
  VkSampler Sampler;
  VK_CHECK (vkCreateSampler (/*device      =>*/ Device,
                             /*pCreateInfo =>*/ &(VkSamplerCreateInfo){
                               .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                               .magFilter    = VK_FILTER_LINEAR,
                               .minFilter    = VK_FILTER_LINEAR,
                               .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                               .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                               .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                               .mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                               .maxLod       = 1.f},
                             /*pAllocator  =>*/ NULL,
                             /*pSampler    =>*/ &Sampler));
  return Sampler;
}

// ═══════════════════════════
//   Sampler_Create_Clamping 
// ═══════════════════════════

VkSampler Sampler_Create_Clamping (void) {
  VkSampler Sampler;
  VK_CHECK (vkCreateSampler (/*device      =>*/ Device,
                             /*pCreateInfo =>*/ &(VkSamplerCreateInfo){
                               .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                               .magFilter    = VK_FILTER_LINEAR,
                               .minFilter    = VK_FILTER_LINEAR,
                               .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                               .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                               .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                               .mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                               .maxLod       = 1.f},
                             /*pAllocator  =>*/ NULL,
                             /*pSampler    =>*/ &Sampler));
  return Sampler;
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §6. Textures
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ════════════
//   TGA_Load 
// ════════════

uint8_t *TGA_Load (const char *Path, uint *Out_Width, uint *Out_Height) {

  // Attempt to open the TGA file for binary reading
  FILE *File = fopen (Path, "rb");
  if (not File) return NULL;

  // Read the entire file into memory
  fseek (File, 0, SEEK_END);
  long Length = ftell (File);
  rewind (File);
  if (Length < 18) {fclose (File); return NULL; }

  // Allocate a buffer and read the entire file contents
  uint8_t *Raw = malloc (Length);
  fread (Raw, 1, Length, File);
  fclose (File);

  // Parse the 18-byte TGA header fields
  uint8_t *Cursor     = Raw;
  uint8_t *End_Cursor = Raw + Length;

  // Extract header: ID length, colormap type, and image type
  uint8_t Id_Length     = Cursor[0];
  uint8_t Colormap_Type = Cursor[1];
  uint8_t Image_Type    = Cursor[2];

  // Extract image dimensions from the header (little-endian 16-bit values)
  uint16_t Image_Width, Image_Height;
  memcpy (&Image_Width,  Cursor + 12, 2);
  memcpy (&Image_Height, Cursor + 14, 2);

  // Extract bits per pixel for decoding dispatch
  uint8_t  Bits_Per_Pixel = Cursor[16];

  // Colormap type is read but not used (we only support true-color and grayscale)
  (void)Colormap_Type;

  // Advance past the 18-byte header and any image identification field
  Cursor += 18 + Id_Length;

  // Reject unsupported image types (only uncompressed and RLE true-color are handled)
  if (Image_Type != 2 and Image_Type != 3 and Image_Type != 10) {
    free (Raw);
    return NULL;
  }

  // Allocate the output RGBA pixel buffer
  uint    Columns     = Image_Width;
  uint    Rows        = Image_Height;
  uint8_t *Output     = malloc (Columns * Rows * 4);
          *Out_Width  = Columns;
          *Out_Height = Rows;

  // Decode uncompressed image data (types 2 and 3), reading rows bottom-to-top
  if (Image_Type == 2 or Image_Type == 3) {
    for (uint Row = Rows; Row-- > 0; ) {
      uint8_t *Destination = Output + Row * Columns * 4;
      for (uint Column = 0; Column < Columns; Column++) {
        if (Cursor >= End_Cursor) break;
        uint8_t Red, Green, Blue, Alpha = 255;
        if (Bits_Per_Pixel == 8) {
          Blue = *Cursor++;
          Red = Green = Blue;
        } else if (Bits_Per_Pixel == 24) {
          Blue  = *Cursor++;
          Green = *Cursor++;
          Red   = *Cursor++;
        } else {
          Blue  = *Cursor++;
          Green = *Cursor++;
          Red   = *Cursor++;
          Alpha = *Cursor++;
        }
        *Destination++ = Red;
        *Destination++ = Green;
        *Destination++ = Blue;
        *Destination++ = Alpha;
      }
    }

  // Decode RLE-compressed image data (type 10)
  } else {
    uint Row = Rows - 1, Column = 0;
    uint8_t *Destination = Output + Row * Columns * 4;
    while (Cursor < End_Cursor and Row < Rows) {

      // Read the RLE packet header: high bit indicates a run-length packet
      uint8_t Header      = *Cursor++;
      uint8_t Pixel_Count = (Header & 0x7F) + 1;

      // Dispatch on packet type: run-length or raw
      if (Header & 0x80) {

        // Run-length packet: one pixel value repeated Pixel_Count times
        uint8_t Blue = 0, Green = 0, Red = 0, Alpha = 255;
        if (Bits_Per_Pixel == 24) {
          Blue = *Cursor++; Green = *Cursor++; Red = *Cursor++;
        } else {
          Blue = *Cursor++; Green = *Cursor++; Red = *Cursor++; Alpha = *Cursor++;
        }
        for (uint8_t Pixel = 0; Pixel < Pixel_Count; Pixel++) {
          *Destination++ = Red;
          *Destination++ = Green;
          *Destination++ = Blue;
          *Destination++ = Alpha;
          if (++Column == Columns) {
            Column = 0;
            if (Row == 0) goto Tga_Done;
            Row--;
            Destination = Output + Row * Columns * 4;
          }
        }

      // Raw packet: Pixel_Count distinct pixel values follow
      } else {
        for (uint8_t Pixel = 0; Pixel < Pixel_Count; Pixel++) {
          uint8_t Blue = 0, Green = 0, Red = 0, Alpha = 255;
          if (Bits_Per_Pixel == 24) {
            Blue = *Cursor++; Green = *Cursor++; Red = *Cursor++;
          } else {
            Blue = *Cursor++; Green = *Cursor++; Red = *Cursor++; Alpha = *Cursor++;
          }
          *Destination++ = Red;
          *Destination++ = Green;
          *Destination++ = Blue;
          *Destination++ = Alpha;
          if (++Column == Columns) {
            Column = 0;
            if (Row == 0) goto Tga_Done;
            Row--;
            Destination = Output + Row * Columns * 4;
          }
        }
      }
    }
    Tga_Done:;
  }

  free (Raw);
  return Output;

} // Tga_Load

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §7. Models
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

void MD3_Parse_Surface (const uint8_t *Surface_Data,
                        Vertex **Inout_Vertices,    uint *Inout_Vertex_Count,
                        uint    **Inout_Indices,     uint *Inout_Index_Count,
                        uint    **Inout_Texture_Ids, uint *Inout_Triangle_Count,
                        uint Assigned_Texture_Index, const float *Transform) {

  // Cast the raw bytes to the surface header structure
  const MD3_Surface *Surface = (const MD3_Surface *)Surface_Data;
  uint Base_Vertex = *Inout_Vertex_Count;

  // Copy triangle indices, offsetting each by the current vertex base
  const int *Triangles = (const int *)(Surface_Data + Surface->Triangles_Offset);
  *Inout_Indices = realloc (*Inout_Indices, sizeof (uint) * (*Inout_Index_Count + Surface->Number_Of_Triangles * 3));
  for (int Index = 0; Index < Surface->Number_Of_Triangles * 3; Index++) {
    (*Inout_Indices)[*Inout_Index_Count + Index] = Base_Vertex + (uint)Triangles[Index];
  }

  // Assign the same texture index to every triangle in this surface
  *Inout_Texture_Ids = realloc (*Inout_Texture_Ids, sizeof (uint) * (*Inout_Triangle_Count + Surface->Number_Of_Triangles));
  for (int Triangle = 0; Triangle < Surface->Number_Of_Triangles; Triangle++) {
    (*Inout_Texture_Ids)[*Inout_Triangle_Count + Triangle] = Assigned_Texture_Index;
  }

  // Decode packed MD3 vertices: int16_t xyz at 1/64 scale, plus spherical normal encoding
  const uint8_t *Vertex_Data               = Surface_Data + Surface->Vertices_Offset;
  const float   *Texture_Coordinate_Data   = (const float *)(Surface_Data + Surface->Texture_Coordinates_Offset);

  // Grow the vertex array and decode each compressed MD3 vertex position and normal
  *Inout_Vertices = realloc (*Inout_Vertices, sizeof (Vertex) * (*Inout_Vertex_Count + Surface->Number_Of_Vertices));

  // Decode each vertex's packed position, spherical normal, and texture coordinates
  for (int Vertex_Index = 0; Vertex_Index < Surface->Number_Of_Vertices; Vertex_Index++) {

    // Unpack the 16-bit position coordinates and scale from MD3's fixed-point representation
    const int16_t *Coordinates = (const int16_t *)(Vertex_Data + Vertex_Index * 8);
    float Position_X = Coordinates[0] / 64.f;
    float Position_Y = Coordinates[1] / 64.f;
    float Position_Z = Coordinates[2] / 64.f;

    // Decode the spherical normal from latitude/longitude byte pair
    uint8_t  Latitude  = Vertex_Data[Vertex_Index * 8 + 6];
    uint8_t  Longitude = Vertex_Data[Vertex_Index * 8 + 7];
    float Latitude_Angle  = Latitude  * (2.f * (float)M_PI / 255.f);
    float Longitude_Angle = Longitude * (2.f * (float)M_PI / 255.f);
    float Normal_X = cosf (Latitude_Angle) * sinf (Longitude_Angle);
    float Normal_Y = sinf (Latitude_Angle) * sinf (Longitude_Angle);
    float Normal_Z = cosf (Longitude_Angle);

    // Optionally apply the tag transform (origin + rotation matrix) to position and normal
    if (Transform) {
      float Origin_X = Transform[0];
      float Origin_Y = Transform[1];
      float Origin_Z = Transform[2];
      float Transformed_X = Transform[3] * Position_X + Transform[6] * Position_Y + Transform[9]  * Position_Z + Origin_X;
      float Transformed_Y = Transform[4] * Position_X + Transform[7] * Position_Y + Transform[10] * Position_Z + Origin_Y;
      float Transformed_Z = Transform[5] * Position_X + Transform[8] * Position_Y + Transform[11] * Position_Z + Origin_Z;
      float Transformed_Normal_X = Transform[3] * Normal_X + Transform[6] * Normal_Y + Transform[9]  * Normal_Z;
      float Transformed_Normal_Y = Transform[4] * Normal_X + Transform[7] * Normal_Y + Transform[10] * Normal_Z;
      float Transformed_Normal_Z = Transform[5] * Normal_X + Transform[8] * Normal_Y + Transform[11] * Normal_Z;
      Position_X = Transformed_X;
      Position_Y = Transformed_Y;
      Position_Z = Transformed_Z;
      Normal_X   = Transformed_Normal_X;
      Normal_Y   = Transformed_Normal_Y;
      Normal_Z   = Transformed_Normal_Z;
    }

    // Read texture coordinates and store the final vertex with Quake3 swizzle applied
    float Texture_U = Texture_Coordinate_Data [Vertex_Index * 2];
    float Texture_V = Texture_Coordinate_Data [Vertex_Index * 2 + 1];

    // Assemble the final vertex with position, texture coordinates, and normal
    (*Inout_Vertices)[*Inout_Vertex_Count + Vertex_Index] = (Vertex){
      .Position   = {Position_X, Position_Z, -Position_Y},
      .Normal     = {Normal_X,   Normal_Z,   -Normal_Y},
      .Texture_Uv = {Texture_U,  Texture_V},
    };
  }

  // Advance the running totals
  *Inout_Vertex_Count   += Surface->Number_Of_Vertices;
  *Inout_Index_Count    += Surface->Number_Of_Triangles * 3;
  *Inout_Triangle_Count += Surface->Number_Of_Triangles;
}

Weapon_Model Weapon_Model_Load () {
  Weapon_Model Result = {0};

  // Open the main weapon body mesh
  FILE *File = fopen ("assets/models/weapons2/machinegun/machinegun.md3", "rb");
  if (not File) {printf ("[weapon] machinegun.md3 not found\n"); return Result; }

  // Read the body file into memory and validate the MD3 magic number
  fseek (File, 0, SEEK_END);
  long File_Size = ftell (File);
  rewind (File);
  uint8_t *Body_Data = malloc (File_Size);
  fread (Body_Data, 1, File_Size, File);
  fclose (File);
  assert (*(uint *)Body_Data == MD3_MAGIC);

  // Read body header fields: surface count, tag count, and their offsets
  int Body_Surface_Count   = *(int *)(Body_Data + 84);
  int Body_Tag_Count       = *(int *)(Body_Data + 80);
  int Body_Tags_Offset     = *(int *)(Body_Data + 96);
  int Body_Surfaces_Offset = *(int *)(Body_Data + 100);

  // Search for the "tag_barrel" attachment point in the body's tag list
  memset (Result.Tag_Barrel, 0, sizeof (Result.Tag_Barrel));
  const MD3_Tag *Body_Tags = (const MD3_Tag *)(Body_Data + Body_Tags_Offset);
  for (int Tag = 0; Tag < Body_Tag_Count; Tag++) {
    if (strncmp (Body_Tags[Tag].Name, "tag_barrel", 64) == 0) {
      memcpy (Result.Tag_Barrel, Body_Tags[Tag].Origin, 3 * sizeof (float));
      memcpy (Result.Tag_Barrel + 3, Body_Tags[Tag].Axis, 9 * sizeof (float));
      break;
    }
  }

  // Iterate over each surface in the body and parse its geometry
  const uint8_t *Surface_Cursor = Body_Data + Body_Surfaces_Offset;
  for (int Surface = 0; Surface < Body_Surface_Count; Surface++) {
    const MD3_Surface *Header = (const MD3_Surface *)Surface_Cursor;
    const char *Shader_Name   = (const char *)(Surface_Cursor + Header->Shaders_Offset);

    // Record the shader name as this surface's texture and parse the geometry
    uint Texture_Index = Result.Surface_Count;
    if (Texture_Index < 3) {
      snprintf (Result.Texture_Names[Texture_Index], 64, "%s", Shader_Name);
      Result.Surface_Count++;
    }

    // Parse the surface geometry into the shared vertex/index/texture arrays
    MD3_Parse_Surface (/*Surface_Data           =>*/ Surface_Cursor,
                       /*Inout_Vertices         =>*/ &Result.Vertices,
                       /*Inout_Vertex_Count     =>*/ &Result.Vertex_Count,
                       /*Inout_Indices          =>*/ &Result.Indices,
                       /*Inout_Index_Count      =>*/ &Result.Index_Count,
                       /*Inout_Texture_Ids      =>*/ &Result.Texture_Ids,
                       /*Inout_Triangle_Count   =>*/ &Result.Triangle_Count,
                       /*Assigned_Texture_Index =>*/ Texture_Index,
                       /*Transform              =>*/ NULL);
    Surface_Cursor += Header->End_Offset;
  }

  // Release the body model data
  free (Body_Data);

  // Load the barrel mesh and pre-transform it by the tag_barrel attachment transform
  File = fopen ("assets/models/weapons2/machinegun/machinegun_barrel.md3", "rb");
  if (File) {
    fseek (File, 0, SEEK_END);
    File_Size = ftell (File);
    rewind (File);
    uint8_t *Barrel_Data = malloc (File_Size);
    fread (Barrel_Data, 1, File_Size, File);
    fclose (File);
    assert (*(uint *)Barrel_Data == MD3_MAGIC);

    // Read the barrel model's surface count and offset
    int Barrel_Surface_Count   = *(int *)(Barrel_Data + 84);
    int Barrel_Surfaces_Offset = *(int *)(Barrel_Data + 100);

    // Parse each barrel surface, applying the tag_barrel attachment transform
    Surface_Cursor = Barrel_Data + Barrel_Surfaces_Offset;
    for (int Surface = 0; Surface < Barrel_Surface_Count; Surface++) {
      MD3_Parse_Surface (/*Surface_Data           =>*/ Surface_Cursor,
                         /*Inout_Vertices         =>*/ &Result.Vertices,
                         /*Inout_Vertex_Count     =>*/ &Result.Vertex_Count,
                         /*Inout_Indices          =>*/ &Result.Indices,
                         /*Inout_Index_Count      =>*/ &Result.Index_Count,
                         /*Inout_Texture_Ids      =>*/ &Result.Texture_Ids,
                         /*Inout_Triangle_Count   =>*/ &Result.Triangle_Count,
                         /*Assigned_Texture_Index =>*/ 0,
                         /*Transform              =>*/ Result.Tag_Barrel);
      Surface_Cursor += ((const MD3_Surface *)Surface_Cursor)->End_Offset;
    }
    free (Barrel_Data);
    printf ("[weapon] barrel merged, tag_barrel=(%.1f,%.1f,%.1f)\n",
            Result.Tag_Barrel[0],
            Result.Tag_Barrel[1],
            Result.Tag_Barrel[2]);
  }

  // Load the hand mesh to extract the tag_weapon animation frames for recoil
  File = fopen ("assets/models/weapons2/machinegun/machinegun_hand.md3", "rb");
  if (File) {
    fseek (File, 0, SEEK_END);
    File_Size = ftell (File);
    rewind (File);
    uint8_t *Hand_Data = malloc (File_Size);
    fread (Hand_Data, 1, File_Size, File);
    fclose (File);
    assert (*(uint *)Hand_Data == MD3_MAGIC);

    // Extract the hand model's frame count and per-frame tag_weapon transforms
    int Hand_Frame_Count = *(int *)(Hand_Data + 76);
    int Hand_Tag_Count   = *(int *)(Hand_Data + 80);
    int Hand_Tags_Offset = *(int *)(Hand_Data + 96);
    Result.Animation_Frame_Count = Hand_Frame_Count < 30 ? Hand_Frame_Count : 30;

    // Extract the origin and axis for tag_weapon at each animation frame
    for (uint Frame = 0; Frame < Result.Animation_Frame_Count; Frame++) {
      const MD3_Tag *Tags = (const MD3_Tag *)(Hand_Data + Hand_Tags_Offset + Frame * Hand_Tag_Count * sizeof (MD3_Tag));
      for (int Tag = 0; Tag < Hand_Tag_Count; Tag++) {
        if (strncmp (Tags[Tag].Name, "tag_weapon", 64) == 0) {
          memcpy (Result.Tag_Weapon[Frame], Tags[Tag].Origin, 3 * sizeof (float));
          memcpy (Result.Tag_Weapon[Frame] + 3, Tags[Tag].Axis, 9 * sizeof (float));
          break;
        }
      }
    }
    free (Hand_Data);
    printf ("[weapon] hand: %u animation frames\n", Result.Animation_Frame_Count);
  }

  // Report the loaded weapon geometry statistics
  printf ("[weapon] loaded: %u vertices, %u triangles, %u surfaces\n",
          Result.Vertex_Count,
          Result.Triangle_Count,
          Result.Surface_Count);

  // Gun is loaded
  return Result;

} // Weapon_Model_Load

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §8. Scene — Setup
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

void Vulkan_Create_Instance () {

  // Gather the instance extensions required by SDL for Vulkan surface presentation
  uint Extension_Count;
  SDL_Vulkan_GetInstanceExtensions (Window, &Extension_Count, NULL);
  const char **Extensions = malloc (sizeof (char *) * (Extension_Count + 1));
  SDL_Vulkan_GetInstanceExtensions (Window, &Extension_Count, Extensions);
  Extensions[Extension_Count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

  // Create the Vulkan instance targeting API version 1.3
  VK_CHECK (vkCreateInstance (/*pCreateInfo =>*/ &(VkInstanceCreateInfo){
                                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                .pApplicationInfo = &(VkApplicationInfo){
                                  .sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                  .pApplicationName = "quake3rt",
                                  .apiVersion       = VK_API_VERSION_1_3},
                                .enabledLayerCount       = 1,
                                .ppEnabledLayerNames     = LAYERS,
                                .enabledExtensionCount   = Extension_Count,
                                .ppEnabledExtensionNames = Extensions},
                              /*pAllocator  =>*/ NULL,
                              /*pInstance   =>*/ &Instance));

  // Release the temporary extensions array now that the instance owns the data
  free (Extensions);

  // Create the platform window surface via SDL's Vulkan integration
  SDL_Vulkan_CreateSurface (Window, Instance, &Surface);
}

void Vulkan_Pick_Physical_Device () {

  // Pick the first available physical device
  uint Device_Count;
  vkEnumeratePhysicalDevices (Instance, &Device_Count, NULL);
  VkPhysicalDevice *Devices = malloc (sizeof (VkPhysicalDevice) * Device_Count);
  vkEnumeratePhysicalDevices (Instance, &Device_Count, Devices);
  Physical_Device = Devices[0];
  free (Devices);

  // Search for a queue family that supports both graphics and surface presentation
  uint Family_Count;
  vkGetPhysicalDeviceQueueFamilyProperties (Physical_Device, &Family_Count, NULL);
  VkQueueFamilyProperties *Families = malloc (sizeof (*Families) * Family_Count);
  vkGetPhysicalDeviceQueueFamilyProperties (Physical_Device, &Family_Count, Families);

  // Select the first queue family that supports both graphics and presentation
  Queue_Family_Index = 0;
  for (uint Index = 0; Index < Family_Count; Index++) {
    VkBool32 Supports_Present;
    vkGetPhysicalDeviceSurfaceSupportKHR (Physical_Device, Index, Surface, &Supports_Present);
    if ((Families[Index].queueFlags & VK_QUEUE_GRAPHICS_BIT) and Supports_Present) {
      Queue_Family_Index = Index;
      break;
    }
  }
  free (Families);
}

void Vulkan_Create_Logical_Device () {

  // Chain together the feature structures for acceleration structure and ray tracing pipeline
  VkPhysicalDeviceAccelerationStructureFeaturesKHR Acceleration_Structure_Features = {
    .sType                  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
    .accelerationStructure  = VK_TRUE};

  // Enable ray tracing pipeline features chained to the acceleration structure features
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR Raytracing_Pipeline_Features = {
    .sType              = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
    .pNext              = &Acceleration_Structure_Features,
    .rayTracingPipeline = VK_TRUE};

  // Enable Vulkan 1.2 features: buffer device address, descriptor indexing with runtime arrays
  VkPhysicalDeviceVulkan12Features Vulkan_12_Features = {
    .sType                                     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    .pNext                                     = &Raytracing_Pipeline_Features,
    .bufferDeviceAddress                       = VK_TRUE,
    .descriptorIndexing                        = VK_TRUE,
    .runtimeDescriptorArray                    = VK_TRUE,
    .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
    .descriptorBindingPartiallyBound           = VK_TRUE,
    .descriptorBindingVariableDescriptorCount  = VK_TRUE};

  // Enable Vulkan 1.3 features: synchronization2 and dynamic rendering
  VkPhysicalDeviceVulkan13Features Vulkan_13_Features = {.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
                                                         .pNext            = &Vulkan_12_Features,
                                                         .synchronization2 = VK_TRUE,
                                                         .dynamicRendering = VK_TRUE};

  // Specify the required device extensions: swapchain, acceleration structure, ray tracing, deferred ops
  const char *Device_Extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                     VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                                     VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                                     VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME};

  // Set queue priority to maximum (1.0) for the single graphics queue
  float Priority = 1.f;

  // Create the logical device with a single graphics queue and all chained features
  VK_CHECK (vkCreateDevice (/*physicalDevice =>*/ Physical_Device,
                            /*pCreateInfo    =>*/ &(VkDeviceCreateInfo){
                              .sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                              .pNext                = &Vulkan_13_Features,
                              .queueCreateInfoCount = 1,
                              .pQueueCreateInfos  = &(VkDeviceQueueCreateInfo){
                                .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                .queueFamilyIndex = Queue_Family_Index,
                                .queueCount       = 1,
                                .pQueuePriorities = &Priority},
                              .enabledExtensionCount   = 4,
                              .ppEnabledExtensionNames = Device_Extensions},
                            /*pAllocator     =>*/ NULL,
                            /*pDevice        =>*/ &Device));

  // Retrieve the queue handle from the newly created device
  vkGetDeviceQueue (Device, Queue_Family_Index, 0, &Queue);

  // Query the physical device's ray tracing pipeline properties for SBT layout
  Raytracing_Properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
  VkPhysicalDeviceProperties2 Device_Properties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                                                   .pNext = &Raytracing_Properties};
  vkGetPhysicalDeviceProperties2 (Physical_Device, &Device_Properties);

  // Load the ray tracing extension function pointers from the logical device
  VULKAN_FUNCTIONS (LOAD_VK)
}

void Vulkan_Create_Swapchain () {
  VkSurfaceCapabilitiesKHR Capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR (Physical_Device, Surface, &Capabilities);
  Swapchain_Extent = Capabilities.currentExtent;
  Swapchain_Format = VK_FORMAT_B8G8R8A8_SRGB;

  // Request one more image than the minimum to avoid stalling on the driver
  uint Image_Count = Capabilities.minImageCount + 1;
  if (Capabilities.maxImageCount and Image_Count > Capabilities.maxImageCount)
    Image_Count = Capabilities.maxImageCount;

  // Create the swapchain with the determined format and present mode
  VK_CHECK (vkCreateSwapchainKHR (/*device      =>*/ Device,
                                  /*pCreateInfo =>*/ &(VkSwapchainCreateInfoKHR){
                                    .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                    .surface          = Surface,
                                    .minImageCount    = Image_Count,
                                    .imageFormat      = Swapchain_Format,
                                    .imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                                    .imageExtent      = Swapchain_Extent,
                                    .imageArrayLayers = 1,
                                    .imageUsage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                    .preTransform     = Capabilities.currentTransform,
                                    .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                    .presentMode      = VK_PRESENT_MODE_FIFO_KHR,
                                    .clipped          = VK_TRUE},
                                  /*pAllocator  =>*/ NULL,
                                  /*pSwapchain  =>*/ &Swapchain));

  // Retrieve the swapchain image handles
  vkGetSwapchainImagesKHR (Device, Swapchain, &Swapchain_Image_Count, NULL);
  vkGetSwapchainImagesKHR (Device, Swapchain, &Swapchain_Image_Count, Swapchain_Images);

} // Vulkan_Create_Swapchain

void Vulkan_Create_Synchronization () {

  // Create a command pool with per-buffer reset capability for our single reusable command buffer
  VK_CHECK (vkCreateCommandPool (/*device       =>*/ Device,
                                 /*pCreateInfo  =>*/ &(VkCommandPoolCreateInfo){
                                   .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                   .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                   .queueFamilyIndex = Queue_Family_Index},
                                 /*pAllocator   =>*/ NULL,
                                 /*pCommandPool =>*/ &Command_Pool));

  // Allocate a single primary-level command buffer for all GPU work
  VK_CHECK (vkAllocateCommandBuffers (/*device          =>*/ Device,
                                      /*pAllocateInfo   =>*/ &(VkCommandBufferAllocateInfo){
                                        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                        .commandPool        = Command_Pool,
                                        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                        .commandBufferCount = 1},
                                      /*pCommandBuffers =>*/ &Command_Buffer));

  // Create a fence (pre-signaled so the first frame doesn't deadlock on vkWaitForFences)
  VK_CHECK (vkCreateFence (/*device      =>*/ Device,
                           /*pCreateInfo =>*/ &(VkFenceCreateInfo){
                             .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                             .flags = VK_FENCE_CREATE_SIGNALED_BIT},
                           /*pAllocator  =>*/ NULL,
                           /*pFence      =>*/ &Fence));

  // Create semaphores for swapchain image acquisition and render completion signaling
  VkSemaphoreCreateInfo Semaphore_Info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
  VK_CHECK (vkCreateSemaphore (Device, &Semaphore_Info, NULL, &Semaphore_Image_Available));
  VK_CHECK (vkCreateSemaphore (Device, &Semaphore_Info, NULL, &Semaphore_Render_Finished));
}

void Vulkan_Transition_Storage_Image () {

  // Begin a one-shot command buffer for the initial layout transition
  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (/*commandBuffer =>*/ Command_Buffer,
                                  /*pBeginInfo    =>*/ &(VkCommandBufferBeginInfo){
                                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));

  // Record and submit a layout transition from undefined to general for storage writes
  Image_Layout_Barrier (/*Command_Buffer     =>*/ Command_Buffer,
                        /*Image              =>*/ Raytracing_Storage_Image.Image,
                        /*Old_Layout         =>*/ VK_IMAGE_LAYOUT_UNDEFINED,
                        /*New_Layout         =>*/ VK_IMAGE_LAYOUT_GENERAL,
                        /*Source_Access      =>*/ 0,
                        /*Destination_Access =>*/ VK_ACCESS_SHADER_WRITE_BIT,
                        /*Source_Stage       =>*/ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        /*Destination_Stage  =>*/ VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

  // Finalize the command buffer recording
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));

  // Submit the transition command and wait for completion before proceeding
  VK_CHECK (vkQueueSubmit (/*queue       =>*/ Queue,
                           /*submitCount =>*/ 1,
                           /*pSubmits    =>*/ &(VkSubmitInfo){
                             .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .commandBufferCount = 1,
                             .pCommandBuffers    = &Command_Buffer},
                           /*fence       =>*/ VK_NULL_HANDLE));
  VK_CHECK (vkQueueWaitIdle (Queue));
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §8. Scene — BSP Loading
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

uint BSP_Tessellate_Patch (const BSP_Vertex *Control_Grid, int Patch_Width, int Patch_Height,
                          Vertex **Inout_Vertices, uint *Inout_Vertex_Count,
                          uint **Inout_Indices, uint *Inout_Index_Count) {

  // Compute how many 3x3 sub-patches exist in the control grid
  int Grid_Columns = (Patch_Width  - 1) / 2;
  int Grid_Rows    = (Patch_Height - 1) / 2;

  // Tessellation resolution: number of subdivisions per sub-patch edge
  int Level  = TESSELLATION_LEVEL;
  int Stride = Level + 1;

  // Compute the total vertices and indices this patch will produce and grow the output arrays
  uint Added_Vertices = (uint)(Grid_Columns * Grid_Rows * Stride * Stride);
  uint Added_Indices  = (uint)(Grid_Columns * Grid_Rows * Level * Level * 6);

  // Grow the output vertex and index arrays to hold the tessellated patch geometry
  *Inout_Vertices = realloc (*Inout_Vertices, sizeof (Vertex) * (*Inout_Vertex_Count + Added_Vertices));
  *Inout_Indices  = realloc (*Inout_Indices,  sizeof (uint)    * (*Inout_Index_Count  + Added_Indices));

  // Track the insertion points for new vertices and indices
  uint Vertex_Base  = *Inout_Vertex_Count;
  uint Index_Cursor = *Inout_Index_Count;

  // Iterate over each 3x3 sub-patch in the control grid
  for (int Patch_Y = 0; Patch_Y < Grid_Rows; Patch_Y++) for (int Patch_X = 0; Patch_X < Grid_Columns; Patch_X++) {
    vec3 Control_Position [3][3];
    vec3 Control_Normal   [3][3];
    vec3 Control_Texture  [3][3];
    vec3 Control_Lightmap [3][3];

    // Extract the 3x3 control points for this sub-patch, applying coordinate swizzle
    for (int Row = 0; Row < 3; Row++)
      for (int Column = 0; Column < 3; Column++) {
        const BSP_Vertex *Vertex_Source = &Control_Grid[(Patch_Y * 2 + Row) * Patch_Width + (Patch_X * 2 + Column)];
        Control_Position[Row][Column]   = Make (Vertex_Source->Position[0],        Vertex_Source->Position[2], -Vertex_Source->Position[1]);
        Control_Normal[Row][Column]     = Make (Vertex_Source->Normal[0],          Vertex_Source->Normal[2],   -Vertex_Source->Normal[1]);
        Control_Texture[Row][Column]    = Make (Vertex_Source->Texture_Coords[0],  Vertex_Source->Texture_Coords[1],  0);
        Control_Lightmap[Row][Column]   = Make (Vertex_Source->Lightmap_Coords[0], Vertex_Source->Lightmap_Coords[1], 0);
      }

    // Compute the base vertex offset for this sub-patch in the output array
    uint Patch_Base = Vertex_Base + (uint)((Patch_Y * Grid_Columns + Patch_X) * Stride * Stride);

    // Evaluate the bi-quadratic Bezier surface at each tessellation grid point
    for (int Vertical = 0; Vertical <= Level; Vertical++) {
      float Parameter_V = (float)Vertical / Level;
      vec3 Row_Position[3], Row_Normal[3], Row_Texture[3], Row_Lightmap[3];

      // First pass: evaluate each row of control points along the V parameter
      for (int Row = 0; Row < 3; Row++) {
        Row_Position[Row] = Bezier_Evaluate (Control_Position [Row][0], Control_Position [Row][1], Control_Position [Row][2], Parameter_V);
        Row_Normal[Row]   = Bezier_Evaluate (Control_Normal   [Row][0], Control_Normal   [Row][1], Control_Normal   [Row][2], Parameter_V);
        Row_Texture[Row]  = Bezier_Evaluate (Control_Texture  [Row][0], Control_Texture  [Row][1], Control_Texture  [Row][2], Parameter_V);
        Row_Lightmap[Row] = Bezier_Evaluate (Control_Lightmap [Row][0], Control_Lightmap [Row][1], Control_Lightmap [Row][2], Parameter_V);
      }

      // Second pass: evaluate the intermediate row results along the U parameter
      for (int Horizontal = 0; Horizontal <= Level; Horizontal++) {
        float Parameter_U = (float)Horizontal / Level;
        vec3 Normal   = Normalize
                          (Bezier_Evaluate (Row_Normal   [0], Row_Normal   [1], Row_Normal   [2], Parameter_U));
        vec3 Position = Bezier_Evaluate    (Row_Position [0], Row_Position [1], Row_Position [2], Parameter_U);
        vec3 Texture  = Bezier_Evaluate    (Row_Texture  [0], Row_Texture  [1], Row_Texture  [2], Parameter_U);
        vec3 Lightmap = Bezier_Evaluate    (Row_Lightmap [0], Row_Lightmap [1], Row_Lightmap [2], Parameter_U);

        // Evaluate the bi-quadratic Bezier surface and store the interpolated vertex
        (*Inout_Vertices)[Patch_Base + Vertical * Stride + Horizontal] = (Vertex){
          .Normal      = {Normal.x,   Normal.y,   Normal.z},
          .Position    = {Position.x, Position.y, Position.z},
          .Texture_Uv  = {Texture.x,  Texture.y},
          .Lightmap_Uv = {Lightmap.x, Lightmap.y},
        };
      }
    }

    // Generate two triangles for each quad in the tessellated grid
    for (int Vertical = 0; Vertical < Level; Vertical++) for (int Horizontal = 0; Horizontal < Level; Horizontal++) {
      uint Index_A = Patch_Base + Vertical * Stride + Horizontal;
      uint Index_B = Index_A + 1;
      uint Index_C = Patch_Base + (Vertical + 1) * Stride + Horizontal;
      uint Index_D = Index_C + 1;
      (*Inout_Indices)[Index_Cursor++] = Index_A;
      (*Inout_Indices)[Index_Cursor++] = Index_C;
      (*Inout_Indices)[Index_Cursor++] = Index_B;
      (*Inout_Indices)[Index_Cursor++] = Index_B;
      (*Inout_Indices)[Index_Cursor++] = Index_C;
      (*Inout_Indices)[Index_Cursor++] = Index_D;
    }
  }

  // Advance the output counts by the number of vertices and indices added
  *Inout_Vertex_Count += Added_Vertices;
  *Inout_Index_Count  += Added_Indices;
  return Added_Indices / 3;

} // BSP_Tessellate_Patch

Scene Scene_Load_From_BSP (const char *Path, Spawn *Out_Spawn, Collision_Map *Out_Collision) {

  // Read the entire BSP file into memory
  FILE *File = fopen (Path, "rb");

  // Abandon loading scenes with invalid paths
  if (not File) {fprintf (stderr, "Cannot open %s\n", Path); exit (1);}

  // Get the file size so we can allocate a buffer, then seek back to the beginning
  fseek (File, 0, SEEK_END); 
  long File_Size = ftell (File);
  rewind (File);              

  // Allocate the buffer and read the file in one go
  uint8_t *File_Data = malloc (File_Size);
  fread (File_Data, 1, File_Size, File);

  // Close the file handle
  fclose (File);

  // Validate the BSP magic number and version
  BSP_Header *Header = (BSP_Header *)(File_Data);
  assert (Header->Magic == BSP_MAGIC and Header->Version == BSP_VERSION);

  // Locate the raw lump data for vertices, indices, faces, and shaders
  BSP_Vertex *Raw_Vertices      = (BSP_Vertex *) (File_Data + Header->Lumps [BSP_VERTICES].Offset);
  BSP_Face   *Raw_Faces         = (BSP_Face *)   (File_Data + Header->Lumps [BSP_FACES]   .Offset);
  BSP_Shader *Raw_Shaders       = (BSP_Shader *) (File_Data + Header->Lumps [BSP_SHADERS] .Offset);
  int        *Raw_Indices       = (int *)        (File_Data + Header->Lumps [BSP_INDICES] .Offset);
  uint        Raw_Vertex_Count  = (uint)          (Header->Lumps [BSP_VERTICES] .Length / sizeof (BSP_Vertex));
  uint        Raw_Face_Count    = (uint)          (Header->Lumps [BSP_FACES]    .Length / sizeof (BSP_Face));
  uint        Raw_Shader_Count  = (uint)          (Header->Lumps [BSP_SHADERS]  .Length / sizeof (BSP_Shader));

  // Build the lightmap atlas by packing all 128x128 lightmap pages into a single texture
  uint8_t *Lightmap_Atlas       = NULL;
  uint Lightmap_Lump_Size   = (uint)Header->Lumps [BSP_LIGHTMAPS].Length;
  uint Lightmap_Page_Count  = Lightmap_Lump_Size / (LIGHTMAP_PAGE_SIZE * LIGHTMAP_PAGE_SIZE * 3);
  uint Total_Pages          = Lightmap_Page_Count + 1;
  uint Atlas_Columns     = 1,    Atlas_Rows       = 1;
  uint Atlas_Width       = 0,    Atlas_Height     = 0;
  float White_Fallback_U = 0.5f, White_Fallback_V = 0.5f;

  // Pack lightmap pages into a grid atlas if any lightmaps exist
  if (Lightmap_Page_Count > 0) {

    // Determine the atlas grid dimensions (square-ish layout)
    while (Atlas_Columns * Atlas_Columns < Total_Pages) Atlas_Columns++;
    Atlas_Rows   = (Total_Pages + Atlas_Columns - 1) / Atlas_Columns;
    Atlas_Width  = Atlas_Columns * LIGHTMAP_PAGE_SIZE;
    Atlas_Height = Atlas_Rows * LIGHTMAP_PAGE_SIZE;
    Lightmap_Atlas = calloc (Atlas_Width * Atlas_Height * 4, 1);

    // Locate the raw lightmap data in the BSP file
    const uint8_t *Lightmap_Data = File_Data + Header->Lumps[BSP_LIGHTMAPS].Offset;

    // Copy each RGB lightmap page into its grid cell, converting RGB to RGBA
    for (uint Page = 0; Page < Lightmap_Page_Count; Page++) {
      uint Column     = Page % Atlas_Columns;
      uint Row        = Page / Atlas_Columns;
      const uint8_t *Source = Lightmap_Data + Page * LIGHTMAP_PAGE_SIZE * LIGHTMAP_PAGE_SIZE * 3;
      for (uint y = 0; y < LIGHTMAP_PAGE_SIZE; y++)
      for (uint x = 0; x < LIGHTMAP_PAGE_SIZE; x++) {
        uint Destination  = ((Row * LIGHTMAP_PAGE_SIZE + y) * Atlas_Width + Column * LIGHTMAP_PAGE_SIZE + x) * 4;
        uint Source_Index = (y * LIGHTMAP_PAGE_SIZE + x) * 3;
        Lightmap_Atlas[Destination]     = Source[Source_Index];
        Lightmap_Atlas[Destination + 1] = Source[Source_Index + 1];
        Lightmap_Atlas[Destination + 2] = Source[Source_Index + 2];
        Lightmap_Atlas[Destination + 3] = 255;
      }
    }

    // Fill an extra all-white page as a fallback for faces with no lightmap
    uint White_Column = Lightmap_Page_Count % Atlas_Columns;
    uint White_Row    = Lightmap_Page_Count / Atlas_Columns;
    for (uint y = 0; y < LIGHTMAP_PAGE_SIZE; y++)
    for (uint x = 0; x < LIGHTMAP_PAGE_SIZE; x++) {
      uint Destination = ((White_Row * LIGHTMAP_PAGE_SIZE + y) * Atlas_Width + White_Column * LIGHTMAP_PAGE_SIZE + x) * 4;
      Lightmap_Atlas[Destination] = Lightmap_Atlas[Destination + 1] = Lightmap_Atlas[Destination + 2] = Lightmap_Atlas[Destination + 3] = 255;
    }
    White_Fallback_U = ((float)White_Column + 0.5f) / (float)Atlas_Columns;
    White_Fallback_V = ((float)White_Row    + 0.5f) / (float)Atlas_Rows;
    printf ("[lightmap] %u pages -> %ux%u atlas (%u columns)\n",
            Lightmap_Page_Count, Atlas_Width, Atlas_Height, Atlas_Columns);
  }

  // Convert all BSP vertices from Z-up to Y-up coordinate system
  uint Vertex_Count = Raw_Vertex_Count;
  Vertex *Vertices = malloc (sizeof (Vertex) * Vertex_Count);
  for (uint Index = 0; Index < Vertex_Count; Index++)
    Vertices[Index] = Convert_BSP_Vertex (&Raw_Vertices[Index]);

  // Allocate growing arrays for assembled indices and per-triangle texture IDs
  uint *Indices = NULL, *Texture_Ids = NULL;
  uint  Index_Count = 0, Triangle_Count = 0;

  // Process each face: planar and mesh faces copy indices directly; patch faces are tessellated
  for (uint Face_Index = 0; Face_Index < Raw_Face_Count; Face_Index++) {
    const BSP_Face *Face = &Raw_Faces[Face_Index];

    // Handle each face type: copy indices for planar/mesh, tessellate for patches
    if (Face->Type == SURFACE_TYPE_PLANAR or Face->Type == SURFACE_TYPE_MESH) {
      uint Face_Triangles = (uint)(Face->Index_Count / 3);

      // Grow the index and texture-id arrays to accommodate this face
      Indices     = realloc (Indices,     sizeof (uint) * (Index_Count    + Face->Index_Count));
      Texture_Ids = realloc (Texture_Ids, sizeof (uint) * (Triangle_Count + Face_Triangles));

      // Copy face indices, offsetting by the face's first vertex
      for (int Loop = 0; Loop < Face->Index_Count; Loop++)
        Indices[Index_Count + Loop] = (uint)(Face->First_Vertex + Raw_Indices[Face->First_Index + Loop]);

      // Assign the face's shader index to each triangle
      for (uint Triangle = 0; Triangle < Face_Triangles; Triangle++)
        Texture_Ids[Triangle_Count + Triangle] = (uint)Face->Shader_Index;

      // Advance the running index and triangle counts
      Index_Count    += Face->Index_Count;
      Triangle_Count += Face_Triangles;

      // Remap lightmap UVs from per-page to atlas space, or use the white fallback
      if (Face->Lightmap_Index >= 0 and Atlas_Columns > 0) {
        float Column_Offset = (float)((uint)Face->Lightmap_Index % Atlas_Columns);
        float Row_Offset    = (float)((uint)Face->Lightmap_Index / Atlas_Columns);
        for (int Vertex_Loop = 0; Vertex_Loop < Face->Vertex_Count; Vertex_Loop++) {
          uint Vertex_Index = (uint)(Face->First_Vertex + Vertex_Loop);
          Vertices[Vertex_Index].Lightmap_Uv[0] = (Column_Offset + Vertices[Vertex_Index].Lightmap_Uv[0]) / (float)Atlas_Columns;
          Vertices[Vertex_Index].Lightmap_Uv[1] = (Row_Offset    + Vertices[Vertex_Index].Lightmap_Uv[1]) / (float)Atlas_Rows;
        }
      } else {
        for (int Vertex_Loop = 0; Vertex_Loop < Face->Vertex_Count; Vertex_Loop++) {
          uint Vertex_Index = (uint)(Face->First_Vertex + Vertex_Loop);
          Vertices[Vertex_Index].Lightmap_Uv[0] = White_Fallback_U;
          Vertices[Vertex_Index].Lightmap_Uv[1] = White_Fallback_V;
        }
      }

    // Tessellate the Bezier patch into triangles
    } else if (Face->Type == SURFACE_TYPE_PATCH) {
      uint Previous_Vertex_Count   = Vertex_Count;
      uint Previous_Triangle_Count = Triangle_Count;

      // Tessellate the Bezier patch and assign shader indices to the new triangles
      Triangle_Count += BSP_Tessellate_Patch (/*Control_Grid       =>*/ &Raw_Vertices[Face->First_Vertex],
                                              /*Patch_Width        =>*/ Face->Patch_Width,
                                              /*Patch_Height       =>*/ Face->Patch_Height,
                                              /*Inout_Vertices     =>*/ &Vertices,
                                              /*Inout_Vertex_Count =>*/ &Vertex_Count,
                                              /*Inout_Indices      =>*/ &Indices,
                                              /*Inout_Index_Count  =>*/ &Index_Count);

      // Assign shader indices to the newly tessellated triangles
      uint Patch_Triangles = Triangle_Count - Previous_Triangle_Count;
      Texture_Ids = realloc (Texture_Ids, sizeof (uint) * Triangle_Count);
      for (uint Triangle = 0; Triangle < Patch_Triangles; Triangle++)
        Texture_Ids[Previous_Triangle_Count + Triangle] = (uint)Face->Shader_Index;

      // Remap lightmap UVs for the tessellated vertices
      if (Face->Lightmap_Index >= 0 and Atlas_Columns > 0) {
        float Column_Offset = (float)((uint)Face->Lightmap_Index % Atlas_Columns);
        float Row_Offset    = (float)((uint)Face->Lightmap_Index / Atlas_Columns);
        for (uint Vertex_Index = Previous_Vertex_Count; Vertex_Index < Vertex_Count; Vertex_Index++) {
          Vertices[Vertex_Index].Lightmap_Uv[0] = (Column_Offset + Vertices[Vertex_Index].Lightmap_Uv[0]) / (float)Atlas_Columns;
          Vertices[Vertex_Index].Lightmap_Uv[1] = (Row_Offset    + Vertices[Vertex_Index].Lightmap_Uv[1]) / (float)Atlas_Rows;
        }
      } else {
        for (uint Vertex_Index = Previous_Vertex_Count; Vertex_Index < Vertex_Count; Vertex_Index++) {
          Vertices[Vertex_Index].Lightmap_Uv[0] = White_Fallback_U;
          Vertices[Vertex_Index].Lightmap_Uv[1] = White_Fallback_V;
        }
      }
    }
  }

  // Build per-material fallback colors by hashing the shader name to a deterministic RGB value
  uint Material_Count = Raw_Shader_Count;
  vec4 *Materials = malloc (sizeof (vec4) * Material_Count);
  char (*Texture_Names)[64] = malloc (sizeof (char[64]) * Material_Count);

  // Hash each shader name to generate a deterministic fallback color
  for (uint Material = 0; Material < Material_Count; Material++) {
    uint Hash = 5381;
    for (int Character = 0; Raw_Shaders[Material].Name[Character]; Character++)
      Hash = Hash * 31 + (uint8_t)Raw_Shaders[Material].Name[Character];
    Materials[Material] = (vec4){0.4f + 0.35f * ((Hash >> 0  & 0xFF) / 255.f),
                                 0.4f + 0.35f * ((Hash >> 8  & 0xFF) / 255.f),
                                 0.4f + 0.35f * ((Hash >> 16 & 0xFF) / 255.f), 1};
    memcpy (Texture_Names[Material], Raw_Shaders[Material].Name, 64);
  }

  // Parse the spawn point from the entity lump
  if (Out_Spawn)
    *Out_Spawn = BSP_Find_Spawn (File_Data, Header);

  // Load collision data from planes, nodes, leafs, brushes, and brush sides lumps
  if (Out_Collision) {
    Collision_Map *Collision = Out_Collision;
    memset (Collision, 0, sizeof (*Collision));

    // Parse planes (16 bytes each: float normal[3], float distance) with coordinate swizzle
    Collision->Plane_Count = (uint)Header->Lumps[BSP_PLANES].Length / 16;
    Collision->Planes      = malloc (sizeof (Collision_Plane) * Collision->Plane_Count);
    for (uint Index = 0; Index < Collision->Plane_Count; Index++) {
      const float *Source = (const float *)(File_Data + Header->Lumps[BSP_PLANES].Offset + Index * 16);

      // BUG FIX: declare and populate the Normal pointer BEFORE using it for Type and Sign_Bits
      float *Normal = Collision->Planes[Index].Normal;
      Normal[0] =  Source[0];
      Normal[1] =  Source[2];
      Normal[2] = -Source[1];
      Collision->Planes[Index].Distance  = Source[3];
      Collision->Planes[Index].Type      = (Normal[0] == 1.f) ? 0 : (Normal[1] == 1.f) ? 1 : (Normal[2] == 1.f) ? 2 : 3;
      Collision->Planes[Index].Sign_Bits = (uint8_t)((Normal[0] < 0) | ((Normal[1] < 0) << 1) | ((Normal[2] < 0) << 2));
    }

    // Parse BSP nodes (36 bytes each: int plane, children[2], mins[3], maxs[3])
    Collision->Node_Count = (uint)Header->Lumps[BSP_NODES].Length / 36;
    Collision->Nodes      = malloc (sizeof (Collision_Node) * Collision->Node_Count);
    for (uint Index = 0; Index < Collision->Node_Count; Index++) {
      const int *Source = (const int *)(File_Data + Header->Lumps[BSP_NODES].Offset + Index * 36);
      Collision->Nodes[Index].Plane_Index = Source[0];
      Collision->Nodes[Index].Children[0] = Source[1];
      Collision->Nodes[Index].Children[1] = Source[2];
    }

    // Parse BSP leafs (48 bytes each: cluster, area, mins[3], maxs[3], first_surface, count, first_brush, count)
    Collision->Leaf_Count = (uint)Header->Lumps[BSP_LEAFS].Length / 48;
    Collision->Leafs      = malloc (sizeof (Collision_Leaf) * Collision->Leaf_Count);
    for (uint Index = 0; Index < Collision->Leaf_Count; Index++) {
      const int *Source = (const int *)(File_Data + Header->Lumps[BSP_LEAFS].Offset + Index * 48);
      Collision->Leafs[Index].Cluster       = Source[0];
      Collision->Leafs[Index].Area          = Source[1];
      Collision->Leafs[Index].First_Surface = Source[8];
      Collision->Leafs[Index].Surface_Count = Source[9];
      Collision->Leafs[Index].First_Brush   = Source[10];
      Collision->Leafs[Index].Brush_Count   = Source[11];
    }

    // Copy the leaf brush indirection array verbatim (maps leaf brush ranges to brush indices)
    Collision->Leaf_Brush_Count = (uint)Header->Lumps[BSP_LEAF_BRUSHES].Length / 4;
    Collision->Leaf_Brushes     = malloc (sizeof (int) * Collision->Leaf_Brush_Count);
    memcpy (Collision->Leaf_Brushes, File_Data + Header->Lumps[BSP_LEAF_BRUSHES].Offset, sizeof (int) * Collision->Leaf_Brush_Count);

    // Parse brushes (12 bytes each: int first_side, side_count, shader)
    Collision->Brush_Count = (uint)Header->Lumps[BSP_BRUSHES].Length / 12;
    Collision->Brushes     = malloc (sizeof (Collision_Brush) * Collision->Brush_Count);
    for (uint Index = 0; Index < Collision->Brush_Count; Index++) {
      const int *Source = (const int *)(File_Data + Header->Lumps[BSP_BRUSHES].Offset + Index * 12);
      Collision->Brushes[Index].First_Side   = Source[0];
      Collision->Brushes[Index].Side_Count   = Source[1];
      Collision->Brushes[Index].Shader_Index = Source[2];
    }

    // Parse brush sides (8 bytes each: int plane_index, shader_index)
    Collision->Side_Count = (uint)Header->Lumps[BSP_BRUSH_SIDES].Length / 8;
    Collision->Sides      = malloc (sizeof (Collision_Brush_Side) * Collision->Side_Count);
    for (uint Index = 0; Index < Collision->Side_Count; Index++) {
      const int *Source = (const int *)(File_Data + Header->Lumps[BSP_BRUSH_SIDES].Offset + Index * 8);
      Collision->Sides[Index].Plane_Index  = Source[0];
      Collision->Sides[Index].Shader_Index = Source[1];
    }

    // Extract the contents flags from each shader for solid-brush filtering
    Collision->Shader_Contents = malloc (sizeof (int) * Raw_Shader_Count);
    for (uint Shader = 0; Shader < Raw_Shader_Count; Shader++)
      Collision->Shader_Contents[Shader] = Raw_Shaders[Shader].Contents;

    // Allocate the per-brush deduplication check array (prevents testing the same brush twice per trace)
    Collision->Brush_Checks  = calloc (Collision->Brush_Count, sizeof (uint));
    Collision->Check_Counter = 0;

    // Report the collision map statistics
    printf ("[collision] %u planes, %u nodes, %u leafs, %u brushes, %u sides\n",
            Collision->Plane_Count,
            Collision->Node_Count,
            Collision->Leaf_Count,
            Collision->Brush_Count,
            Collision->Side_Count);
  }

  // Release the raw BSP file buffer and return the assembled scene
  free (File_Data);
  printf ("[bsp] %s: %u vertices, %u triangles, %u shaders\n", Path, Vertex_Count, Triangle_Count, Raw_Shader_Count);

  return (Scene){
    .Vertices        = Vertices,       .Vertex_Count    = Vertex_Count,
    .Indices         = Indices,        .Index_Count     = Index_Count,
    .Materials       = Materials,      .Material_Count  = Material_Count,
    .Texture_Ids     = Texture_Ids,    .Texture_Names   = Texture_Names,
    .Lightmap_Atlas  = Lightmap_Atlas, .Lightmap_Width  = Atlas_Width,
    .Lightmap_Height = Atlas_Height,   .Triangle_Count  = Triangle_Count};

} // Scene_Load_From_BSP

Spawn BSP_Find_Spawn (const uint8_t *File_Data, const BSP_Header *Header) {
  const char *Entities = (const char *)(File_Data + Header->Lumps[BSP_ENTITIES].Offset);
  int Length = Header->Lumps[BSP_ENTITIES].Length;
  Spawn Result = {.Origin = {0, 0, 0}, .Angle = 0 };

  // Set up cursor to walk through the entity lump text
  const char *Cursor = Entities;
  const char *End    = Entities + Length;

  // Walk through each entity block delimited by curly braces
  while (Cursor < End) {
    while (Cursor < End and *Cursor != '{') Cursor++;
    if (Cursor >= End) break;
    Cursor++;

    // Initialize per-entity state for tracking spawn candidates
    int    Is_Spawn   = 0;
    vec3   Origin     = {0, 0, 0};
    float  Angle      = 0;
    int    Has_Origin = 0;

    // Parse key-value pairs within this entity
    while (Cursor < End and *Cursor != '}') {
      while (Cursor < End and (*Cursor == ' ' or *Cursor == '\t' or *Cursor == '\n' or *Cursor == '\r'))
        Cursor++;
      if (Cursor >= End or *Cursor == '}') break;

      // Read the quoted key string
      if (*Cursor != '"') {Cursor++; continue; }
      Cursor++;
      const char *Key = Cursor;
      while (Cursor < End and *Cursor != '"') Cursor++;
      int Key_Length = (int)(Cursor - Key);
      if (Cursor < End) Cursor++;

      // Skip leading whitespace before the value string
      while (Cursor < End and (*Cursor == ' ' or *Cursor == '\t')) Cursor++;

      // Read the quoted value string
      if (Cursor >= End or *Cursor != '"') continue;
      Cursor++;
      const char *Value = Cursor;
      while (Cursor < End and *Cursor != '"') Cursor++;
      int Value_Length = (int)(Cursor - Value);
      if (Cursor < End) Cursor++;

      // Check if this entity is a deathmatch spawn point
      if (Key_Length == 9 and memcmp (Key, "classname", 9) == 0
          and Value_Length == 22 and memcmp (Value, "info_player_deathmatch", 22) == 0)
        Is_Spawn = 1;

      // Parse the "origin" key into three floats
      if (Key_Length == 6 and memcmp (Key, "origin", 6) == 0) {
        char Temporary[64];
        int Limit = Value_Length < 63 ? Value_Length : 63;
        memcpy (Temporary, Value, Limit);
        Temporary[Limit] = 0;
        sscanf (Temporary, "%f %f %f", &Origin.x, &Origin.y, &Origin.z);
        Has_Origin = 1;
      }

      // Parse the "angle" key into a facing direction
      if (Key_Length == 5 and memcmp (Key, "angle", 5) == 0) {
        char Temporary[32];
        int Limit = Value_Length < 31 ? Value_Length : 31;
        memcpy (Temporary, Value, Limit);
        Temporary[Limit] = 0;
        sscanf (Temporary, "%f", &Angle);
      }
    }

    // If this entity is a spawn with a valid origin, swizzle and return it
    if (Is_Spawn and Has_Origin) {
      Result.Origin = Make (Origin.x, Origin.z, -Origin.y);
      Result.Angle  = Angle;
      printf ("[bsp] spawn: %.0f %.0f %.0f angle %.0f\n",
              Result.Origin.x, Result.Origin.y, Result.Origin.z, Angle);
      return Result;
    }
    if (Cursor < End) Cursor++;
  }

  printf ("[bsp] no spawn found, using origin\n");
  return Result;

} // BSP_Find_Spawn

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §8. Scene — Texture Loading
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

void Scene_Load_Textures (const Scene *Scene_Data) {
  Texture_Sampler  = Sampler_Create_Repeating ();
  Texture_Count    = Scene_Data->Material_Count;
  Textures_Loaded  = 0;
  Texture_Images   = calloc (Texture_Count, sizeof (VkImage));
  Texture_Memories = calloc (Texture_Count, sizeof (VkDeviceMemory));
  Texture_Views    = calloc (Texture_Count, sizeof (VkImageView));

  // Load each material's TGA texture, or generate a fallback solid-color pixel
  for (uint Index = 0; Index < Texture_Count; Index++) {
    uint Width = 0, Height = 0;
    uint8_t *Pixels = NULL;
    if (Scene_Data->Texture_Names) {
      char Path[256];
      snprintf (Path, sizeof (Path), "assets/%s.tga", Scene_Data->Texture_Names[Index]);
      Pixels = TGA_Load (Path, &Width, &Height);
    }
    if (Pixels and Width and Height) {
      Texture_Upload (Command_Buffer, Queue, Pixels, Width, Height,
                      &Texture_Images[Index], &Texture_Memories[Index], &Texture_Views[Index]);
      free (Pixels);
      Textures_Loaded++;
    } else {
      vec4 Color = Scene_Data->Materials[Index];
      uint8_t Fallback[4] = {(uint8_t)(Color.x * 255), (uint8_t)(Color.y * 255), (uint8_t)(Color.z * 255), 255};
      Texture_Upload (Command_Buffer, Queue, Fallback, 1, 1,
                      &Texture_Images[Index], &Texture_Memories[Index], &Texture_Views[Index]);
    }
  }

  // Upload per-triangle texture IDs as a storage buffer
  Texture_Id_Buffer = Buffer_Stage_Upload (Command_Buffer, Queue,
                                           Scene_Data->Texture_Ids,
                                           sizeof (uint) * Scene_Data->Triangle_Count,
                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

  printf ("[textures] loaded %u/%u textures, %u fallbacks\n",
          Textures_Loaded, Texture_Count, Texture_Count - Textures_Loaded);

  // Upload the lightmap atlas (or a 1x1 white fallback if no lightmaps exist)
  Lightmap_Sampler = Sampler_Create_Clamping ();
  if (Scene_Data->Lightmap_Atlas and Scene_Data->Lightmap_Width and Scene_Data->Lightmap_Height) {
    Texture_Upload_With_Format (Command_Buffer, Queue,
                                Scene_Data->Lightmap_Atlas,
                                Scene_Data->Lightmap_Width, Scene_Data->Lightmap_Height,
                                VK_FORMAT_R8G8B8A8_UNORM,
                                &Lightmap_Image, &Lightmap_Memory, &Lightmap_View);
    printf ("[lightmap] uploaded %ux%u atlas (UNORM)\n", Scene_Data->Lightmap_Width, Scene_Data->Lightmap_Height);
  } else {
    uint8_t White[4] = {255, 255, 255, 255};
    Texture_Upload_With_Format (Command_Buffer, Queue, White, 1, 1,
                                VK_FORMAT_R8G8B8A8_UNORM,
                                &Lightmap_Image, &Lightmap_Memory, &Lightmap_View);
  }
} // Scene_Load_Textures

void Weapon_Load_Textures (Weapon_Instance *Weapon) {

  // Record the starting index in the global texture array for this weapon's textures
  Weapon->Texture_Base_Index = Texture_Count;

  // Grow the global texture arrays to accommodate the weapon textures
  uint New_Total = Texture_Count + Weapon_Texture_Count;
  Texture_Images   = realloc (Texture_Images,   sizeof (VkImage)        * New_Total);
  Texture_Memories = realloc (Texture_Memories,  sizeof (VkDeviceMemory) * New_Total);
  Texture_Views    = realloc (Texture_Views,     sizeof (VkImageView)    * New_Total);

  // Load each weapon texture from TGA, or create a grey fallback pixel
  for (uint Index = 0; Index < Weapon_Texture_Count; Index++) {
    uint Width = 0, Height = 0;
    uint8_t *Pixels = TGA_Load (Weapon_Texture_Paths[Index], &Width, &Height);
    if (Pixels and Width and Height) {
      Texture_Upload (Command_Buffer, Queue, Pixels, Width, Height,
                      &Texture_Images[Texture_Count], &Texture_Memories[Texture_Count], &Texture_Views[Texture_Count]);
      free (Pixels);
      printf ("[weapon] loaded texture %s (%ux%u)\n", Weapon_Texture_Paths[Index], Width, Height);

    // Texture file not found or corrupt — use a neutral grey fallback
    } else {
      uint8_t Fallback[4] = {180, 180, 180, 255};
      Texture_Upload (Command_Buffer, Queue, Fallback, 1, 1,
                      &Texture_Images[Texture_Count], &Texture_Memories[Texture_Count], &Texture_Views[Texture_Count]);
      printf ("[weapon] fallback texture for %s\n", Weapon_Texture_Paths[Index]);
    }
    Texture_Count++;
  }
  printf ("[weapon] textures: base=%u, count=%u\n", Weapon->Texture_Base_Index, Weapon_Texture_Count);
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §10. Acceleration Structures
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

Acceleration_Structure Build_World_Bottom_Level (const Scene *Scene_Data) {

  // Upload scene vertex, index, and material data to device-local GPU buffers
  Vertex_Buffer = Buffer_Stage_Upload (Command_Buffer, Queue,
                                       Scene_Data->Vertices,
                                       sizeof (Vertex) * Scene_Data->Vertex_Count,
                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                     | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                     | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);

  // Upload the index buffer to device-local memory for BLAS construction and shader access
  Index_Buffer = Buffer_Stage_Upload (Command_Buffer, Queue,
                                      Scene_Data->Indices,
                                      sizeof (uint) * Scene_Data->Index_Count,
                                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                    | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                    | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);

  // Allocate a host-visible material buffer for per-surface RGBA tints
  Material_Buffer = Buffer_Allocate (sizeof (vec4) * Scene_Data->Material_Count,
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                   | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                   | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Upload the material color array
  Buffer_Upload (Material_Buffer, Scene_Data->Materials, sizeof (vec4) * Scene_Data->Material_Count);

  // Define the triangle geometry referencing the uploaded vertex and index buffers
  VkAccelerationStructureGeometryKHR Geometry = {
    .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
    .flags        = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.triangles = {
      .sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
      .vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT,
      .vertexData.deviceAddress = Vertex_Buffer.Address,
      .vertexStride             = sizeof (Vertex),
      .maxVertex                = Scene_Data->Vertex_Count - 1,
      .indexType                = VK_INDEX_TYPE_UINT32,
      .indexData.deviceAddress  = Index_Buffer.Address}};

  // Query the required buffer sizes for the acceleration structure and scratch memory
  VkAccelerationStructureBuildGeometryInfoKHR Build_Info = {
    .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
    .geometryCount = 1,
    .pGeometries   = &Geometry};

  // Query required acceleration structure and scratch sizes from the driver
  uint Primitive_Count = Scene_Data->Triangle_Count;
  VkAccelerationStructureBuildSizesInfoKHR Build_Sizes = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
  vkGetAccelerationStructureBuildSizes (Device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &Build_Info, &Primitive_Count, &Build_Sizes);

  // Allocate the acceleration structure buffer and create the BLAS object
  Acceleration_Structure Result = {0};
  Result.Buffer = Buffer_Allocate (Build_Sizes.accelerationStructureSize,
                                   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
                                 | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Create the bottom-level acceleration structure object
  VK_CHECK (vkCreateAccelerationStructure (Device,
                                           &(VkAccelerationStructureCreateInfoKHR){
                                             .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                                             .buffer = Result.Buffer.Buffer,
                                             .size   = Build_Sizes.accelerationStructureSize,
                                             .type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR},
                                           NULL, &Result.Handle));

  // Allocate temporary scratch memory for the build operation
  Gpu_Buffer Scratch = Buffer_Allocate (Build_Sizes.buildScratchSize,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                      | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Finalize the build info with the destination structure and scratch address
  Build_Info.dstAccelerationStructure  = Result.Handle;
  Build_Info.scratchData.deviceAddress = Scratch.Address;

  // Set up the build range covering all triangles in one geometry
  VkAccelerationStructureBuildRangeInfoKHR Range = {.primitiveCount = Primitive_Count};
  const VkAccelerationStructureBuildRangeInfoKHR *Range_Pointer = &Range;

  // Record and submit a one-shot command buffer to build the BLAS
  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (Command_Buffer,
                                  &(VkCommandBufferBeginInfo){
                                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));

  // Record the BLAS build command
  vkCmdBuildAccelerationStructures (Command_Buffer, 1, &Build_Info, &Range_Pointer);
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));
  VK_CHECK (vkQueueSubmit (Queue, 1,
                           &(VkSubmitInfo){
                             .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .commandBufferCount = 1,
                             .pCommandBuffers    = &Command_Buffer},
                           VK_NULL_HANDLE));

  // Wait for the build to complete before querying the device address
  VK_CHECK (vkQueueWaitIdle (Queue));

  // Query the device address of the built BLAS for referencing from the TLAS
  Result.Address = vkGetAccelerationStructureDeviceAddress (Device,
                     &(VkAccelerationStructureDeviceAddressInfoKHR){
                       .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                       .accelerationStructure = Result.Handle});

  // Free the scratch buffer (no longer needed after the build)
  vkDestroyBuffer (Device, Scratch.Buffer, NULL);
  vkFreeMemory    (Device, Scratch.Memory, NULL);
  return Result;

} // Build_World_Bottom_Level

void Weapon_Bottom_Level_Initialize (Weapon_Instance *Weapon) {
  if (not Weapon->Model.Vertex_Count) return;

  // Allocate a host-visible copy of the weapon vertices for per-frame CPU transformation
  Weapon->Transformed_Vertices = malloc (sizeof (Vertex) * Weapon->Model.Vertex_Count);
  memcpy (Weapon->Transformed_Vertices, Weapon->Model.Vertices, sizeof (Vertex) * Weapon->Model.Vertex_Count);

  // Create host-visible vertex buffer for direct CPU writes each frame
  Weapon->Vertex_Buffer = Buffer_Allocate (sizeof (Vertex) * Weapon->Model.Vertex_Count,
                                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                         | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                         | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                                         | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Upload the initial vertex positions
  Buffer_Upload (Weapon->Vertex_Buffer, Weapon->Transformed_Vertices, sizeof (Vertex) * Weapon->Model.Vertex_Count);

  // Upload index and texture-id data (static, device-local)
  Weapon->Index_Buffer      = Buffer_Stage_Upload (Command_Buffer, Queue,
                                                   Weapon->Model.Indices,
                                                   sizeof (uint) * Weapon->Model.Index_Count,
                                                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                                 | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                 | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
  Weapon->Texture_Id_Buffer = Buffer_Stage_Upload (Command_Buffer, Queue,
                                                   Weapon->Model.Texture_Ids,
                                                   sizeof (uint) * Weapon->Model.Triangle_Count,
                                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

  // Configure the BLAS for fast builds with update capability
  VkAccelerationStructureGeometryKHR Geometry = {
    .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
    .flags        = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.triangles = {
      .sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
      .vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT,
      .vertexData.deviceAddress = Weapon->Vertex_Buffer.Address,
      .vertexStride             = sizeof (Vertex),
      .maxVertex                = Weapon->Model.Vertex_Count - 1,
      .indexType                = VK_INDEX_TYPE_UINT32,
      .indexData.deviceAddress  = Weapon->Index_Buffer.Address}};

  // Use FAST_BUILD + ALLOW_UPDATE since the weapon is rebuilt every frame
  VkAccelerationStructureBuildGeometryInfoKHR Build_Info = {
    .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR
                   | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
    .geometryCount = 1,
    .pGeometries   = &Geometry};

  // Query required sizes for the weapon BLAS and its scratch buffer
  uint Primitive_Count = Weapon->Model.Triangle_Count;
  VkAccelerationStructureBuildSizesInfoKHR Build_Sizes = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
  vkGetAccelerationStructureBuildSizes (Device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &Build_Info, &Primitive_Count, &Build_Sizes);

  // Allocate the BLAS buffer and persistent scratch buffer (reused across frames)
  Weapon->Bottom_Level.Buffer  = Buffer_Allocate (Build_Sizes.accelerationStructureSize,
                                                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
                                                | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK (vkCreateAccelerationStructure (Device,
                                           &(VkAccelerationStructureCreateInfoKHR){
                                             .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                                             .buffer = Weapon->Bottom_Level.Buffer.Buffer,
                                             .size   = Build_Sizes.accelerationStructureSize,
                                             .type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR},
                                           NULL, &Weapon->Bottom_Level.Handle));
  Weapon->Bottom_Level_Scratch = Buffer_Allocate (Build_Sizes.buildScratchSize,
                                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Perform the initial BLAS build
  Build_Info.dstAccelerationStructure  = Weapon->Bottom_Level.Handle;
  Build_Info.scratchData.deviceAddress = Weapon->Bottom_Level_Scratch.Address;

  // Build range: all weapon triangles in a single geometry entry
  VkAccelerationStructureBuildRangeInfoKHR Range = {.primitiveCount = Primitive_Count};
  const VkAccelerationStructureBuildRangeInfoKHR *Range_Pointer = &Range;

  // Record and submit the initial weapon BLAS build
  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (Command_Buffer,
                                  &(VkCommandBufferBeginInfo){
                                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));
  vkCmdBuildAccelerationStructures (Command_Buffer, 1, &Build_Info, &Range_Pointer);
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));

  // Submit and wait for the build to complete
  VK_CHECK (vkQueueSubmit (Queue, 1,
                           &(VkSubmitInfo){.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .commandBufferCount = 1, .pCommandBuffers = &Command_Buffer},
                           VK_NULL_HANDLE));
  VK_CHECK (vkQueueWaitIdle (Queue));

  // Query the BLAS device address for TLAS instance referencing
  Weapon->Bottom_Level.Address = vkGetAccelerationStructureDeviceAddress (Device,
    &(VkAccelerationStructureDeviceAddressInfoKHR){
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      .accelerationStructure = Weapon->Bottom_Level.Handle});
  printf ("[weapon] BLAS built: %u triangles\n", Primitive_Count);

} // Weapon_Bottom_Level_Initialize

void Weapon_Bottom_Level_Rebuild (Weapon_Instance *Weapon) {

  // Skip if no weapon geometry is loaded
  if (not Weapon->Model.Vertex_Count) return;

  // Re-upload the CPU-transformed vertices to the host-visible GPU buffer
  Buffer_Upload (Weapon->Vertex_Buffer, Weapon->Transformed_Vertices, sizeof (Vertex) * Weapon->Model.Vertex_Count);

  // Rebuild the BLAS with the updated vertex positions (full rebuild, not update)
  VkAccelerationStructureGeometryKHR Geometry = {
    .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
    .flags        = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.triangles = {
      .sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
      .vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT,
      .vertexData.deviceAddress = Weapon->Vertex_Buffer.Address,
      .vertexStride             = sizeof (Vertex),
      .maxVertex                = Weapon->Model.Vertex_Count - 1,
      .indexType                = VK_INDEX_TYPE_UINT32,
      .indexData.deviceAddress  = Weapon->Index_Buffer.Address}};

  // Configure as a full rebuild (not an update) from the current vertex data
  VkAccelerationStructureBuildGeometryInfoKHR Build_Info = {
    .sType                     = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type                      = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .flags                     = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR
                               | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
    .mode                      = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    .srcAccelerationStructure  = VK_NULL_HANDLE,
    .dstAccelerationStructure  = Weapon->Bottom_Level.Handle,
    .scratchData.deviceAddress = Weapon->Bottom_Level_Scratch.Address,
    .geometryCount             = 1,
    .pGeometries               = &Geometry};

  // Build range covering all weapon triangles
  VkAccelerationStructureBuildRangeInfoKHR Range = {.primitiveCount = Weapon->Model.Triangle_Count};
  const VkAccelerationStructureBuildRangeInfoKHR *Range_Pointer = &Range;

  // Record the rebuild command
  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (Command_Buffer,
                                  &(VkCommandBufferBeginInfo){
                                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));

  // Issue the BLAS rebuild command
  vkCmdBuildAccelerationStructures (Command_Buffer, 1, &Build_Info, &Range_Pointer);

  // Submit and synchronize
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));
  VK_CHECK (vkQueueSubmit (Queue, 1,
                           &(VkSubmitInfo){.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .commandBufferCount = 1, .pCommandBuffers = &Command_Buffer},
                           VK_NULL_HANDLE));
  VK_CHECK (vkQueueWaitIdle (Queue));
}

void Top_Level_Initialize (uint Maximum_Instances) {

  // Allocate a host-visible instance buffer for writing TLAS instance descriptors each frame
  Top_Level_Instance_Buffer = Buffer_Allocate (sizeof (VkAccelerationStructureInstanceKHR) * Maximum_Instances,
                                               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                                             | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                             | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Query the required sizes for the TLAS and its scratch buffer
  VkAccelerationStructureGeometryKHR Geometry = {
    .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
    .flags        = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.instances = {
      .sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
      .arrayOfPointers    = VK_FALSE,
      .data.deviceAddress = Top_Level_Instance_Buffer.Address}};

  // Query TLAS build sizes from the driver
  VkAccelerationStructureBuildGeometryInfoKHR Build_Info = {
    .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR,
    .geometryCount = 1,
    .pGeometries   = &Geometry};
  VkAccelerationStructureBuildSizesInfoKHR Build_Sizes = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
  vkGetAccelerationStructureBuildSizes (Device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &Build_Info, &Maximum_Instances, &Build_Sizes);

  // Allocate the TLAS storage buffer and create the acceleration structure object
  Top_Level.Buffer = Buffer_Allocate (Build_Sizes.accelerationStructureSize,
                                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
                                    | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Create the top-level acceleration structure object
  VK_CHECK (vkCreateAccelerationStructure (Device,
                                           &(VkAccelerationStructureCreateInfoKHR){
                                             .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                                             .buffer = Top_Level.Buffer.Buffer,
                                             .size   = Build_Sizes.accelerationStructureSize,
                                             .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR},
                                           NULL, &Top_Level.Handle));

  // Allocate persistent scratch memory for per-frame TLAS rebuilds
  Top_Level_Scratch_Buffer = Buffer_Allocate (Build_Sizes.buildScratchSize,
                                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Query the TLAS device address for descriptor binding
  Top_Level.Address = vkGetAccelerationStructureDeviceAddress (Device,
    &(VkAccelerationStructureDeviceAddressInfoKHR){
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      .accelerationStructure = Top_Level.Handle});
} // Top_Level_Initialize

void Top_Level_Rebuild (Acceleration_Structure *World, Acceleration_Structure *Weapon) {

  // Zero-initialize the instance descriptors
  VkAccelerationStructureInstanceKHR Instances[2];
  memset (Instances, 0, sizeof (Instances));

  // Instance 0: the world geometry with identity transform, visible to all rays
  Instances[0].transform.matrix[0][0]         = 1.f;
  Instances[0].transform.matrix[1][1]         = 1.f;
  Instances[0].transform.matrix[2][2]         = 1.f;
  Instances[0].mask                           = 0xFF;
  Instances[0].instanceCustomIndex            = 0;
  Instances[0].flags                          = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
  Instances[0].accelerationStructureReference = World->Address;

  uint Instance_Count = 1;

  // Instance 1 (optional): the weapon viewmodel, excluded from shadow rays via mask 0x01
  if (Weapon and Weapon->Handle) {
    Instances[1].transform.matrix[0][0]         = 1.f;
    Instances[1].transform.matrix[1][1]         = 1.f;
    Instances[1].transform.matrix[2][2]         = 1.f;
    Instances[1].mask                           = 0x01;
    Instances[1].instanceCustomIndex            = 1;
    Instances[1].flags                          = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    Instances[1].accelerationStructureReference = Weapon->Address;
    Instance_Count = 2;
  }

  // Upload instance data to the host-visible instance buffer
  Buffer_Upload (Top_Level_Instance_Buffer, Instances, sizeof (VkAccelerationStructureInstanceKHR) * Instance_Count);

  // Set up the TLAS build range and geometry
  VkAccelerationStructureBuildRangeInfoKHR Range = {.primitiveCount = Instance_Count};
  const VkAccelerationStructureBuildRangeInfoKHR *Range_Pointer = &Range;

  // TLAS geometry: references the instance buffer
  VkAccelerationStructureGeometryKHR Geometry = {
    .sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType       = VK_GEOMETRY_TYPE_INSTANCES_KHR,
    .flags              = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.instances = {
      .sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
      .arrayOfPointers    = VK_FALSE,
      .data.deviceAddress = Top_Level_Instance_Buffer.Address}};

  // Configure the TLAS rebuild (full rebuild each frame, not update)
  VkAccelerationStructureBuildGeometryInfoKHR Build_Info = {
    .sType                     = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type                      = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    .flags                     = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR,
    .mode                      = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    .dstAccelerationStructure  = Top_Level.Handle,
    .scratchData.deviceAddress = Top_Level_Scratch_Buffer.Address,
    .geometryCount             = 1,
    .pGeometries               = &Geometry};

  // Record and submit the TLAS rebuild
  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (Command_Buffer,
                                  &(VkCommandBufferBeginInfo){
                                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));
  vkCmdBuildAccelerationStructures (Command_Buffer, 1, &Build_Info, &Range_Pointer);
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));

  // Submit and wait for the TLAS rebuild to complete
  VK_CHECK (vkQueueSubmit (Queue, 1,
                           &(VkSubmitInfo){.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .commandBufferCount = 1, .pCommandBuffers = &Command_Buffer},
                           VK_NULL_HANDLE));
  VK_CHECK (vkQueueWaitIdle (Queue));

} // Top_Level_Rebuild

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §12. Pipeline
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Shader SPIR-V file paths (compiled offline from the glsl blocks above)
#define SHADER_PATH_RAY_GENERATION  "shaders/raygen.spv"
#define SHADER_PATH_CLOSEST_HIT     "shaders/closesthit.spv"
#define SHADER_PATH_PRIMARY_MISS    "shaders/miss.spv"
#define SHADER_PATH_SHADOW_MISS     "shaders/shadow_miss.spv"
#define SHADER_PATH_PHYSICS_COMPUTE "shaders/physics.spv"

void Raytracing_Pipeline_Create () {

  // Define the 12 descriptor bindings for the ray tracing pipeline
  VkDescriptorSetLayoutBinding Bindings[] = {
    {0,  VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,   VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {1,  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1,   VK_SHADER_STAGE_RAYGEN_BIT_KHR,                                        NULL},
    {2,  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1,   VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {3,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1,   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                                   NULL},
    {4,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1,   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                                   NULL},
    {5,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1,   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                                   NULL},
    {6,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1,   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                                   NULL},
    {7,  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1,   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                                   NULL},
    {8,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1,   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                                   NULL},
    {9,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1,   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                                   NULL},
    {10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1,   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                                   NULL},
    {11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     256, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,                                   NULL},
  };

  // The last binding (texture array) uses partially-bound and variable-count flags
  VkDescriptorBindingFlags Binding_Flags[] =
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};

  // Chain the binding flags extension into the descriptor set layout creation
  VkDescriptorSetLayoutBindingFlagsCreateInfo Binding_Flags_Info = {
    .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
    .bindingCount  = 12,
    .pBindingFlags = Binding_Flags};

  // Create the descriptor set layout with all 12 bindings
  VK_CHECK (vkCreateDescriptorSetLayout (Device,
                                         &(VkDescriptorSetLayoutCreateInfo){
                                           .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                           .pNext        = &Binding_Flags_Info,
                                           .bindingCount = 12,
                                           .pBindings    = Bindings},
                                         NULL, &Descriptor_Set_Layout));

  // Create the pipeline layout referencing the single descriptor set
  VK_CHECK (vkCreatePipelineLayout (Device,
                                    &(VkPipelineLayoutCreateInfo){
                                      .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                      .setLayoutCount = 1,
                                      .pSetLayouts    = &Descriptor_Set_Layout},
                                    NULL, &Pipeline_Layout));

  // Load the four SPIR-V shader modules from pre-compiled files
  VkShaderModule Ray_Generation_Module = Shader_Module_Load (SHADER_PATH_RAY_GENERATION);
  VkShaderModule Closest_Hit_Module    = Shader_Module_Load (SHADER_PATH_CLOSEST_HIT);
  VkShaderModule Primary_Miss_Module   = Shader_Module_Load (SHADER_PATH_PRIMARY_MISS);
  VkShaderModule Shadow_Miss_Module    = Shader_Module_Load (SHADER_PATH_SHADOW_MISS);

  // Define the pipeline shader stages (raygen, two miss, one closest-hit)
  VkPipelineShaderStageCreateInfo Stages[] = {
    {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_RAYGEN_BIT_KHR,      Ray_Generation_Module, "main", NULL},
    {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_MISS_BIT_KHR,        Primary_Miss_Module,   "main", NULL},
    {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_MISS_BIT_KHR,        Shadow_Miss_Module,    "main", NULL},
    {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, Closest_Hit_Module,    "main", NULL}};

  // Define the shader groups: raygen (0), primary miss (1), shadow miss (2), hit group (3)
  VkRayTracingShaderGroupCreateInfoKHR Groups[] = {
    {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, NULL, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,                0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR},
    {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, NULL, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,                1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR},
    {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, NULL, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,                2, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR},
    {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, NULL, VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 3, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR}};

  // Create the ray tracing pipeline with recursion depth of 2 (primary + shadow rays)
  VK_CHECK (vkCreateRayTracingPipelines (Device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1,
                                         &(VkRayTracingPipelineCreateInfoKHR){
                                           .sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
                                           .stageCount                   = 4,
                                           .pStages                      = Stages,
                                           .groupCount                   = 4,
                                           .pGroups                      = Groups,
                                           .maxPipelineRayRecursionDepth = 2,
                                           .layout                       = Pipeline_Layout},
                                         NULL, &Pipeline));

  // Destroy the shader modules now that the pipeline owns the compiled code
  vkDestroyShaderModule (Device, Ray_Generation_Module, NULL);
  vkDestroyShaderModule (Device, Closest_Hit_Module,    NULL);
  vkDestroyShaderModule (Device, Primary_Miss_Module,   NULL);
  vkDestroyShaderModule (Device, Shadow_Miss_Module,    NULL);
}

void Shader_Binding_Table_Create () {
  uint Handle_Size      = Raytracing_Properties.shaderGroupHandleSize;
  uint Handle_Alignment = Raytracing_Properties.shaderGroupHandleAlignment;
  uint Base_Alignment   = Raytracing_Properties.shaderGroupBaseAlignment;

  // Compute the stride: aligned handle size, at least as large as the base alignment
  uint Stride = (Handle_Size + Handle_Alignment - 1) & ~(Handle_Alignment - 1);
  if (Stride < Base_Alignment) Stride = Base_Alignment;

  // Retrieve the raw shader group handles from the pipeline
  uint Group_Count = 4;
  uint8_t *Handles = malloc (Handle_Size * Group_Count);
  VK_CHECK (vkGetRayTracingShaderGroupHandles (Device, Pipeline, 0, Group_Count, Handle_Size * Group_Count, Handles));

  // Allocate the SBT buffer and copy each handle at the proper stride offset
  uint Table_Size = Stride * Group_Count;
  Shader_Binding_Table_Buffer = Buffer_Allocate (Table_Size,
                                                 VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR
                                               | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                               | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Map the SBT buffer and copy each group's handle at the aligned stride offset
  uint8_t *Destination;
  vkMapMemory (Device, Shader_Binding_Table_Buffer.Memory, 0, Table_Size, 0, (void **)&Destination);
  for (uint Index = 0; Index < Group_Count; Index++)
    memcpy (Destination + Index * Stride, Handles + Index * Handle_Size, Handle_Size);
  vkUnmapMemory (Device, Shader_Binding_Table_Buffer.Memory);
  free (Handles);

  // Set up the strided device address regions for each shader group type
  VkDeviceAddress Base = Shader_Binding_Table_Buffer.Address;
  Shader_Binding_Ray_Generation = (VkStridedDeviceAddressRegionKHR){Base + 0 * Stride, Stride, Stride};
  Shader_Binding_Miss           = (VkStridedDeviceAddressRegionKHR){Base + 1 * Stride, Stride, Stride * 2};
  Shader_Binding_Hit            = (VkStridedDeviceAddressRegionKHR){Base + 3 * Stride, Stride, Stride};
  Shader_Binding_Callable       = (VkStridedDeviceAddressRegionKHR){0, 0, 0, 0};

} // Shader_Binding_Table_Create

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §12. Pipeline — Descriptors
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

void Descriptor_Set_Create (Weapon_Instance *Weapon) {

  // Allocate a descriptor pool large enough for all binding types
  VkDescriptorPoolSize Pool_Sizes[] = {{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
                                       {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1},
                                       {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1},
                                       {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             7},
                                       {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     257}};
  VK_CHECK (vkCreateDescriptorPool (Device,
                                    &(VkDescriptorPoolCreateInfo){
                                      .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                      .maxSets       = 1,
                                      .poolSizeCount = 5,
                                      .pPoolSizes    = Pool_Sizes},
                                    NULL, &Descriptor_Pool));

  // Allocate the descriptor set with a variable descriptor count for the texture array
  uint Variable_Count = Texture_Count;
  VkDescriptorSetVariableDescriptorCountAllocateInfo Variable_Allocate = {
    .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
    .descriptorSetCount = 1,
    .pDescriptorCounts  = &Variable_Count};
  VK_CHECK (vkAllocateDescriptorSets (Device,
                                      &(VkDescriptorSetAllocateInfo){
                                        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                        .pNext              = &Variable_Allocate,
                                        .descriptorPool     = Descriptor_Pool,
                                        .descriptorSetCount = 1,
                                        .pSetLayouts        = &Descriptor_Set_Layout},
                                      &Descriptor_Set));

  // Prepare descriptor info structures for each binding
  VkWriteDescriptorSetAccelerationStructureKHR Acceleration_Write = {
    .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
    .accelerationStructureCount = 1,
    .pAccelerationStructures    = &Top_Level.Handle};
  VkDescriptorImageInfo  Image_Info             = {.imageView = Raytracing_Storage_Image.View, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorBufferInfo Camera_Info            = {Camera_Uniform_Buffer.Buffer, 0, Camera_Uniform_Buffer.Size};
  VkDescriptorBufferInfo Vertex_Info            = {Vertex_Buffer.Buffer,         0, Vertex_Buffer.Size};
  VkDescriptorBufferInfo Index_Info             = {Index_Buffer.Buffer,          0, Index_Buffer.Size};
  VkDescriptorBufferInfo Material_Info          = {Material_Buffer.Buffer,       0, Material_Buffer.Size};
  VkDescriptorBufferInfo Texture_Id_Info        = {Texture_Id_Buffer.Buffer,     0, Texture_Id_Buffer.Size};
  VkDescriptorBufferInfo Weapon_Vertex_Info     = {Weapon->Vertex_Buffer.Buffer,     0, Weapon->Vertex_Buffer.Size};
  VkDescriptorBufferInfo Weapon_Index_Info      = {Weapon->Index_Buffer.Buffer,      0, Weapon->Index_Buffer.Size};
  VkDescriptorBufferInfo Weapon_Texture_Id_Info = {Weapon->Texture_Id_Buffer.Buffer, 0, Weapon->Texture_Id_Buffer.Size};

  // Build the texture array descriptor info for all loaded textures (world + weapon)
  VkDescriptorImageInfo *Texture_Infos = calloc (Texture_Count, sizeof (VkDescriptorImageInfo));
  for (uint Index = 0; Index < Texture_Count; Index++) {
    Texture_Infos[Index] = (VkDescriptorImageInfo){.sampler     = Texture_Sampler,
                                                   .imageView   = Texture_Views[Index],
                                                   .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  }

  // Lightmap sampler descriptor
  VkDescriptorImageInfo Lightmap_Info = {.sampler     = Lightmap_Sampler,
                                         .imageView   = Lightmap_View,
                                         .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

  // Write all 12 descriptor bindings in one batch
  VkWriteDescriptorSet Writes[] = {
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, &Acceleration_Write, Descriptor_Set, 0,  0, 1,             VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, NULL,           NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 1,  0, 1,                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              &Image_Info,    NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 2,  0, 1,                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             NULL,           &Camera_Info},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 3,  0, 1,                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Vertex_Info},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 4,  0, 1,                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Index_Info},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 5,  0, 1,                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Material_Info},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 6,  0, 1,                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Texture_Id_Info},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 7,  0, 1,                            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     &Lightmap_Info, NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 8,  0, 1,                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Weapon_Vertex_Info},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 9,  0, 1,                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Weapon_Index_Info},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 10, 0, 1,                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Weapon_Texture_Id_Info},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 11, 0, Texture_Count,                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     Texture_Infos,  NULL},
  };

  vkUpdateDescriptorSets (Device, 12, Writes, 0, NULL);
  free (Texture_Infos);

} // Descriptor_Set_Create

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §13. Shaders — Module Loading
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// BUG FIX: the original had TWO definitions of this function merged together (opening the file
// twice, declaring Size twice). This is the single correct version.

VkShaderModule Shader_Module_Load (const char *Path) {

  // Open the SPIR-V binary file
  FILE *File = fopen (Path, "rb");
  if (not File) {fprintf (stderr, "Cannot open shader %s\n", Path); exit (1); }

  // Read the file size and allocate a buffer for the SPIR-V bytecode
  fseek (File, 0, SEEK_END);
  long Size = ftell (File);
  rewind (File);
  uint *Code = malloc (Size);
  fread (Code, 1, Size, File);
  fclose (File);

  // Wrap the raw SPIR-V code in a Vulkan shader module
  VkShaderModule Module;
  VK_CHECK (vkCreateShaderModule (Device,
                                  &(VkShaderModuleCreateInfo){
                                    .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                    .codeSize = (size_t)Size,
                                    .pCode    = Code},
                                  NULL, &Module));

  free (Code);
  return Module;
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §13. Shaders — GLSL Source (compiled offline to SPIR-V)
//
// The four shader stages (Ray_Generation, Closest_Hit, Primary_Miss, Shadow_Miss) and the
// physics compute shader are defined as glsl blocks earlier in the specification section.
// They are compiled to SPIR-V offline using glslangValidator and loaded at runtime via
// Shader_Module_Load from the paths defined by the SHADER_PATH_* macros.
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §12. Pipeline — Camera Upload
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// BUG FIX: local variable `View` shadowed the `View()` function. Renamed to `View_Matrix`.
void Camera_Upload (Camera *State, float Field_Of_View, uint Weapon_Texture_Base) {

  // Build the view and projection matrices from the camera state
  mat4 View_Matrix = View (State->Position, State->Yaw, State->Pitch);
  mat4 Proj_Matrix = Perspective (Field_Of_View, (float)Width / Height, 0.1f, 10000.f);

  // Uniform layout matching the GPU Camera_Uniform block (std140, 160 bytes)
  struct {
    mat4  Inverse_View, Inverse_Projection;
    uint  Frame;
    uint  Weapon_Texture_Base;
    float Padding[2];
  } Uniform;

  // Compute the inverse matrices for reconstructing world-space rays from screen coordinates
  Uniform.Inverse_View        = Inverse_Orthogonal (View_Matrix);
  Uniform.Inverse_Projection  = Inverse_Projection (Proj_Matrix);
  Uniform.Frame               = State->Frame;
  Uniform.Weapon_Texture_Base = Weapon_Texture_Base;

  // Upload the uniform data to the camera buffer
  Buffer_Upload (Camera_Uniform_Buffer, &Uniform, sizeof (Uniform));
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §12. Pipeline — Weapon Update
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

void Weapon_Update (Weapon_Instance *Weapon, const Camera *Camera_Data, float Delta_Time, int Fire) {
  if (not Weapon->Model.Vertex_Count) return;

  // Advance the fire animation state machine: start firing on button press
  if (Fire and not Weapon->Is_Firing) {
    Weapon->Is_Firing = 1;
    Weapon->Fire_Time = 0;
  }

  // Advance the fire timer and stop after 6 animation frames
  if (Weapon->Is_Firing) {
    Weapon->Fire_Time += Delta_Time * 10.f;
    if (Weapon->Fire_Time >= 6.f) {
      Weapon->Is_Firing = 0;
      Weapon->Fire_Time = 0;
    }
  }

  // Advance the idle bob phase accumulator
  Weapon->Bob_Time += Delta_Time;

  // Derive the camera's orthonormal basis from yaw and pitch
  float Cosine_Yaw   = cosf (Camera_Data->Yaw);
  float Sine_Yaw     = sinf (Camera_Data->Yaw);
  float Cosine_Pitch = cosf (Camera_Data->Pitch);
  float Sine_Pitch   = sinf (Camera_Data->Pitch);

  vec3 Forward = Make      (-Sine_Pitch, Cosine_Pitch, -Cosine_Yaw * Cosine_Pitch);
  vec3 Right   = Normalize (Cross (Forward, Make (0, 1, 0)));
  vec3 Up      = Cross     (Right, Forward);

  // Compute the viewmodel offset with idle bob and recoil animations
  float Bob_Vertical   = sinf (Weapon->Bob_Time * 3.5f) * 0.4f;
  float Bob_Horizontal = cosf (Weapon->Bob_Time * 1.7f) * 0.2f;
  float Recoil         = Weapon->Is_Firing ? -1.2f * expf (-Weapon->Fire_Time * 5.f) : 0.f;

  // Final weapon position: camera origin + forward/right/up offsets with bob and recoil
  vec3 Offset = Add (Camera_Data->Position,
                     Add (Scale (Forward, 8.f + Recoil),
                          Add (Scale (Right, 5.f + Bob_Horizontal),
                               Scale (Up,   -5.f + Bob_Vertical))));

  // Select the current animation frame from the hand model's tag_weapon data
  uint Frame_Index = 0;
  if (Weapon->Model.Animation_Frame_Count > 1) {
    if (Weapon->Is_Firing) {
      Frame_Index = (uint)Weapon->Fire_Time;
      if (Frame_Index >= Weapon->Model.Animation_Frame_Count)
        Frame_Index = Weapon->Model.Animation_Frame_Count - 1;
    }
  }

  // Read the tag transform (origin + 3x3 axis matrix) for the current animation frame
  const float *Tag = Weapon->Model.Tag_Weapon[Frame_Index];

  // Swizzle each tag axis from Quake 3 Z-up to Y-up: (x,y,z) becomes (x,z,-y)
  vec3 Axis_0 = (vec3){Tag[3],  Tag[5],  -Tag[4]};
  vec3 Axis_1 = (vec3){Tag[6],  Tag[8],  -Tag[7]};
  vec3 Axis_2 = (vec3){Tag[9],  Tag[11], -Tag[10]};

  // Build the Y-up tag rotation matrix: columns = [forward | up | -left]
  float Tag_Y_Up[9] = {Axis_0.x, Axis_2.x, -Axis_1.x,
                       Axis_0.y, Axis_2.y, -Axis_1.y,
                       Axis_0.z, Axis_2.z, -Axis_1.z};

  // Camera basis matrix (row-major): columns = forward, up, right
  float Camera_Basis[9] = {Forward.x, Up.x, Right.x,
                           Forward.y, Up.y, Right.y,
                           Forward.z, Up.z, Right.z};

  // Combined rotation = Camera_Basis * Tag_Y_Up
  float Rotation[9];
  for (int Row = 0; Row < 3; Row++)
    for (int Column = 0; Column < 3; Column++)
      Rotation[Row * 3 + Column] = Camera_Basis[Row * 3 + 0] * Tag_Y_Up[0 * 3 + Column]
                                 + Camera_Basis[Row * 3 + 1] * Tag_Y_Up[1 * 3 + Column]
                                 + Camera_Basis[Row * 3 + 2] * Tag_Y_Up[2 * 3 + Column];

  // Scale the viewmodel down slightly for a better first-person perspective feel
  float Model_Scale = 0.7f;

  // Transform each vertex from model space to world space
  for (uint Index = 0; Index < Weapon->Model.Vertex_Count; Index++) {
    float Source_X = Weapon->Model.Vertices[Index].Position[0] * Model_Scale;
    float Source_Y = Weapon->Model.Vertices[Index].Position[1] * Model_Scale;
    float Source_Z = Weapon->Model.Vertices[Index].Position[2] * Model_Scale;

    // Apply the combined rotation and translate by the camera offset
    Weapon->Transformed_Vertices[Index].Position[0] = Rotation[0] * Source_X + Rotation[1] * Source_Y + Rotation[2] * Source_Z + Offset.x;
    Weapon->Transformed_Vertices[Index].Position[1] = Rotation[3] * Source_X + Rotation[4] * Source_Y + Rotation[5] * Source_Z + Offset.y;
    Weapon->Transformed_Vertices[Index].Position[2] = Rotation[6] * Source_X + Rotation[7] * Source_Y + Rotation[8] * Source_Z + Offset.z;

    // Rotate the vertex normal by the same 3x3 rotation matrix (no translation)
    float Normal_X = Weapon->Model.Vertices[Index].Normal[0];
    float Normal_Y = Weapon->Model.Vertices[Index].Normal[1];
    float Normal_Z = Weapon->Model.Vertices[Index].Normal[2];
    Weapon->Transformed_Vertices[Index].Normal[0] = Rotation[0] * Normal_X + Rotation[1] * Normal_Y + Rotation[2] * Normal_Z;
    Weapon->Transformed_Vertices[Index].Normal[1] = Rotation[3] * Normal_X + Rotation[4] * Normal_Y + Rotation[5] * Normal_Z;
    Weapon->Transformed_Vertices[Index].Normal[2] = Rotation[6] * Normal_X + Rotation[7] * Normal_Y + Rotation[8] * Normal_Z;

    // Pass texture coordinates through unchanged
    Weapon->Transformed_Vertices[Index].Texture_Uv[0] = Weapon->Model.Vertices[Index].Texture_Uv[0];
    Weapon->Transformed_Vertices[Index].Texture_Uv[1] = Weapon->Model.Vertices[Index].Texture_Uv[1];
  }
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §14. Render — Input
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

Input Poll_Input () {
  Input Input_Data = {0};
  SDL_Event Event;

  // Process all pending SDL events: quit, escape, mouse motion, and mouse clicks
  while (SDL_PollEvent (&Event)) {
    if (Event.type == SDL_QUIT) Quit = 1;
    if (Event.type == SDL_KEYDOWN and Event.key.keysym.sym == SDLK_ESCAPE) Quit = 1;
    if (Event.type == SDL_MOUSEMOTION) {
      Input_Data.Delta_X += Event.motion.xrel;
      Input_Data.Delta_Y += Event.motion.yrel;
    }
    if (Event.type == SDL_MOUSEBUTTONDOWN and Event.button.button == SDL_BUTTON_LEFT)
      Input_Data.Fire = 1;
  }

  // Sample the current keyboard state for movement keys
  const uint8_t *Keyboard = SDL_GetKeyboardState (NULL);
  Input_Data.Forward = Keyboard[SDL_SCANCODE_W]     or Keyboard[SDL_SCANCODE_UP];
  Input_Data.Back    = Keyboard[SDL_SCANCODE_S]     or Keyboard[SDL_SCANCODE_DOWN];
  Input_Data.Left    = Keyboard[SDL_SCANCODE_A]     or Keyboard[SDL_SCANCODE_LEFT];
  Input_Data.Right   = Keyboard[SDL_SCANCODE_D]     or Keyboard[SDL_SCANCODE_RIGHT];
  Input_Data.Jump    = Keyboard[SDL_SCANCODE_SPACE];
  Input_Data.Crouch  = Keyboard[SDL_SCANCODE_LCTRL] or Keyboard[SDL_SCANCODE_C];
  return Input_Data;
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §14. Render — Frame
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

void Raytracing_Frame () {

  // Wait for the previous frame's GPU work to complete
  VK_CHECK (vkWaitForFences (Device, 1, &Fence, VK_TRUE, UINT64_MAX));

  // Acquire the next swapchain image
  uint Image_Index;
  VK_CHECK (vkAcquireNextImageKHR (Device, Swapchain, UINT64_MAX,
                                   Semaphore_Image_Available, VK_NULL_HANDLE, &Image_Index));

  // Reset the fence and begin recording the frame's command buffer
  VK_CHECK (vkResetFences (Device, 1, &Fence));
  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (Command_Buffer,
                                  &(VkCommandBufferBeginInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}));

  // Bind the ray tracing pipeline and descriptor set
  vkCmdBindPipeline       (Command_Buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, Pipeline);
  vkCmdBindDescriptorSets (Command_Buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                           Pipeline_Layout, 0, 1, &Descriptor_Set, 0, NULL);

  // Dispatch ray tracing for every pixel
  vkCmdTraceRays (Command_Buffer,
                  &Shader_Binding_Ray_Generation, &Shader_Binding_Miss,
                  &Shader_Binding_Hit, &Shader_Binding_Callable,
                  Width, Height, 1);

  // Transition the storage image from general to transfer-source for the blit
  Image_Layout_Barrier (Command_Buffer, Raytracing_Storage_Image.Image,
                        VK_IMAGE_LAYOUT_GENERAL,              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_ACCESS_SHADER_WRITE_BIT,           VK_ACCESS_TRANSFER_READ_BIT,
                        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Transition the swapchain image from undefined to transfer-destination
  Image_Layout_Barrier (Command_Buffer, Swapchain_Images[Image_Index],
                        VK_IMAGE_LAYOUT_UNDEFINED,            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        0,                                    VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,    VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Blit the ray tracing result to the swapchain image (with potential scaling)
  vkCmdBlitImage (Command_Buffer,
                  Raytracing_Storage_Image.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  Swapchain_Images[Image_Index],  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  1, &(VkImageBlit){
                    .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                    .srcOffsets[1]  = {Width, Height, 1},
                    .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                    .dstOffsets[1]  = {(int)Swapchain_Extent.width, (int)Swapchain_Extent.height, 1}},
                  VK_FILTER_LINEAR);

  // Transition the storage image back to general for the next frame's writes
  Image_Layout_Barrier (Command_Buffer, Raytracing_Storage_Image.Image,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                        VK_ACCESS_TRANSFER_READ_BIT,          VK_ACCESS_SHADER_WRITE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,       VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

  // Transition the swapchain image to present-source for display
  Image_Layout_Barrier (Command_Buffer, Swapchain_Images[Image_Index],
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_ACCESS_TRANSFER_WRITE_BIT,         0,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

  // Finalize the command buffer recording
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));

  // Submit the command buffer, waiting on image-available and signaling render-finished
  VkPipelineStageFlags Wait_Stage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
  VK_CHECK (vkQueueSubmit (Queue, 1,
                           &(VkSubmitInfo){
                             .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .waitSemaphoreCount   = 1,
                             .pWaitSemaphores      = &Semaphore_Image_Available,
                             .pWaitDstStageMask    = &Wait_Stage,
                             .commandBufferCount   = 1,
                             .pCommandBuffers      = &Command_Buffer,
                             .signalSemaphoreCount = 1,
                             .pSignalSemaphores    = &Semaphore_Render_Finished},
                           Fence));

  // Present the rendered image to the display
  VK_CHECK (vkQueuePresentKHR (Queue,
                               &(VkPresentInfoKHR){
                                 .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                 .waitSemaphoreCount = 1,
                                 .pWaitSemaphores    = &Semaphore_Render_Finished,
                                 .swapchainCount     = 1,
                                 .pSwapchains        = &Swapchain,
                                 .pImageIndices      = &Image_Index}));

  // Drain the queue to serialize frames (simple but not optimal — use multiple frames-in-flight for perf)
  vkQueueWaitIdle (Queue);

} // Raytracing_Frame

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §. Physics
//
//   Six collider shapes, three ray strategies, one grade cascade.
//   See the glsl Physics_Compute block in §13 for the shader source.
//
//   Shapes (each defines a map S² → ℝ³ from unit directions to surface offsets):
//     SPHERE      s(d̂) = d̂ · r                                  Projectiles, pickups
//     CAPSULE     s(d̂) = d̂ · r + (0, sign(d̂.y) · spine, 0)     Player, NPCs
//     AABB        s(d̂) = sign(d̂) ⊙ extents                      Crates, elevators
//     CYLINDER    s(d̂) = (d̂.xz/‖d̂.xz‖ · r, sign(d̂.y) · h, 0)  Barrels, columns
//     ELLIPSOID   s(d̂) = normalize(d̂ ⊘ axes) ⊙ axes · ‖...‖    Vehicles
//     HULL        s(d̂) = argmax(v · d̂) over vertex set          Arbitrary convex models
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
// §11A. GPU Physics Types
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

enum { SHAPE_SPHERE, SHAPE_CAPSULE, SHAPE_AABB, SHAPE_CYLINDER, SHAPE_ELLIPSOID, SHAPE_HULL };

typedef struct {                          // GPU-resident player state, std430, 112 bytes
  float Position     [3]; float Pad_A;
  float Velocity     [3]; float Pad_B;
  float Yaw, Pitch;
  int   On_Ground, Jump_Held;
  float Ground_Normal[3]; float Pad_C;
  int   Ground_Plane, Ducked;
  float View_Height, Stuck_Time;
  float Speed_Last;  int Shape;           // Previous frame speed; active collider shape
  float Extents      [3]; float Pad_D;    // Half-extents / radii / semi-axes (shape-dependent)
  float Spine;       float Pad_E [3];     // Capsule spine half-length (hh - radius), 0 for non-capsules
} Gpu_Player;

typedef struct {                          // Per-frame input via push constants, 48 bytes
  int   Forward, Back, Left, Right;
  int   Jump, Fire, Crouch, Pad;
  float Delta_X, Delta_Y, Dt, Pad2;
} Gpu_Input;

VkPipeline            Physics_Pipeline;
VkPipelineLayout      Physics_Pipeline_Layout;
VkDescriptorSetLayout Physics_Descriptor_Layout;
VkDescriptorPool      Physics_Descriptor_Pool;
VkDescriptorSet       Physics_Descriptor_Set;
Gpu_Buffer            Player_State_Buffer;

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
// §11B. Convex Hulls
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

#define HULL_MAX_VERTS    256 // Per-hull vertex cap (GPU array size)
#define HULL_MAX_ADJ      16  // Max adjacency per vertex (for hill-climb)
#define HULL_MAX_FACES    512 // Quickhull face cap
#define HULL_MAX_ENTITIES 32  // Max simultaneous hull colliders

typedef struct {                    // Single convex hull on CPU
  vec3  Vertices  [HULL_MAX_VERTS]; // Hull vertex positions in local space
  int   Adjacency [HULL_MAX_VERTS][HULL_MAX_ADJ]; // Per-vertex neighbor indices (-1 terminated)
  uint  Vertex_Count;               // Number of hull vertices
  vec3  Centroid;                   // Geometric center (for local-space offset)
  float Bounding_Radius;            // Tight bounding sphere radius from centroid
} Convex_Hull;

typedef struct {                                 // GPU-packed hull data (uploaded to storage buffer)
  float Vertices [HULL_MAX_VERTS][4];            // xyz + padding per vertex (std430 vec4 array)
  int   Adjacency[HULL_MAX_VERTS][HULL_MAX_ADJ]; // Neighbor indices, -1 terminated
  int   Count;                                   // Vertex count
  float Radius;                                  // Bounding sphere radius
  float Centroid [3];                            // Local-space centroid
  int   Pad;
} Gpu_Hull;

Gpu_Buffer Hull_Storage_Buffer; // SSBO holding hull vertex + adjacency data

// ── Quickhull helpers ────────────────────────────────────────────────────────────────────────────

typedef struct { int A, B, C; int Dead; } QH_Face;
typedef struct { int V0, V1, Face; } QH_Edge;

// Signed distance from point P to the plane of triangle (A, B, C) — trivector magnitude
static float QH_Dist (vec3 P, vec3 A, vec3 B, vec3 C) {
  vec3 N = Cross (Subtract (B, A), Subtract (C, A));
  float L = sqrtf (Dot (N, N));
  return L > 1e-8f ? Dot (Subtract (P, A), Scale (N, 1.f / L)) : 0;
}

// ── Quickhull: build a convex hull from a point cloud ────────────────────────────────────────────

Convex_Hull Quickhull (const vec3 *Points, uint Count) {
  Convex_Hull Result = {0};
  if (Count < 4) {
    for (uint i = 0; i < Count and i < HULL_MAX_VERTS; i++) Result.Vertices[Result.Vertex_Count++] = Points[i];
    return Result;
  }

  // Find the 6 extremal points (min/max per axis)
  int Ext [6] = {0,0,0,0,0,0};
  for (uint i = 1; i < Count; i++) {
    if (Points[i].x < Points[Ext[0]].x) Ext[0] = i;
    if (Points[i].x > Points[Ext[1]].x) Ext[1] = i;
    if (Points[i].y < Points[Ext[2]].y) Ext[2] = i;
    if (Points[i].y > Points[Ext[3]].y) Ext[3] = i;
    if (Points[i].z < Points[Ext[4]].z) Ext[4] = i;
    if (Points[i].z > Points[Ext[5]].z) Ext[5] = i;
  }

  // Pick the two most distant extremal points as the initial edge
  int P0 = Ext[0], P1 = Ext[1];
  float Best_Dist = 0;
  for (int i = 0; i < 6; i++) for (int j = i + 1; j < 6; j++) {
    float D = Dot (Subtract (Points[Ext[i]], Points[Ext[j]]), Subtract (Points[Ext[i]], Points[Ext[j]]));
    if (D > Best_Dist) { Best_Dist = D; P0 = Ext[i]; P1 = Ext[j]; }
  }

  // Find the point most distant from the initial edge
  vec3 Edge = Subtract (Points[P1], Points[P0]);
  float Edge_Len2 = Dot (Edge, Edge);
  int P2 = -1; Best_Dist = 0;
  for (uint i = 0; i < Count; i++) {
    if ((int)i == P0 or (int)i == P1) continue;
    vec3 V = Subtract (Points[i], Points[P0]);
    float T = Dot (V, Edge) / Edge_Len2;
    vec3 Proj = Subtract (V, Scale (Edge, T));
    float D = Dot (Proj, Proj);
    if (D > Best_Dist) { Best_Dist = D; P2 = i; }
  }
  if (P2 < 0) P2 = (P0 + 1) % Count;

  // Find the point most distant from the initial triangle
  int P3 = -1; Best_Dist = 0;
  for (uint i = 0; i < Count; i++) {
    if ((int)i == P0 or (int)i == P1 or (int)i == P2) continue;
    float D = fabsf (QH_Dist (Points[i], Points[P0], Points[P1], Points[P2]));
    if (D > Best_Dist) { Best_Dist = D; P3 = i; }
  }
  if (P3 < 0) P3 = (P2 + 1) % Count;

  // Orient the initial tetrahedron so all face normals point outward
  if (QH_Dist (Points[P3], Points[P0], Points[P1], Points[P2]) > 0) { int T = P0; P0 = P1; P1 = T; }

  // Build the initial 4 faces of the tetrahedron
  QH_Face Faces [HULL_MAX_FACES];
  int Face_Count = 0;
  Faces[Face_Count++] = (QH_Face){P0, P1, P2, 0};
  Faces[Face_Count++] = (QH_Face){P0, P2, P3, 0};
  Faces[Face_Count++] = (QH_Face){P0, P3, P1, 0};
  Faces[Face_Count++] = (QH_Face){P1, P3, P2, 0};

  // Assign each remaining point to the face it is most above
  int *Assigned = calloc (Count, sizeof (int));
  for (uint i = 0; i < Count; i++) Assigned[i] = -1;
  for (uint i = 0; i < Count; i++) {
    if ((int)i == P0 or (int)i == P1 or (int)i == P2 or (int)i == P3) continue;
    float Best = 0;
    for (int f = 0; f < Face_Count; f++) {
      if (Faces[f].Dead) continue;
      float D = QH_Dist (Points[i], Points[Faces[f].A], Points[Faces[f].B], Points[Faces[f].C]);
      if (D > Best) { Best = D; Assigned[i] = f; }
    }
  }

  // Iterative hull expansion: find the most-distant conflict point and expand the hull
  for (int Iteration = 0; Iteration < (int)Count and Face_Count < HULL_MAX_FACES - 20; Iteration++) {
    int Best_Face = -1, Best_Point = -1;
    Best_Dist = 0;
    for (uint i = 0; i < Count; i++) {
      if (Assigned[i] < 0 or Faces[Assigned[i]].Dead) continue;
      int f = Assigned[i];
      float D = QH_Dist (Points[i], Points[Faces[f].A], Points[Faces[f].B], Points[Faces[f].C]);
      if (D > Best_Dist) { Best_Dist = D; Best_Face = f; Best_Point = i; }
    }
    if (Best_Point < 0) break;

    // Mark all faces visible from the conflict point
    int Visible [HULL_MAX_FACES]; int Visible_Count = 0;
    for (int f = 0; f < Face_Count; f++) {
      if (Faces[f].Dead) continue;
      if (QH_Dist (Points[Best_Point], Points[Faces[f].A], Points[Faces[f].B], Points[Faces[f].C]) > 1e-6f)
        Visible[Visible_Count++] = f;
    }

    // Find the horizon ridge: edges between visible and non-visible faces
    QH_Edge Horizon [HULL_MAX_FACES * 3]; int Horizon_Count = 0;
    for (int vi = 0; vi < Visible_Count; vi++) {
      int f = Visible[vi];
      int Tri [3][2] = {{Faces[f].A, Faces[f].B}, {Faces[f].B, Faces[f].C}, {Faces[f].C, Faces[f].A}};
      for (int e = 0; e < 3; e++) {
        int Shared = 0;
        for (int vj = 0; vj < Visible_Count; vj++) {
          if (vj == vi) continue;
          int g = Visible[vj];
          int Fv [3] = {Faces[g].A, Faces[g].B, Faces[g].C};
          int Has0 = 0, Has1 = 0;
          for (int k = 0; k < 3; k++) { Has0 |= (Fv[k] == Tri[e][0]); Has1 |= (Fv[k] == Tri[e][1]); }
          if (Has0 and Has1) { Shared = 1; break; }
        }
        if (not Shared) Horizon[Horizon_Count++] = (QH_Edge){Tri[e][0], Tri[e][1], f};
      }
    }

    // Kill visible faces and create new faces from the horizon to the conflict point
    for (int vi = 0; vi < Visible_Count; vi++) Faces[Visible[vi]].Dead = 1;
    int New_Start = Face_Count;
    for (int hi = 0; hi < Horizon_Count and Face_Count < HULL_MAX_FACES; hi++)
      Faces[Face_Count++] = (QH_Face){Horizon[hi].V1, Horizon[hi].V0, Best_Point, 0};

    // Reassign orphaned points to new faces
    Assigned[Best_Point] = -1;
    for (uint i = 0; i < Count; i++) {
      if (Assigned[i] < 0) continue;
      if (not Faces[Assigned[i]].Dead) continue;
      Assigned[i] = -1;
      float Best = 0;
      for (int f = New_Start; f < Face_Count; f++) {
        float D = QH_Dist (Points[i], Points[Faces[f].A], Points[Faces[f].B], Points[Faces[f].C]);
        if (D > Best) { Best = D; Assigned[i] = f; }
      }
    }
  }
  free (Assigned);

  // Extract unique vertices from surviving faces
  int Remap [HULL_MAX_FACES * 3]; memset (Remap, -1, sizeof (Remap));
  for (int f = 0; f < Face_Count; f++) {
    if (Faces[f].Dead) continue;
    int Tri [3] = {Faces[f].A, Faces[f].B, Faces[f].C};
    for (int k = 0; k < 3; k++) {
      if (Tri[k] >= 0 and Tri[k] < (int)Count and Remap[Tri[k]] < 0) {
        if (Result.Vertex_Count < HULL_MAX_VERTS) {
          Remap[Tri[k]] = (int)Result.Vertex_Count;
          Result.Vertices[Result.Vertex_Count++] = Points[Tri[k]];
        }
      }
    }
  }

  // Build adjacency from face connectivity
  memset (Result.Adjacency, -1, sizeof (Result.Adjacency));
  for (int f = 0; f < Face_Count; f++) {
    if (Faces[f].Dead) continue;
    int Rv [3] = {Remap[Faces[f].A], Remap[Faces[f].B], Remap[Faces[f].C]};
    for (int e = 0; e < 3; e++) {
      int V0 = Rv[e], V1 = Rv[(e + 1) % 3];
      if (V0 < 0 or V1 < 0) continue;
      for (int a = 0; a < HULL_MAX_ADJ; a++) { if (Result.Adjacency[V0][a] == V1) break; if (Result.Adjacency[V0][a] == -1) { Result.Adjacency[V0][a] = V1; break; } }
      for (int a = 0; a < HULL_MAX_ADJ; a++) { if (Result.Adjacency[V1][a] == V0) break; if (Result.Adjacency[V1][a] == -1) { Result.Adjacency[V1][a] = V0; break; } }
    }
  }

  // Compute centroid and bounding radius
  Result.Centroid = Make (0, 0, 0);
  for (uint i = 0; i < Result.Vertex_Count; i++) Result.Centroid = Add (Result.Centroid, Result.Vertices[i]);
  if (Result.Vertex_Count) Result.Centroid = Scale (Result.Centroid, 1.f / Result.Vertex_Count);
  Result.Bounding_Radius = 0;
  for (uint i = 0; i < Result.Vertex_Count; i++) {
    float D = Dot (Subtract (Result.Vertices[i], Result.Centroid), Subtract (Result.Vertices[i], Result.Centroid));
    if (D > Result.Bounding_Radius * Result.Bounding_Radius) Result.Bounding_Radius = sqrtf (D);
  }
  printf ("[hull] %u vertices, radius %.1f\n", Result.Vertex_Count, Result.Bounding_Radius);
  return Result;
}

Convex_Hull Hull_From_Vertices (const Vertex *Verts, uint Count) {
  vec3 *Points = malloc (sizeof (vec3) * Count);
  for (uint i = 0; i < Count; i++) Points[i] = Make (Verts[i].Position[0], Verts[i].Position[1], Verts[i].Position[2]);
  Convex_Hull H = Quickhull (Points, Count);
  free (Points);
  return H;
}

void Hull_Upload (const Convex_Hull *Hull) {
  Gpu_Hull Packed = {0};
  Packed.Count  = (int)Hull->Vertex_Count;
  Packed.Radius = Hull->Bounding_Radius;
  Packed.Centroid[0] = Hull->Centroid.x;
  Packed.Centroid[1] = Hull->Centroid.y;
  Packed.Centroid[2] = Hull->Centroid.z;
  for (uint i = 0; i < Hull->Vertex_Count; i++) {
    Packed.Vertices[i][0] = Hull->Vertices[i].x;
    Packed.Vertices[i][1] = Hull->Vertices[i].y;
    Packed.Vertices[i][2] = Hull->Vertices[i].z;
    Packed.Vertices[i][3] = 0;
    memcpy (Packed.Adjacency[i], Hull->Adjacency[i], sizeof (int) * HULL_MAX_ADJ);
  }
  if (not Hull_Storage_Buffer.Buffer) {
    Hull_Storage_Buffer = Buffer_Allocate (sizeof (Gpu_Hull),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  }
  Buffer_Upload (Hull_Storage_Buffer, &Packed, sizeof (Packed));
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
// §11C. GPU Physics — Pipeline and Dispatch (with hull binding)
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

void Physics_Pipeline_Create () {
  VkDescriptorSetLayoutBinding B [] = {
    {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},
    {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},  // Vertex buffer
    {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},  // Index buffer
    {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},  // Player state
    {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},  // Hull data
  };

  VK_CHECK (vkCreateDescriptorSetLayout (Device,
    &(VkDescriptorSetLayoutCreateInfo){.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 5, .pBindings = B}, NULL, &Physics_Descriptor_Layout));

  VkPushConstantRange Push = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof (Gpu_Input)};
  VK_CHECK (vkCreatePipelineLayout (Device,
    &(VkPipelineLayoutCreateInfo){.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1, .pSetLayouts = &Physics_Descriptor_Layout,
      .pushConstantRangeCount = 1, .pPushConstantRanges = &Push}, NULL, &Physics_Pipeline_Layout));

  VkShaderModule M = Shader_Module_Load (SHADER_PATH_PHYSICS_COMPUTE);
  VK_CHECK (vkCreateComputePipelines (Device, VK_NULL_HANDLE, 1,
    &(VkComputePipelineCreateInfo){.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                NULL, 0, VK_SHADER_STAGE_COMPUTE_BIT, M, "main", NULL},
      .layout = Physics_Pipeline_Layout}, NULL, &Physics_Pipeline));
  vkDestroyShaderModule (Device, M, NULL);
}

void Physics_Resources_Create (const Player *Init) {
  Player_State_Buffer = Buffer_Allocate (sizeof (Gpu_Player),
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  Gpu_Player S = {
    .Position = {Init->Position.x, Init->Position.y, Init->Position.z},
    .Yaw = Init->Yaw, .Pitch = Init->Pitch, .View_Height = DEFAULT_VIEW_HEIGHT,
    .Shape = SHAPE_CAPSULE, .Extents = {15, 32, 15}, .Spine = 17};
  Buffer_Upload (Player_State_Buffer, &S, sizeof (S));

  // Allocate a dummy hull buffer (replaced when a hull is actually loaded)
  if (not Hull_Storage_Buffer.Buffer) {
    Hull_Storage_Buffer = Buffer_Allocate (sizeof (Gpu_Hull),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    Gpu_Hull Empty = {0}; Empty.Count = 1;
    Empty.Vertices[0][0] = 0; Empty.Vertices[0][1] = 0; Empty.Vertices[0][2] = 0;
    Buffer_Upload (Hull_Storage_Buffer, &Empty, sizeof (Empty));
  }

  VkDescriptorPoolSize Sz [] = {{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
                                 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4}};
  VK_CHECK (vkCreateDescriptorPool (Device,
    &(VkDescriptorPoolCreateInfo){.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 1, .poolSizeCount = 2, .pPoolSizes = Sz}, NULL, &Physics_Descriptor_Pool));

  VK_CHECK (vkAllocateDescriptorSets (Device,
    &(VkDescriptorSetAllocateInfo){.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = Physics_Descriptor_Pool, .descriptorSetCount = 1,
      .pSetLayouts = &Physics_Descriptor_Layout}, &Physics_Descriptor_Set));

  VkWriteDescriptorSetAccelerationStructureKHR As = {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
    .accelerationStructureCount = 1, .pAccelerationStructures = &Top_Level.Handle};
  VkDescriptorBufferInfo Vb = {Vertex_Buffer.Buffer, 0, Vertex_Buffer.Size};
  VkDescriptorBufferInfo Ib = {Index_Buffer.Buffer,  0, Index_Buffer.Size};
  VkDescriptorBufferInfo Pb = {Player_State_Buffer.Buffer, 0, Player_State_Buffer.Size};
  VkDescriptorBufferInfo Hb = {Hull_Storage_Buffer.Buffer, 0, Hull_Storage_Buffer.Size};

  VkWriteDescriptorSet W [] = {
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, &As,  Physics_Descriptor_Set, 0, 0, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, NULL, NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Physics_Descriptor_Set, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &Vb},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Physics_Descriptor_Set, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &Ib},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Physics_Descriptor_Set, 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &Pb},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Physics_Descriptor_Set, 4, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &Hb},
  };
  vkUpdateDescriptorSets (Device, 5, W, 0, NULL);
}

Player Physics_Dispatch (Input In, float Dt) {
  Gpu_Input G = {In.Forward, In.Back, In.Left, In.Right,
                 In.Jump, In.Fire, In.Crouch, 0,
                 In.Delta_X, In.Delta_Y, Dt, 0};

  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (Command_Buffer,
    &(VkCommandBufferBeginInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));

  vkCmdBindPipeline       (Command_Buffer, VK_PIPELINE_BIND_POINT_COMPUTE, Physics_Pipeline);
  vkCmdBindDescriptorSets (Command_Buffer, VK_PIPELINE_BIND_POINT_COMPUTE, Physics_Pipeline_Layout,
                           0, 1, &Physics_Descriptor_Set, 0, NULL);
  vkCmdPushConstants      (Command_Buffer, Physics_Pipeline_Layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof (G), &G);
  vkCmdDispatch           (Command_Buffer, 1, 1, 1);

  // Memory barrier: ensure compute shader writes are visible to the host for readback
  vkCmdPipelineBarrier (Command_Buffer,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
    1, &(VkMemoryBarrier){.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
      .srcAccessM



// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §8. Scene — BSP Loading
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

uint BSP_Tessellate_Patch (const BSP_Vertex *Control_Grid, int Patch_Width, int Patch_Height,
                          Vertex **Inout_Vertices, uint *Inout_Vertex_Count,
                          uint **Inout_Indices, uint *Inout_Index_Count) {

  int Grid_Columns = (Patch_Width  - 1) / 2;                                                  // Number of 3×3 sub-patches horizontally
  int Grid_Rows    = (Patch_Height - 1) / 2;                                                  // Number of 3×3 sub-patches vertically
  int Level  = TESSELLATION_LEVEL;                                                             // Subdivisions per sub-patch edge
  int Stride = Level + 1;

  uint Added_Vertices = (uint)(Grid_Columns * Grid_Rows * Stride * Stride);
  uint Added_Indices  = (uint)(Grid_Columns * Grid_Rows * Level * Level * 6);

  // Grow the output vertex and index arrays to hold the tessellated patch geometry
  *Inout_Vertices = realloc (*Inout_Vertices, sizeof (Vertex) * (*Inout_Vertex_Count + Added_Vertices));
  *Inout_Indices  = realloc (*Inout_Indices,  sizeof (uint)    * (*Inout_Index_Count  + Added_Indices));

  uint Vertex_Base  = *Inout_Vertex_Count;
  uint Index_Cursor = *Inout_Index_Count;

  // Iterate over each 3×3 sub-patch in the control grid
  for (int Patch_Y = 0; Patch_Y < Grid_Rows; Patch_Y++) for (int Patch_X = 0; Patch_X < Grid_Columns; Patch_X++) {
    vec3 Control_Position [3][3], Control_Normal [3][3], Control_Texture [3][3], Control_Lightmap [3][3];

    // Extract 3×3 control points with coordinate swizzle (Z-up → Y-up)
    for (int Row = 0; Row < 3; Row++)
      for (int Column = 0; Column < 3; Column++) {
        const BSP_Vertex *S = &Control_Grid[(Patch_Y * 2 + Row) * Patch_Width + (Patch_X * 2 + Column)];
        Control_Position[Row][Column] = Make (S->Position[0],       S->Position[2],       -S->Position[1]);
        Control_Normal[Row][Column]   = Make (S->Normal[0],         S->Normal[2],         -S->Normal[1]);
        Control_Texture[Row][Column]  = Make (S->Texture_Coords[0], S->Texture_Coords[1],  0);
        Control_Lightmap[Row][Column] = Make (S->Lightmap_Coords[0],S->Lightmap_Coords[1], 0);
      }

    uint Patch_Base = Vertex_Base + (uint)((Patch_Y * Grid_Columns + Patch_X) * Stride * Stride);

    // Evaluate the bi-quadratic Bézier surface at each tessellation grid point
    for (int V = 0; V <= Level; V++) {
      float Pv = (float)V / Level;
      vec3 Rp[3], Rn[3], Rt[3], Rl[3];
      for (int R = 0; R < 3; R++) {
        Rp[R] = Bezier_Evaluate (Control_Position [R][0], Control_Position [R][1], Control_Position [R][2], Pv);
        Rn[R] = Bezier_Evaluate (Control_Normal   [R][0], Control_Normal   [R][1], Control_Normal   [R][2], Pv);
        Rt[R] = Bezier_Evaluate (Control_Texture  [R][0], Control_Texture  [R][1], Control_Texture  [R][2], Pv);
        Rl[R] = Bezier_Evaluate (Control_Lightmap [R][0], Control_Lightmap [R][1], Control_Lightmap [R][2], Pv);
      }
      for (int H = 0; H <= Level; H++) {
        float Pu      = (float)H / Level;
        vec3 Normal    = Normalize (Bezier_Evaluate (Rn[0], Rn[1], Rn[2], Pu));
        vec3 Position  = Bezier_Evaluate (Rp[0], Rp[1], Rp[2], Pu);
        vec3 Texture   = Bezier_Evaluate (Rt[0], Rt[1], Rt[2], Pu);
        vec3 Lightmap  = Bezier_Evaluate (Rl[0], Rl[1], Rl[2], Pu);
        (*Inout_Vertices)[Patch_Base + V * Stride + H] = (Vertex){
          .Normal      = {Normal.x,   Normal.y,   Normal.z},
          .Position    = {Position.x, Position.y, Position.z},
          .Texture_Uv  = {Texture.x,  Texture.y},
          .Lightmap_Uv = {Lightmap.x, Lightmap.y}};
      }
    }

    // Generate two triangles for each quad in the tessellated grid
    for (int V = 0; V < Level; V++) for (int H = 0; H < Level; H++) {
      uint A = Patch_Base + V * Stride + H, B = A + 1;
      uint C = Patch_Base + (V + 1) * Stride + H, D = C + 1;
      (*Inout_Indices)[Index_Cursor++] = A; (*Inout_Indices)[Index_Cursor++] = C; (*Inout_Indices)[Index_Cursor++] = B;
      (*Inout_Indices)[Index_Cursor++] = B; (*Inout_Indices)[Index_Cursor++] = C; (*Inout_Indices)[Index_Cursor++] = D;
    }
  }
  *Inout_Vertex_Count += Added_Vertices;
  *Inout_Index_Count  += Added_Indices;
  return Added_Indices / 3;
}

Spawn BSP_Find_Spawn (const uint8_t *File_Data, const BSP_Header *Header) {
  const char *Cursor = (const char *)(File_Data + Header->Lumps[BSP_ENTITIES].Offset);
  const char *End    = Cursor + Header->Lumps[BSP_ENTITIES].Length;
  Spawn Result = {.Origin = {0,0,0}, .Angle = 0};

  while (Cursor < End) {
    while (Cursor < End and *Cursor != '{') Cursor++;
    if (Cursor >= End) break;
    Cursor++;
    int Is_Spawn = 0, Has_Origin = 0;
    vec3 Origin = {0}; float Angle = 0;

    while (Cursor < End and *Cursor != '}') {
      while (Cursor < End and (*Cursor == ' ' or *Cursor == '\t' or *Cursor == '\n' or *Cursor == '\r')) Cursor++;
      if (Cursor >= End or *Cursor == '}') break;
      if (*Cursor != '"') { Cursor++; continue; }
      Cursor++;
      const char *Key = Cursor;
      while (Cursor < End and *Cursor != '"') Cursor++;
      int Kl = (int)(Cursor - Key);
      if (Cursor < End) Cursor++;
      while (Cursor < End and (*Cursor == ' ' or *Cursor == '\t')) Cursor++;
      if (Cursor >= End or *Cursor != '"') continue;
      Cursor++;
      const char *Val = Cursor;
      while (Cursor < End and *Cursor != '"') Cursor++;
      int Vl = (int)(Cursor - Val);
      if (Cursor < End) Cursor++;

      if (Kl == 9 and memcmp (Key, "classname", 9) == 0 and Vl == 22 and memcmp (Val, "info_player_deathmatch", 22) == 0) Is_Spawn = 1;
      if (Kl == 6 and memcmp (Key, "origin", 6) == 0) {
        char T[64]; int L = Vl < 63 ? Vl : 63; memcpy (T, Val, L); T[L] = 0;
        sscanf (T, "%f %f %f", &Origin.x, &Origin.y, &Origin.z); Has_Origin = 1;
      }
      if (Kl == 5 and memcmp (Key, "angle", 5) == 0) {
        char T[32]; int L = Vl < 31 ? Vl : 31; memcpy (T, Val, L); T[L] = 0;
        sscanf (T, "%f", &Angle);
      }
    }
    if (Is_Spawn and Has_Origin) {
      Result.Origin = Make (Origin.x, Origin.z, -Origin.y);                                    // Quake 3 Z-up → Y-up
      Result.Angle  = Angle;
      printf ("[bsp] spawn: %.0f %.0f %.0f angle %.0f\n", Result.Origin.x, Result.Origin.y, Result.Origin.z, Angle);
      return Result;
    }
    if (Cursor < End) Cursor++;
  }
  printf ("[bsp] no spawn found, using origin\n");
  return Result;
}

Scene Scene_Load_From_BSP (const char *Path, Spawn *Out_Spawn, Collision_Map *Out_Collision) {
  FILE *File = fopen (Path, "rb");
  if (not File) { fprintf (stderr, "Cannot open %s\n", Path); exit (1); }
  fseek (File, 0, SEEK_END); long File_Size = ftell (File); rewind (File);
  uint8_t *File_Data = malloc (File_Size);
  fread (File_Data, 1, File_Size, File);
  fclose (File);

  BSP_Header *Header = (BSP_Header *)(File_Data);
  assert (Header->Magic == BSP_MAGIC and Header->Version == BSP_VERSION);

  BSP_Vertex *Raw_Vertices     = (BSP_Vertex *)(File_Data + Header->Lumps[BSP_VERTICES].Offset);
  BSP_Face   *Raw_Faces        = (BSP_Face *)  (File_Data + Header->Lumps[BSP_FACES].Offset);
  BSP_Shader *Raw_Shaders      = (BSP_Shader *)(File_Data + Header->Lumps[BSP_SHADERS].Offset);
  int        *Raw_Indices      = (int *)       (File_Data + Header->Lumps[BSP_INDICES].Offset);
  uint Raw_Vertex_Count = (uint)(Header->Lumps[BSP_VERTICES].Length / sizeof (BSP_Vertex));
  uint Raw_Face_Count   = (uint)(Header->Lumps[BSP_FACES].Length    / sizeof (BSP_Face));
  uint Raw_Shader_Count = (uint)(Header->Lumps[BSP_SHADERS].Length  / sizeof (BSP_Shader));

  // ── Build lightmap atlas ───────────────────────────────────────────────────────────────────────
  uint8_t *Lightmap_Atlas = NULL;
  uint Lm_Size       = (uint)Header->Lumps[BSP_LIGHTMAPS].Length;
  uint Lm_Pages      = Lm_Size / (LIGHTMAP_PAGE_SIZE * LIGHTMAP_PAGE_SIZE * 3);
  uint Total_Pages   = Lm_Pages + 1;
  uint Atlas_Cols = 1, Atlas_Rows = 1, Atlas_W = 0, Atlas_H = 0;
  float White_U = 0.5f, White_V = 0.5f;

  if (Lm_Pages > 0) {
    while (Atlas_Cols * Atlas_Cols < Total_Pages) Atlas_Cols++;
    Atlas_Rows = (Total_Pages + Atlas_Cols - 1) / Atlas_Cols;
    Atlas_W    = Atlas_Cols * LIGHTMAP_PAGE_SIZE;
    Atlas_H    = Atlas_Rows * LIGHTMAP_PAGE_SIZE;
    Lightmap_Atlas = calloc (Atlas_W * Atlas_H * 4, 1);
    const uint8_t *Lm_Data = File_Data + Header->Lumps[BSP_LIGHTMAPS].Offset;

    // Copy each RGB lightmap page into its grid cell, converting RGB → RGBA
    for (uint P = 0; P < Lm_Pages; P++) {
      uint Col = P % Atlas_Cols, Row = P / Atlas_Cols;
      const uint8_t *Src = Lm_Data + P * LIGHTMAP_PAGE_SIZE * LIGHTMAP_PAGE_SIZE * 3;
      for (uint y = 0; y < LIGHTMAP_PAGE_SIZE; y++)
      for (uint x = 0; x < LIGHTMAP_PAGE_SIZE; x++) {
        uint Dst = ((Row * LIGHTMAP_PAGE_SIZE + y) * Atlas_W + Col * LIGHTMAP_PAGE_SIZE + x) * 4;
        uint Si  = (y * LIGHTMAP_PAGE_SIZE + x) * 3;
        Lightmap_Atlas[Dst] = Src[Si]; Lightmap_Atlas[Dst+1] = Src[Si+1]; Lightmap_Atlas[Dst+2] = Src[Si+2]; Lightmap_Atlas[Dst+3] = 255;
      }
    }
    // White fallback page for faces with no lightmap
    uint Wc = Lm_Pages % Atlas_Cols, Wr = Lm_Pages / Atlas_Cols;
    for (uint y = 0; y < LIGHTMAP_PAGE_SIZE; y++)
    for (uint x = 0; x < LIGHTMAP_PAGE_SIZE; x++) {
      uint Dst = ((Wr * LIGHTMAP_PAGE_SIZE + y) * Atlas_W + Wc * LIGHTMAP_PAGE_SIZE + x) * 4;
      Lightmap_Atlas[Dst] = Lightmap_Atlas[Dst+1] = Lightmap_Atlas[Dst+2] = Lightmap_Atlas[Dst+3] = 255;
    }
    White_U = ((float)Wc + 0.5f) / (float)Atlas_Cols;
    White_V = ((float)Wr + 0.5f) / (float)Atlas_Rows;
    printf ("[lightmap] %u pages -> %ux%u atlas\n", Lm_Pages, Atlas_W, Atlas_H);
  }

  // ── Convert vertices from Z-up to Y-up ────────────────────────────────────────────────────────
  uint Vertex_Count = Raw_Vertex_Count;
  Vertex *Vertices  = malloc (sizeof (Vertex) * Vertex_Count);
  for (uint I = 0; I < Vertex_Count; I++) Vertices[I] = Convert_BSP_Vertex (&Raw_Vertices[I]);

  uint *Indices = NULL, *Texture_Ids = NULL;
  uint  Index_Count = 0, Triangle_Count = 0;

  // ── Process faces ──────────────────────────────────────────────────────────────────────────────
  for (uint Fi = 0; Fi < Raw_Face_Count; Fi++) {
    const BSP_Face *F = &Raw_Faces[Fi];

    if (F->Type == SURFACE_TYPE_PLANAR or F->Type == SURFACE_TYPE_MESH) {
      uint Ft = (uint)(F->Index_Count / 3);
      Indices     = realloc (Indices,     sizeof (uint) * (Index_Count    + F->Index_Count));
      Texture_Ids = realloc (Texture_Ids, sizeof (uint) * (Triangle_Count + Ft));
      for (int L = 0; L < F->Index_Count; L++)
        Indices[Index_Count + L] = (uint)(F->First_Vertex + Raw_Indices[F->First_Index + L]);
      for (uint T = 0; T < Ft; T++) Texture_Ids[Triangle_Count + T] = (uint)F->Shader_Index;
      Index_Count += F->Index_Count; Triangle_Count += Ft;

      // Remap lightmap UVs to atlas space
      if (F->Lightmap_Index >= 0 and Atlas_Cols > 0) {
        float Co = (float)((uint)F->Lightmap_Index % Atlas_Cols), Ro = (float)((uint)F->Lightmap_Index / Atlas_Cols);
        for (int Vl = 0; Vl < F->Vertex_Count; Vl++) {
          uint Vi = (uint)(F->First_Vertex + Vl);
          Vertices[Vi].Lightmap_Uv[0] = (Co + Vertices[Vi].Lightmap_Uv[0]) / (float)Atlas_Cols;
          Vertices[Vi].Lightmap_Uv[1] = (Ro + Vertices[Vi].Lightmap_Uv[1]) / (float)Atlas_Rows;
        }
      } else {
        for (int Vl = 0; Vl < F->Vertex_Count; Vl++) {
          uint Vi = (uint)(F->First_Vertex + Vl);
          Vertices[Vi].Lightmap_Uv[0] = White_U; Vertices[Vi].Lightmap_Uv[1] = White_V;
        }
      }

    } else if (F->Type == SURFACE_TYPE_PATCH) {
      uint Pv = Vertex_Count, Pt = Triangle_Count;
      Triangle_Count += BSP_Tessellate_Patch (&Raw_Vertices[F->First_Vertex], F->Patch_Width, F->Patch_Height,
                                              &Vertices, &Vertex_Count, &Indices, &Index_Count);
      uint Np = Triangle_Count - Pt;
      Texture_Ids = realloc (Texture_Ids, sizeof (uint) * Triangle_Count);
      for (uint T = 0; T < Np; T++) Texture_Ids[Pt + T] = (uint)F->Shader_Index;

      if (F->Lightmap_Index >= 0 and Atlas_Cols > 0) {
        float Co = (float)((uint)F->Lightmap_Index % Atlas_Cols), Ro = (float)((uint)F->Lightmap_Index / Atlas_Cols);
        for (uint Vi = Pv; Vi < Vertex_Count; Vi++) {
          Vertices[Vi].Lightmap_Uv[0] = (Co + Vertices[Vi].Lightmap_Uv[0]) / (float)Atlas_Cols;
          Vertices[Vi].Lightmap_Uv[1] = (Ro + Vertices[Vi].Lightmap_Uv[1]) / (float)Atlas_Rows;
        }
      } else {
        for (uint Vi = Pv; Vi < Vertex_Count; Vi++) {
          Vertices[Vi].Lightmap_Uv[0] = White_U; Vertices[Vi].Lightmap_Uv[1] = White_V;
        }
      }
    }
  }

  // ── Materials: hash shader names to fallback colors ────────────────────────────────────────────
  uint Material_Count = Raw_Shader_Count;
  vec4 *Materials          = malloc (sizeof (vec4) * Material_Count);
  char (*Texture_Names)[64] = malloc (sizeof (char[64]) * Material_Count);
  for (uint M = 0; M < Material_Count; M++) {
    uint H = 5381;
    for (int C = 0; Raw_Shaders[M].Name[C]; C++) H = H * 31 + (uint8_t)Raw_Shaders[M].Name[C];
    Materials[M] = (vec4){0.4f + 0.35f*((H>>0&0xFF)/255.f), 0.4f + 0.35f*((H>>8&0xFF)/255.f), 0.4f + 0.35f*((H>>16&0xFF)/255.f), 1};
    memcpy (Texture_Names[M], Raw_Shaders[M].Name, 64);
  }

  if (Out_Spawn) *Out_Spawn = BSP_Find_Spawn (File_Data, Header);

  // ── Collision map ──────────────────────────────────────────────────────────────────────────────
  if (Out_Collision) {
    Collision_Map *C = Out_Collision;
    memset (C, 0, sizeof (*C));

    // Planes (16 bytes: float normal[3], float distance)
    C->Plane_Count = (uint)Header->Lumps[BSP_PLANES].Length / 16;
    C->Planes      = malloc (sizeof (Collision_Plane) * C->Plane_Count);
    for (uint I = 0; I < C->Plane_Count; I++) {
      const float *Src = (const float *)(File_Data + Header->Lumps[BSP_PLANES].Offset + I * 16);
      // BUG FIX: declare Normal BEFORE using it for Type and Sign_Bits
      float *Normal = C->Planes[I].Normal;
      Normal[0] = Src[0]; Normal[1] = Src[2]; Normal[2] = -Src[1];
      C->Planes[I].Distance  = Src[3];
      C->Planes[I].Type      = (Normal[0]==1.f) ? 0 : (Normal[1]==1.f) ? 1 : (Normal[2]==1.f) ? 2 : 3;
      C->Planes[I].Sign_Bits = (uint8_t)((Normal[0]<0) | ((Normal[1]<0)<<1) | ((Normal[2]<0)<<2));
    }

    // Nodes (36 bytes: int plane, children[2], mins[3], maxs[3])
    C->Node_Count = (uint)Header->Lumps[BSP_NODES].Length / 36;
    C->Nodes      = malloc (sizeof (Collision_Node) * C->Node_Count);
    for (uint I = 0; I < C->Node_Count; I++) {
      const int *Src = (const int *)(File_Data + Header->Lumps[BSP_NODES].Offset + I * 36);
      C->Nodes[I].Plane_Index = Src[0]; C->Nodes[I].Children[0] = Src[1]; C->Nodes[I].Children[1] = Src[2];
    }

    // Leafs (48 bytes)
    C->Leaf_Count = (uint)Header->Lumps[BSP_LEAFS].Length / 48;
    C->Leafs      = malloc (sizeof (Collision_Leaf) * C->Leaf_Count);
    for (uint I = 0; I < C->Leaf_Count; I++) {
      const int *Src = (const int *)(File_Data + Header->Lumps[BSP_LEAFS].Offset + I * 48);
      C->Leafs[I].Cluster = Src[0]; C->Leafs[I].Area = Src[1];
      C->Leafs[I].First_Surface = Src[8]; C->Leafs[I].Surface_Count = Src[9];
      C->Leafs[I].First_Brush   = Src[10]; C->Leafs[I].Brush_Count  = Src[11];
    }

    // Leaf brushes (indirection array)
    C->Leaf_Brush_Count = (uint)Header->Lumps[BSP_LEAF_BRUSHES].Length / 4;
    C->Leaf_Brushes     = malloc (sizeof (int) * C->Leaf_Brush_Count);
    memcpy (C->Leaf_Brushes, File_Data + Header->Lumps[BSP_LEAF_BRUSHES].Offset, sizeof (int) * C->Leaf_Brush_Count);

    // Brushes (12 bytes: first_side, side_count, shader)
    C->Brush_Count = (uint)Header->Lumps[BSP_BRUSHES].Length / 12;
    C->Brushes     = malloc (sizeof (Collision_Brush) * C->Brush_Count);
    for (uint I = 0; I < C->Brush_Count; I++) {
      const int *Src = (const int *)(File_Data + Header->Lumps[BSP_BRUSHES].Offset + I * 12);
      C->Brushes[I].First_Side = Src[0]; C->Brushes[I].Side_Count = Src[1]; C->Brushes[I].Shader_Index = Src[2];
    }

    // Brush sides (8 bytes: plane_index, shader_index)
    C->Side_Count = (uint)Header->Lumps[BSP_BRUSH_SIDES].Length / 8;
    C->Sides      = malloc (sizeof (Collision_Brush_Side) * C->Side_Count);
    for (uint I = 0; I < C->Side_Count; I++) {
      const int *Src = (const int *)(File_Data + Header->Lumps[BSP_BRUSH_SIDES].Offset + I * 8);
      C->Sides[I].Plane_Index = Src[0]; C->Sides[I].Shader_Index = Src[1];
    }

    C->Shader_Contents = malloc (sizeof (int) * Raw_Shader_Count);
    for (uint S = 0; S < Raw_Shader_Count; S++) C->Shader_Contents[S] = Raw_Shaders[S].Contents;
    C->Brush_Checks  = calloc (C->Brush_Count, sizeof (uint));
    C->Check_Counter = 0;
    printf ("[collision] %u planes, %u nodes, %u leafs, %u brushes, %u sides\n",
            C->Plane_Count, C->Node_Count, C->Leaf_Count, C->Brush_Count, C->Side_Count);
  }

  free (File_Data);
  printf ("[bsp] %s: %u vertices, %u triangles, %u shaders\n", Path, Vertex_Count, Triangle_Count, Raw_Shader_Count);
  return (Scene){
    .Vertices = Vertices, .Vertex_Count = Vertex_Count, .Indices = Indices, .Index_Count = Index_Count,
    .Materials = Materials, .Material_Count = Material_Count, .Texture_Ids = Texture_Ids, .Texture_Names = Texture_Names,
    .Lightmap_Atlas = Lightmap_Atlas, .Lightmap_Width = Atlas_W, .Lightmap_Height = Atlas_H, .Triangle_Count = Triangle_Count};
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §8. Scene — Texture Loading
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

void Scene_Load_Textures (const Scene *S) {
  Texture_Sampler  = Sampler_Create_Repeating ();
  Texture_Count    = S->Material_Count;
  Textures_Loaded  = 0;
  Texture_Images   = calloc (Texture_Count, sizeof (VkImage));
  Texture_Memories = calloc (Texture_Count, sizeof (VkDeviceMemory));
  Texture_Views    = calloc (Texture_Count, sizeof (VkImageView));

  for (uint I = 0; I < Texture_Count; I++) {
    uint W = 0, H = 0; uint8_t *Px = NULL;
    if (S->Texture_Names) {
      char P[256]; snprintf (P, sizeof P, "assets/%s.tga", S->Texture_Names[I]);
      Px = TGA_Load (P, &W, &H);
    }
    if (Px and W and H) {
      Texture_Upload (Command_Buffer, Queue, Px, W, H, &Texture_Images[I], &Texture_Memories[I], &Texture_Views[I]);
      free (Px); Textures_Loaded++;
    } else {
      vec4 Cl = S->Materials[I];
      uint8_t Fb[4] = {(uint8_t)(Cl.x*255), (uint8_t)(Cl.y*255), (uint8_t)(Cl.z*255), 255};
      Texture_Upload (Command_Buffer, Queue, Fb, 1, 1, &Texture_Images[I], &Texture_Memories[I], &Texture_Views[I]);
    }
  }

  // Per-triangle texture ID storage buffer
  Texture_Id_Buffer = Buffer_Stage_Upload (Command_Buffer, Queue, S->Texture_Ids,
                                           sizeof (uint) * S->Triangle_Count, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  printf ("[textures] loaded %u/%u, %u fallbacks\n", Textures_Loaded, Texture_Count, Texture_Count - Textures_Loaded);

  // Lightmap atlas (or 1×1 white fallback)
  Lightmap_Sampler = Sampler_Create_Clamping ();
  if (S->Lightmap_Atlas and S->Lightmap_Width and S->Lightmap_Height) {
    Texture_Upload_With_Format (Command_Buffer, Queue, S->Lightmap_Atlas, S->Lightmap_Width, S->Lightmap_Height,
                                VK_FORMAT_R8G8B8A8_UNORM, &Lightmap_Image, &Lightmap_Memory, &Lightmap_View);
  } else {
    uint8_t W[4] = {255,255,255,255};
    Texture_Upload_With_Format (Command_Buffer, Queue, W, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, &Lightmap_Image, &Lightmap_Memory, &Lightmap_View);
  }
}

void Weapon_Load_Textures (Weapon_Instance *Weapon) {
  Weapon->Texture_Base_Index = Texture_Count;
  uint New_Total = Texture_Count + Weapon_Texture_Count;
  Texture_Images   = realloc (Texture_Images,   sizeof (VkImage)        * New_Total);
  Texture_Memories = realloc (Texture_Memories,  sizeof (VkDeviceMemory) * New_Total);
  Texture_Views    = realloc (Texture_Views,     sizeof (VkImageView)    * New_Total);
  for (uint I = 0; I < Weapon_Texture_Count; I++) {
    uint W = 0, H = 0;
    uint8_t *Px = TGA_Load (Weapon_Texture_Paths[I], &W, &H);
    if (Px and W and H) {
      Texture_Upload (Command_Buffer, Queue, Px, W, H, &Texture_Images[Texture_Count], &Texture_Memories[Texture_Count], &Texture_Views[Texture_Count]);
      free (Px);
    } else {
      uint8_t Fb[4] = {180,180,180,255};
      Texture_Upload (Command_Buffer, Queue, Fb, 1, 1, &Texture_Images[Texture_Count], &Texture_Memories[Texture_Count], &Texture_Views[Texture_Count]);
    }
    Texture_Count++;
  }
  printf ("[weapon] textures: base=%u, count=%u\n", Weapon->Texture_Base_Index, Weapon_Texture_Count);
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §10. Acceleration Structures
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

Acceleration_Structure Build_World_Bottom_Level (const Scene *S) {
  Vertex_Buffer   = Buffer_Stage_Upload (Command_Buffer, Queue, S->Vertices, sizeof (Vertex) * S->Vertex_Count,
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
  Index_Buffer    = Buffer_Stage_Upload (Command_Buffer, Queue, S->Indices,  sizeof (uint)   * S->Index_Count,
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT  | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
  Material_Buffer = Buffer_Allocate (sizeof (vec4) * S->Material_Count,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  Buffer_Upload (Material_Buffer, S->Materials, sizeof (vec4) * S->Material_Count);

  VkAccelerationStructureGeometryKHR Geom = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
    .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.triangles = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
      .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT, .vertexData.deviceAddress = Vertex_Buffer.Address,
      .vertexStride = sizeof (Vertex), .maxVertex = S->Vertex_Count - 1,
      .indexType = VK_INDEX_TYPE_UINT32, .indexData.deviceAddress = Index_Buffer.Address}};

  VkAccelerationStructureBuildGeometryInfoKHR Bi = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR, .geometryCount = 1, .pGeometries = &Geom};

  uint Pc = S->Triangle_Count;
  VkAccelerationStructureBuildSizesInfoKHR Sz = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
  vkGetAccelerationStructureBuildSizes (Device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &Bi, &Pc, &Sz);

  Acceleration_Structure R = {0};
  R.Buffer = Buffer_Allocate (Sz.accelerationStructureSize,
               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK (vkCreateAccelerationStructure (Device,
    &(VkAccelerationStructureCreateInfoKHR){.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
      .buffer = R.Buffer.Buffer, .size = Sz.accelerationStructureSize,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR}, NULL, &R.Handle));

  Gpu_Buffer Scratch = Buffer_Allocate (Sz.buildScratchSize,
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  Bi.dstAccelerationStructure  = R.Handle;
  Bi.scratchData.deviceAddress = Scratch.Address;

  VkAccelerationStructureBuildRangeInfoKHR Range = {.primitiveCount = Pc};
  const VkAccelerationStructureBuildRangeInfoKHR *Rp = &Range;

  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (Command_Buffer,
    &(VkCommandBufferBeginInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));
  vkCmdBuildAccelerationStructures (Command_Buffer, 1, &Bi, &Rp);
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));
  VK_CHECK (vkQueueSubmit (Queue, 1, &(VkSubmitInfo){.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &Command_Buffer}, VK_NULL_HANDLE));
  VK_CHECK (vkQueueWaitIdle (Queue));

  R.Address = vkGetAccelerationStructureDeviceAddress (Device,
    &(VkAccelerationStructureDeviceAddressInfoKHR){.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, .accelerationStructure = R.Handle});
  vkDestroyBuffer (Device, Scratch.Buffer, NULL);
  vkFreeMemory    (Device, Scratch.Memory, NULL);
  return R;
}

void Weapon_Bottom_Level_Initialize (Weapon_Instance *W) {
  if (not W->Model.Vertex_Count) return;
  W->Transformed_Vertices = malloc (sizeof (Vertex) * W->Model.Vertex_Count);
  memcpy (W->Transformed_Vertices, W->Model.Vertices, sizeof (Vertex) * W->Model.Vertex_Count);
  W->Vertex_Buffer = Buffer_Allocate (sizeof (Vertex) * W->Model.Vertex_Count,
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  Buffer_Upload (W->Vertex_Buffer, W->Transformed_Vertices, sizeof (Vertex) * W->Model.Vertex_Count);

  W->Index_Buffer      = Buffer_Stage_Upload (Command_Buffer, Queue, W->Model.Indices, sizeof (uint) * W->Model.Index_Count,
                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
  W->Texture_Id_Buffer = Buffer_Stage_Upload (Command_Buffer, Queue, W->Model.Texture_Ids, sizeof (uint) * W->Model.Triangle_Count,
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

  VkAccelerationStructureGeometryKHR Geom = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
    .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.triangles = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
      .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT, .vertexData.deviceAddress = W->Vertex_Buffer.Address,
      .vertexStride = sizeof (Vertex), .maxVertex = W->Model.Vertex_Count - 1,
      .indexType = VK_INDEX_TYPE_UINT32, .indexData.deviceAddress = W->Index_Buffer.Address}};

  VkAccelerationStructureBuildGeometryInfoKHR Bi = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
    .geometryCount = 1, .pGeometries = &Geom};

  uint Pc = W->Model.Triangle_Count;
  VkAccelerationStructureBuildSizesInfoKHR Sz = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
  vkGetAccelerationStructureBuildSizes (Device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &Bi, &Pc, &Sz);

  W->Bottom_Level.Buffer = Buffer_Allocate (Sz.accelerationStructureSize,
    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK (vkCreateAccelerationStructure (Device,
    &(VkAccelerationStructureCreateInfoKHR){.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
      .buffer = W->Bottom_Level.Buffer.Buffer, .size = Sz.accelerationStructureSize,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR}, NULL, &W->Bottom_Level.Handle));
  W->Bottom_Level_Scratch = Buffer_Allocate (Sz.buildScratchSize,
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  Bi.dstAccelerationStructure = W->Bottom_Level.Handle;
  Bi.scratchData.deviceAddress = W->Bottom_Level_Scratch.Address;
  VkAccelerationStructureBuildRangeInfoKHR Range = {.primitiveCount = Pc};
  const VkAccelerationStructureBuildRangeInfoKHR *Rp = &Range;

  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (Command_Buffer,
    &(VkCommandBufferBeginInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));
  vkCmdBuildAccelerationStructures (Command_Buffer, 1, &Bi, &Rp);
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));
  VK_CHECK (vkQueueSubmit (Queue, 1, &(VkSubmitInfo){.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &Command_Buffer}, VK_NULL_HANDLE));
  VK_CHECK (vkQueueWaitIdle (Queue));

  W->Bottom_Level.Address = vkGetAccelerationStructureDeviceAddress (Device,
    &(VkAccelerationStructureDeviceAddressInfoKHR){.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, .accelerationStructure = W->Bottom_Level.Handle});
  printf ("[weapon] BLAS built: %u triangles\n", Pc);
}

void Weapon_Bottom_Level_Rebuild (Weapon_Instance *W) {
  if (not W->Model.Vertex_Count) return;
  Buffer_Upload (W->Vertex_Buffer, W->Transformed_Vertices, sizeof (Vertex) * W->Model.Vertex_Count);

  VkAccelerationStructureGeometryKHR Geom = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
    .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.triangles = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
      .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT, .vertexData.deviceAddress = W->Vertex_Buffer.Address,
      .vertexStride = sizeof (Vertex), .maxVertex = W->Model.Vertex_Count - 1,
      .indexType = VK_INDEX_TYPE_UINT32, .indexData.deviceAddress = W->Index_Buffer.Address}};
  VkAccelerationStructureBuildGeometryInfoKHR Bi = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
    .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    .dstAccelerationStructure = W->Bottom_Level.Handle,
    .scratchData.deviceAddress = W->Bottom_Level_Scratch.Address,
    .geometryCount = 1, .pGeometries = &Geom};
  VkAccelerationStructureBuildRangeInfoKHR Range = {.primitiveCount = W->Model.Triangle_Count};
  const VkAccelerationStructureBuildRangeInfoKHR *Rp = &Range;

  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (Command_Buffer,
    &(VkCommandBufferBeginInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));
  vkCmdBuildAccelerationStructures (Command_Buffer, 1, &Bi, &Rp);
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));
  VK_CHECK (vkQueueSubmit (Queue, 1, &(VkSubmitInfo){.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &Command_Buffer}, VK_NULL_HANDLE));
  VK_CHECK (vkQueueWaitIdle (Queue));
}

void Top_Level_Initialize (uint Max_Instances) {
  Top_Level_Instance_Buffer = Buffer_Allocate (sizeof (VkAccelerationStructureInstanceKHR) * Max_Instances,
    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  VkAccelerationStructureGeometryKHR Geom = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
    .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.instances = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
      .arrayOfPointers = VK_FALSE, .data.deviceAddress = Top_Level_Instance_Buffer.Address}};
  VkAccelerationStructureBuildGeometryInfoKHR Bi = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR, .geometryCount = 1, .pGeometries = &Geom};
  VkAccelerationStructureBuildSizesInfoKHR Sz = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
  vkGetAccelerationStructureBuildSizes (Device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &Bi, &Max_Instances, &Sz);

  Top_Level.Buffer = Buffer_Allocate (Sz.accelerationStructureSize,
    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK (vkCreateAccelerationStructure (Device,
    &(VkAccelerationStructureCreateInfoKHR){.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
      .buffer = Top_Level.Buffer.Buffer, .size = Sz.accelerationStructureSize,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR}, NULL, &Top_Level.Handle));
  Top_Level_Scratch_Buffer = Buffer_Allocate (Sz.buildScratchSize,
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  Top_Level.Address = vkGetAccelerationStructureDeviceAddress (Device,
    &(VkAccelerationStructureDeviceAddressInfoKHR){.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, .accelerationStructure = Top_Level.Handle});
}

void Top_Level_Rebuild (Acceleration_Structure *World, Acceleration_Structure *Weapon) {
  VkAccelerationStructureInstanceKHR Inst[2]; memset (Inst, 0, sizeof Inst);
  Inst[0].transform.matrix[0][0] = 1.f; Inst[0].transform.matrix[1][1] = 1.f; Inst[0].transform.matrix[2][2] = 1.f;
  Inst[0].mask = 0xFF; Inst[0].instanceCustomIndex = 0;
  Inst[0].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
  Inst[0].accelerationStructureReference = World->Address;
  uint Ic = 1;
  if (Weapon and Weapon->Handle) {
    Inst[1].transform.matrix[0][0] = 1.f; Inst[1].transform.matrix[1][1] = 1.f; Inst[1].transform.matrix[2][2] = 1.f;
    Inst[1].mask = 0x01; Inst[1].instanceCustomIndex = 1;
    Inst[1].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    Inst[1].accelerationStructureReference = Weapon->Address; Ic = 2;
  }
  Buffer_Upload (Top_Level_Instance_Buffer, Inst, sizeof (VkAccelerationStructureInstanceKHR) * Ic);

  VkAccelerationStructureBuildRangeInfoKHR Range = {.primitiveCount = Ic};
  const VkAccelerationStructureBuildRangeInfoKHR *Rp = &Range;
  VkAccelerationStructureGeometryKHR Geom = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
    .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.instances = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
      .arrayOfPointers = VK_FALSE, .data.deviceAddress = Top_Level_Instance_Buffer.Address}};
  VkAccelerationStructureBuildGeometryInfoKHR Bi = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR,
    .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    .dstAccelerationStructure = Top_Level.Handle,
    .scratchData.deviceAddress = Top_Level_Scratch_Buffer.Address,
    .geometryCount = 1, .pGeometries = &Geom};

  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (Command_Buffer,
    &(VkCommandBufferBeginInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));
  vkCmdBuildAccelerationStructures (Command_Buffer, 1, &Bi, &Rp);
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));
  VK_CHECK (vkQueueSubmit (Queue, 1, &(VkSubmitInfo){.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &Command_Buffer}, VK_NULL_HANDLE));
  VK_CHECK (vkQueueWaitIdle (Queue));
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §12. Pipeline
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

#define SHADER_PATH_RAY_GENERATION  "shaders/raygen.spv"
#define SHADER_PATH_CLOSEST_HIT     "shaders/closesthit.spv"
#define SHADER_PATH_PRIMARY_MISS    "shaders/miss.spv"
#define SHADER_PATH_SHADOW_MISS     "shaders/shadow_miss.spv"
#define SHADER_PATH_PHYSICS_COMPUTE "shaders/physics.spv"

// BUG FIX: single correct definition (original had duplicate file-open/size logic merged together)
VkShaderModule Shader_Module_Load (const char *Path) {
  FILE *File = fopen (Path, "rb");
  if (not File) { fprintf (stderr, "Cannot open shader %s\n", Path); exit (1); }
  fseek (File, 0, SEEK_END); long Size = ftell (File); rewind (File);
  uint *Code = malloc (Size);
  fread (Code, 1, Size, File);
  fclose (File);
  VkShaderModule Module;
  VK_CHECK (vkCreateShaderModule (Device,
    &(VkShaderModuleCreateInfo){.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = (size_t)Size, .pCode = Code},
    NULL, &Module));
  free (Code);
  return Module;
}

void Raytracing_Pipeline_Create () {
  VkDescriptorSetLayoutBinding Bindings[] = {
    {0,  VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,   VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {1,  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1,   VK_SHADER_STAGE_RAYGEN_BIT_KHR, NULL},
    {2,  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1,   VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {3,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1,   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {4,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1,   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {5,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1,   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {6,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1,   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {7,  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1,   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {8,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1,   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {9,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1,   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1,   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     256, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
  };
  VkDescriptorBindingFlags Bf[] = {0,0,0,0,0,0,0,0,0,0,0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};
  VkDescriptorSetLayoutBindingFlagsCreateInfo Bfi = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, .bindingCount = 12, .pBindingFlags = Bf};
  VK_CHECK (vkCreateDescriptorSetLayout (Device,
    &(VkDescriptorSetLayoutCreateInfo){.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = &Bfi, .bindingCount = 12, .pBindings = Bindings}, NULL, &Descriptor_Set_Layout));
  VK_CHECK (vkCreatePipelineLayout (Device,
    &(VkPipelineLayoutCreateInfo){.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1, .pSetLayouts = &Descriptor_Set_Layout}, NULL, &Pipeline_Layout));

  // Load the four SPIR-V shader modules from pre-compiled files
  VkShaderModule Rgen = Shader_Module_Load (SHADER_PATH_RAY_GENERATION);
  VkShaderModule Chit = Shader_Module_Load (SHADER_PATH_CLOSEST_HIT);
  VkShaderModule Pmis = Shader_Module_Load (SHADER_PATH_PRIMARY_MISS);
  VkShaderModule Smis = Shader_Module_Load (SHADER_PATH_SHADOW_MISS);

  VkPipelineShaderStageCreateInfo St[] = {
    {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_RAYGEN_BIT_KHR,      Rgen, "main", NULL},
    {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_MISS_BIT_KHR,        Pmis, "main", NULL},
    {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_MISS_BIT_KHR,        Smis, "main", NULL},
    {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, Chit, "main", NULL}};
  VkRayTracingShaderGroupCreateInfoKHR Gr[] = {
    {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, NULL, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,              0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR},
    {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, NULL, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,              1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR},
    {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, NULL, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,              2, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR},
    {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, NULL, VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,  VK_SHADER_UNUSED_KHR, 3, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR}};

  VK_CHECK (vkCreateRayTracingPipelines (Device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1,
    &(VkRayTracingPipelineCreateInfoKHR){.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
      .stageCount = 4, .pStages = St, .groupCount = 4, .pGroups = Gr,
      .maxPipelineRayRecursionDepth = 2, .layout = Pipeline_Layout}, NULL, &Pipeline));
  vkDestroyShaderModule (Device, Rgen, NULL); vkDestroyShaderModule (Device, Chit, NULL);
  vkDestroyShaderModule (Device, Pmis, NULL); vkDestroyShaderModule (Device, Smis, NULL);
}

void Shader_Binding_Table_Create () {
  uint Hs = Raytracing_Properties.shaderGroupHandleSize;
  uint Ha = Raytracing_Properties.shaderGroupHandleAlignment;
  uint Ba = Raytracing_Properties.shaderGroupBaseAlignment;
  uint Stride = (Hs + Ha - 1) & ~(Ha - 1);
  if (Stride < Ba) Stride = Ba;

  uint Gc = 4;
  uint8_t *H = malloc (Hs * Gc);
  VK_CHECK (vkGetRayTracingShaderGroupHandles (Device, Pipeline, 0, Gc, Hs * Gc, H));
  uint Ts = Stride * Gc;
  Shader_Binding_Table_Buffer = Buffer_Allocate (Ts,
    VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  uint8_t *Dst; vkMapMemory (Device, Shader_Binding_Table_Buffer.Memory, 0, Ts, 0, (void**)&Dst);
  for (uint I = 0; I < Gc; I++) memcpy (Dst + I * Stride, H + I * Hs, Hs);
  vkUnmapMemory (Device, Shader_Binding_Table_Buffer.Memory);
  free (H);

  VkDeviceAddress Base = Shader_Binding_Table_Buffer.Address;
  Shader_Binding_Ray_Generation = (VkStridedDeviceAddressRegionKHR){Base + 0*Stride, Stride, Stride};
  Shader_Binding_Miss           = (VkStridedDeviceAddressRegionKHR){Base + 1*Stride, Stride, Stride * 2};
  Shader_Binding_Hit            = (VkStridedDeviceAddressRegionKHR){Base + 3*Stride, Stride, Stride};
  Shader_Binding_Callable       = (VkStridedDeviceAddressRegionKHR){0, 0, 0, 0};
}

void Descriptor_Set_Create (Weapon_Instance *Weapon) {
  VkDescriptorPoolSize Ps[] = {{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,1}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1},
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,7}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,257}};
  VK_CHECK (vkCreateDescriptorPool (Device,
    &(VkDescriptorPoolCreateInfo){.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = 5, .pPoolSizes = Ps}, NULL, &Descriptor_Pool));

  uint Vc = Texture_Count;
  VkDescriptorSetVariableDescriptorCountAllocateInfo Va = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO, .descriptorSetCount = 1, .pDescriptorCounts = &Vc};
  VK_CHECK (vkAllocateDescriptorSets (Device,
    &(VkDescriptorSetAllocateInfo){.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .pNext = &Va,
      .descriptorPool = Descriptor_Pool, .descriptorSetCount = 1, .pSetLayouts = &Descriptor_Set_Layout}, &Descriptor_Set));

  VkWriteDescriptorSetAccelerationStructureKHR Aw = {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, .accelerationStructureCount = 1, .pAccelerationStructures = &Top_Level.Handle};
  VkDescriptorImageInfo  Ii = {.imageView = Raytracing_Storage_Image.View, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorBufferInfo Ci = {Camera_Uniform_Buffer.Buffer, 0, Camera_Uniform_Buffer.Size};
  VkDescriptorBufferInfo Vi = {Vertex_Buffer.Buffer, 0, Vertex_Buffer.Size};
  VkDescriptorBufferInfo Ix = {Index_Buffer.Buffer, 0, Index_Buffer.Size};
  VkDescriptorBufferInfo Mi = {Material_Buffer.Buffer, 0, Material_Buffer.Size};
  VkDescriptorBufferInfo Ti = {Texture_Id_Buffer.Buffer, 0, Texture_Id_Buffer.Size};
  VkDescriptorBufferInfo Wv = {Weapon->Vertex_Buffer.Buffer, 0, Weapon->Vertex_Buffer.Size};
  VkDescriptorBufferInfo Wi = {Weapon->Index_Buffer.Buffer, 0, Weapon->Index_Buffer.Size};
  VkDescriptorBufferInfo Wt = {Weapon->Texture_Id_Buffer.Buffer, 0, Weapon->Texture_Id_Buffer.Size};
  VkDescriptorImageInfo  Li = {Lightmap_Sampler, Lightmap_View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkDescriptorImageInfo *Tx = calloc (Texture_Count, sizeof (VkDescriptorImageInfo));
  for (uint I = 0; I < Texture_Count; I++)
    Tx[I] = (VkDescriptorImageInfo){Texture_Sampler, Texture_Views[I], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

  VkWriteDescriptorSet W[] = {
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, &Aw, Descriptor_Set, 0,0,1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, NULL, NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 1,0,1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &Ii, NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 2,0,1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL, &Ci},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 3,0,1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &Vi},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 4,0,1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &Ix},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 5,0,1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &Mi},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 6,0,1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &Ti},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 7,0,1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &Li, NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 8,0,1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &Wv},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 9,0,1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &Wi},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 10,0,1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &Wt},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 11,0,Texture_Count, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Tx, NULL},
  };
  vkUpdateDescriptorSets (Device, 12, W, 0, NULL);
  free (Tx);
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §14. Render — Camera, Weapon, Input, Frame
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// BUG FIX: local variable renamed from `View` to `View_Matrix` to avoid shadowing the View() function
void Camera_Upload (Camera *St, float Fov, uint Weapon_Tex_Base) {
  mat4 View_Matrix = View (St->Position, St->Yaw, St->Pitch);
  mat4 Proj_Matrix = Perspective (Fov, (float)Width / Height, 0.1f, 10000.f);
  struct { mat4 Inv_View, Inv_Proj; uint Frame, Wtb; float Pad[2]; } U;
  U.Inv_View = Inverse_Orthogonal (View_Matrix);
  U.Inv_Proj = Inverse_Projection (Proj_Matrix);
  U.Frame    = St->Frame;
  U.Wtb      = Weapon_Tex_Base;
  Buffer_Upload (Camera_Uniform_Buffer, &U, sizeof U);
}

void Weapon_Update (Weapon_Instance *W, const Camera *Cam, float Dt, int Fire) {
  if (not W->Model.Vertex_Count) return;

  // Fire animation state machine
  if (Fire and not W->Is_Firing) { W->Is_Firing = 1; W->Fire_Time = 0; }
  if (W->Is_Firing) { W->Fire_Time += Dt * 10.f; if (W->Fire_Time >= 6.f) { W->Is_Firing = 0; W->Fire_Time = 0; } }
  W->Bob_Time += Dt;

  // Camera orthonormal basis from yaw and pitch
  float Cy = cosf(Cam->Yaw), Sy = sinf(Cam->Yaw), Cp = cosf(Cam->Pitch), Sp = sinf(Cam->Pitch);
  vec3 Fwd = Make (-Sp, Cp, -Cy * Cp);
  vec3 Rgt = Normalize (Cross (Fwd, Make (0,1,0)));
  vec3 Up  = Cross (Rgt, Fwd);

  // Viewmodel offset with idle bob and recoil
  float Bv = sinf(W->Bob_Time * 3.5f) * 0.4f, Bh = cosf(W->Bob_Time * 1.7f) * 0.2f;
  float Rc = W->Is_Firing ? -1.2f * expf(-W->Fire_Time * 5.f) : 0.f;
  vec3 Off = Add (Cam->Position, Add (Scale (Fwd, 8.f+Rc), Add (Scale (Rgt, 5.f+Bh), Scale (Up, -5.f+Bv))));

  // Select animation frame
  uint Fi = 0;
  if (W->Model.Animation_Frame_Count > 1 and W->Is_Firing) {
    Fi = (uint)W->Fire_Time;
    if (Fi >= W->Model.Animation_Frame_Count) Fi = W->Model.Animation_Frame_Count - 1;
  }

  // Swizzle tag axes from Quake 3 Z-up → Y-up
  const float *Tag = W->Model.Tag_Weapon[Fi];
  vec3 A0 = {Tag[3], Tag[5], -Tag[4]}, A1 = {Tag[6], Tag[8], -Tag[7]}, A2 = {Tag[9], Tag[11], -Tag[10]};
  float Ty[9] = {A0.x, A2.x, -A1.x, A0.y, A2.y, -A1.y, A0.z, A2.z, -A1.z};
  float Cb[9] = {Fwd.x, Up.x, Rgt.x, Fwd.y, Up.y, Rgt.y, Fwd.z, Up.z, Rgt.z};
  float Rot[9];
  for (int R = 0; R < 3; R++) for (int C = 0; C < 3; C++)
    Rot[R*3+C] = Cb[R*3]*Ty[C] + Cb[R*3+1]*Ty[3+C] + Cb[R*3+2]*Ty[6+C];
  float Sc = 0.7f;

  // Transform each vertex from model space to world space
  for (uint I = 0; I < W->Model.Vertex_Count; I++) {
    float Sx = W->Model.Vertices[I].Position[0]*Sc, Sy2 = W->Model.Vertices[I].Position[1]*Sc, Sz = W->Model.Vertices[I].Position[2]*Sc;
    W->Transformed_Vertices[I].Position[0] = Rot[0]*Sx + Rot[1]*Sy2 + Rot[2]*Sz + Off.x;
    W->Transformed_Vertices[I].Position[1] = Rot[3]*Sx + Rot[4]*Sy2 + Rot[5]*Sz + Off.y;
    W->Transformed_Vertices[I].Position[2] = Rot[6]*Sx + Rot[7]*Sy2 + Rot[8]*Sz + Off.z;
    float Nx = W->Model.Vertices[I].Normal[0], Ny = W->Model.Vertices[I].Normal[1], Nz = W->Model.Vertices[I].Normal[2];
    W->Transformed_Vertices[I].Normal[0] = Rot[0]*Nx + Rot[1]*Ny + Rot[2]*Nz;
    W->Transformed_Vertices[I].Normal[1] = Rot[3]*Nx + Rot[4]*Ny + Rot[5]*Nz;
    W->Transformed_Vertices[I].Normal[2] = Rot[6]*Nx + Rot[7]*Ny + Rot[8]*Nz;
    W->Transformed_Vertices[I].Texture_Uv[0] = W->Model.Vertices[I].Texture_Uv[0];
    W->Transformed_Vertices[I].Texture_Uv[1] = W->Model.Vertices[I].Texture_Uv[1];
  }
}

Input Poll_Input () {
  Input In = {0}; SDL_Event Ev;
  while (SDL_PollEvent (&Ev)) {
    if (Ev.type == SDL_QUIT) Quit = 1;
    if (Ev.type == SDL_KEYDOWN and Ev.key.keysym.sym == SDLK_ESCAPE) Quit = 1;
    if (Ev.type == SDL_MOUSEMOTION) { In.Delta_X += Ev.motion.xrel; In.Delta_Y += Ev.motion.yrel; }
    if (Ev.type == SDL_MOUSEBUTTONDOWN and Ev.button.button == SDL_BUTTON_LEFT) In.Fire = 1;
  }
  const uint8_t *Kb = SDL_GetKeyboardState (NULL);
  In.Forward = Kb[SDL_SCANCODE_W] or Kb[SDL_SCANCODE_UP];
  In.Back    = Kb[SDL_SCANCODE_S] or Kb[SDL_SCANCODE_DOWN];
  In.Left    = Kb[SDL_SCANCODE_A] or Kb[SDL_SCANCODE_LEFT];
  In.Right   = Kb[SDL_SCANCODE_D] or Kb[SDL_SCANCODE_RIGHT];
  In.Jump    = Kb[SDL_SCANCODE_SPACE];
  In.Crouch  = Kb[SDL_SCANCODE_LCTRL] or Kb[SDL_SCANCODE_C];
  return In;
}

void Raytracing_Frame () {
  VK_CHECK (vkWaitForFences (Device, 1, &Fence, VK_TRUE, UINT64_MAX));
  uint Img;
  VK_CHECK (vkAcquireNextImageKHR (Device, Swapchain, UINT64_MAX, Semaphore_Image_Available, VK_NULL_HANDLE, &Img));
  VK_CHECK (vkResetFences (Device, 1, &Fence));
  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (Command_Buffer, &(VkCommandBufferBeginInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}));

  vkCmdBindPipeline       (Command_Buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, Pipeline);
  vkCmdBindDescriptorSets (Command_Buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, Pipeline_Layout, 0, 1, &Descriptor_Set, 0, NULL);
  vkCmdTraceRays (Command_Buffer, &Shader_Binding_Ray_Generation, &Shader_Binding_Miss, &Shader_Binding_Hit, &Shader_Binding_Callable, Width, Height, 1);

  // Storage image → transfer source
  Image_Layout_Barrier (Command_Buffer, Raytracing_Storage_Image.Image,
    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
    VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Swapchain image → transfer destination
  Image_Layout_Barrier (Command_Buffer, Swapchain_Images[Img],
    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Blit ray tracing result to swapchain
  vkCmdBlitImage (Command_Buffer,
    Raytracing_Storage_Image.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    Swapchain_Images[Img], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1, &(VkImageBlit){
      .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}, .srcOffsets[1] = {Width, Height, 1},
      .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}, .dstOffsets[1] = {(int)Swapchain_Extent.width, (int)Swapchain_Extent.height, 1}},
    VK_FILTER_LINEAR);

  // Storage image back to general
  Image_Layout_Barrier (Command_Buffer, Raytracing_Storage_Image.Image,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
    VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

  // Swapchain image → present
  Image_Layout_Barrier (Command_Buffer, Swapchain_Images[Img],
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

  VK_CHECK (vkEndCommandBuffer (Command_Buffer));
  VkPipelineStageFlags Wait = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
  VK_CHECK (vkQueueSubmit (Queue, 1,
    &(VkSubmitInfo){.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1, .pWaitSemaphores = &Semaphore_Image_Available, .pWaitDstStageMask = &Wait,
      .commandBufferCount = 1, .pCommandBuffers = &Command_Buffer,
      .signalSemaphoreCount = 1, .pSignalSemaphores = &Semaphore_Render_Finished}, Fence));
  VK_CHECK (vkQueuePresentKHR (Queue,
    &(VkPresentInfoKHR){.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1, .pWaitSemaphores = &Semaphore_Render_Finished,
      .swapchainCount = 1, .pSwapchains = &Swapchain, .pImageIndices = &Img}));
  vkQueueWaitIdle (Queue);
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §. Physics
//
//   Six collider shapes (sphere, capsule, AABB, cylinder, ellipsoid, HULL), three ray strategies,
//   one grade cascade.  The support function s(d̂) maps unit directions to surface offsets:
//
//     SPHERE      s(d̂) = d̂ · r                                   Projectiles, pickups
//     CAPSULE     s(d̂) = d̂ · r + (0, sign(d̂.y) · spine, 0)      Player, NPCs
//     AABB        s(d̂) = sign(d̂) ⊙ extents                       Crates, elevators
//     CYLINDER    s(d̂) = (d̂.xz/‖d̂.xz‖ · r, sign(d̂.y) · h, 0)   Barrels, columns
//     ELLIPSOID   s(d̂) = normalize(d̂ ⊘ axes) ⊙ axes · ‖...‖     Vehicles
//     HULL        s(d̂) = argmax(v · d̂) over vertex set            Arbitrary convex models
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ── §11A. GPU Physics Types ──────────────────────────────────────────────────────────────────────

enum { SHAPE_SPHERE, SHAPE_CAPSULE, SHAPE_AABB, SHAPE_CYLINDER, SHAPE_ELLIPSOID, SHAPE_HULL };

typedef struct {                          // GPU-resident player state, std430, 112 bytes
  float Position     [3]; float Pad_A;
  float Velocity     [3]; float Pad_B;
  float Yaw, Pitch;
  int   On_Ground, Jump_Held;
  float Ground_Normal[3]; float Pad_C;
  int   Ground_Plane, Ducked;
  float View_Height, Stuck_Time;
  float Speed_Last;  int Shape;           // Active collider shape (0-5)
  float Extents      [3]; float Pad_D;    // Half-extents / radii / semi-axes (shape-dependent)
  float Spine;       float Pad_E [3];     // Capsule spine half-length (hh - radius), 0 for non-capsules
} Gpu_Player;

typedef struct {                          // Per-frame input via push constants, 48 bytes
  int   Forward, Back, Left, Right;
  int   Jump, Fire, Crouch, Pad;
  float Delta_X, Delta_Y, Dt, Pad2;
} Gpu_Input;

VkPipeline            Physics_Pipeline;
VkPipelineLayout      Physics_Pipeline_Layout;
VkDescriptorSetLayout Physics_Descriptor_Layout;
VkDescriptorPool      Physics_Descriptor_Pool;
VkDescriptorSet       Physics_Descriptor_Set;
Gpu_Buffer            Player_State_Buffer;

// ── §11B. Convex Hulls ──────────────────────────────────────────────────────────────────────────

#define HULL_MAX_VERTS    256
#define HULL_MAX_ADJ      16
#define HULL_MAX_FACES    512
#define HULL_MAX_ENTITIES 32

typedef struct {
  vec3  Vertices  [HULL_MAX_VERTS];
  int   Adjacency [HULL_MAX_VERTS][HULL_MAX_ADJ];
  uint  Vertex_Count;
  vec3  Centroid;
  float Bounding_Radius;
} Convex_Hull;

typedef struct {
  float Vertices [HULL_MAX_VERTS][4];
  int   Adjacency[HULL_MAX_VERTS][HULL_MAX_ADJ];
  int   Count;
  float Radius;
  float Centroid [3];
  int   Pad;
} Gpu_Hull;

Gpu_Buffer Hull_Storage_Buffer;

typedef struct { int A, B, C; int Dead; } QH_Face;
typedef struct { int V0, V1, Face; } QH_Edge;

static float QH_Dist (vec3 P, vec3 A, vec3 B, vec3 C) {
  vec3 N = Cross (Subtract (B, A), Subtract (C, A));
  float L = sqrtf (Dot (N, N));
  return L > 1e-8f ? Dot (Subtract (P, A), Scale (N, 1.f / L)) : 0;
}

Convex_Hull Quickhull (const vec3 *Pts, uint Cnt) {
  Convex_Hull R = {0};
  if (Cnt < 4) { for (uint i = 0; i < Cnt and i < HULL_MAX_VERTS; i++) R.Vertices[R.Vertex_Count++] = Pts[i]; return R; }

  // 6 extremal points → initial tetrahedron
  int Ex[6] = {0,0,0,0,0,0};
  for (uint i = 1; i < Cnt; i++) {
    if (Pts[i].x < Pts[Ex[0]].x) Ex[0]=i; if (Pts[i].x > Pts[Ex[1]].x) Ex[1]=i;
    if (Pts[i].y < Pts[Ex[2]].y) Ex[2]=i; if (Pts[i].y > Pts[Ex[3]].y) Ex[3]=i;
    if (Pts[i].z < Pts[Ex[4]].z) Ex[4]=i; if (Pts[i].z > Pts[Ex[5]].z) Ex[5]=i;
  }
  int P0=Ex[0], P1=Ex[1]; float Bd = 0;
  for (int i=0;i<6;i++) for (int j=i+1;j<6;j++) {
    float D = Dot(Subtract(Pts[Ex[i]],Pts[Ex[j]]),Subtract(Pts[Ex[i]],Pts[Ex[j]]));
    if (D > Bd) { Bd = D; P0 = Ex[i]; P1 = Ex[j]; }
  }
  vec3 Edg = Subtract(Pts[P1],Pts[P0]); float El2 = Dot(Edg,Edg);
  int P2=-1; Bd=0;
  for (uint i=0;i<Cnt;i++) {
    if ((int)i==P0 or (int)i==P1) continue;
    vec3 V = Subtract(Pts[i],Pts[P0]); float T = Dot(V,Edg)/El2;
    float D = Dot(Subtract(V,Scale(Edg,T)),Subtract(V,Scale(Edg,T)));
    if (D>Bd) { Bd=D; P2=i; }
  }
  if (P2<0) P2=(P0+1)%Cnt;
  int P3=-1; Bd=0;
  for (uint i=0;i<Cnt;i++) {
    if ((int)i==P0 or (int)i==P1 or (int)i==P2) continue;
    float D = fabsf(QH_Dist(Pts[i],Pts[P0],Pts[P1],Pts[P2]));
    if (D>Bd) { Bd=D; P3=i; }
  }
  if (P3<0) P3=(P2+1)%Cnt;
  if (QH_Dist(Pts[P3],Pts[P0],Pts[P1],Pts[P2])>0) { int T=P0; P0=P1; P1=T; }

  QH_Face Fc[HULL_MAX_FACES]; int Fn=0;
  Fc[Fn++]=(QH_Face){P0,P1,P2,0}; Fc[Fn++]=(QH_Face){P0,P2,P3,0};
  Fc[Fn++]=(QH_Face){P0,P3,P1,0}; Fc[Fn++]=(QH_Face){P1,P3,P2,0};

  int *As = calloc(Cnt,sizeof(int));
  for (uint i=0;i<Cnt;i++) As[i]=-1;
  for (uint i=0;i<Cnt;i++) {
    if ((int)i==P0 or (int)i==P1 or (int)i==P2 or (int)i==P3) continue;
    float Best=0;
    for (int f=0;f<Fn;f++) { if (Fc[f].Dead) continue;
      float D = QH_Dist(Pts[i],Pts[Fc[f].A],Pts[Fc[f].B],Pts[Fc[f].C]);
      if (D>Best) { Best=D; As[i]=f; }
    }
  }

  for (int It=0; It<(int)Cnt and Fn<HULL_MAX_FACES-20; It++) {
    int Bf=-1,Bp=-1; Bd=0;
    for (uint i=0;i<Cnt;i++) { if (As[i]<0 or Fc[As[i]].Dead) continue;
      float D = QH_Dist(Pts[i],Pts[Fc[As[i]].A],Pts[Fc[As[i]].B],Pts[Fc[As[i]].C]);
      if (D>Bd) { Bd=D; Bf=As[i]; Bp=i; }
    }
    if (Bp<0) break;

    int Vis[HULL_MAX_FACES]; int Vc2=0;
    for (int f=0;f<Fn;f++) { if (Fc[f].Dead) continue;
      if (QH_Dist(Pts[Bp],Pts[Fc[f].A],Pts[Fc[f].B],Pts[Fc[f].C])>1e-6f) Vis[Vc2++]=f;
    }

    QH_Edge Hz[HULL_MAX_FACES*3]; int Hc=0;
    for (int vi=0;vi<Vc2;vi++) { int f=Vis[vi];
      int Tr[3][2]={{Fc[f].A,Fc[f].B},{Fc[f].B,Fc[f].C},{Fc[f].C,Fc[f].A}};
      for (int e=0;e<3;e++) { int Sh=0;
        for (int vj=0;vj<Vc2;vj++) { if (vj==vi) continue; int g=Vis[vj];
          int Fv[3]={Fc[g].A,Fc[g].B,Fc[g].C}; int H0=0,H1=0;
          for (int k=0;k<3;k++) { H0|=(Fv[k]==Tr[e][0]); H1|=(Fv[k]==Tr[e][1]); }
          if (H0 and H1) { Sh=1; break; }
        }
        if (not Sh) Hz[Hc++]=(QH_Edge){Tr[e][0],Tr[e][1],f};
      }
    }
    for (int vi=0;vi<Vc2;vi++) Fc[Vis[vi]].Dead=1;
    int Ns=Fn;
    for (int hi=0;hi<Hc and Fn<HULL_MAX_FACES;hi++) Fc[Fn++]=(QH_Face){Hz[hi].V1,Hz[hi].V0,Bp,0};
    As[Bp]=-1;
    for (uint i=0;i<Cnt;i++) { if (As[i]<0) continue; if (not Fc[As[i]].Dead) continue;
      As[i]=-1; float Best=0;
      for (int f=Ns;f<Fn;f++) { float D=QH_Dist(Pts[i],Pts[Fc[f].A],Pts[Fc[f].B],Pts[Fc[f].C]); if (D>Best){Best=D;As[i]=f;} }
    }
  }
  free(As);

  // Extract unique vertices
  int Rm[HULL_MAX_FACES*3]; memset(Rm,-1,sizeof Rm);
  for (int f=0;f<Fn;f++) { if (Fc[f].Dead) continue;
    int Tr[3]={Fc[f].A,Fc[f].B,Fc[f].C};
    for (int k=0;k<3;k++) if (Tr[k]>=0 and Tr[k]<(int)Cnt and Rm[Tr[k]]<0 and R.Vertex_Count<HULL_MAX_VERTS) {
      Rm[Tr[k]]=(int)R.Vertex_Count; R.Vertices[R.Vertex_Count++]=Pts[Tr[k]];
    }
  }
  // Build adjacency
  memset(R.Adjacency,-1,sizeof R.Adjacency);
  for (int f=0;f<Fn;f++) { if (Fc[f].Dead) continue;
    int Rv[3]={Rm[Fc[f].A],Rm[Fc[f].B],Rm[Fc[f].C]};
    for (int e=0;e<3;e++) { int V0=Rv[e],V1=Rv[(e+1)%3]; if (V0<0 or V1<0) continue;
      for (int a=0;a<HULL_MAX_ADJ;a++) { if (R.Adjacency[V0][a]==V1) break; if (R.Adjacency[V0][a]==-1){R.Adjacency[V0][a]=V1;break;} }
      for (int a=0;a<HULL_MAX_ADJ;a++) { if (R.Adjacency[V1][a]==V0) break; if (R.Adjacency[V1][a]==-1){R.Adjacency[V1][a]=V0;break;} }
    }
  }
  // Centroid and bounding radius
  R.Centroid = Make(0,0,0);
  for (uint i=0;i<R.Vertex_Count;i++) R.Centroid = Add(R.Centroid,R.Vertices[i]);
  if (R.Vertex_Count) R.Centroid = Scale(R.Centroid, 1.f/R.Vertex_Count);
  R.Bounding_Radius = 0;
  for (uint i=0;i<R.Vertex_Count;i++) {
    float D = Dot(Subtract(R.Vertices[i],R.Centroid),Subtract(R.Vertices[i],R.Centroid));
    if (D > R.Bounding_Radius*R.Bounding_Radius) R.Bounding_Radius = sqrtf(D);
  }
  printf("[hull] %u vertices, radius %.1f\n", R.Vertex_Count, R.Bounding_Radius);
  return R;
}

Convex_Hull Hull_From_Vertices (const Vertex *V, uint C) {
  vec3 *P = malloc(sizeof(vec3)*C);
  for (uint i=0;i<C;i++) P[i] = Make(V[i].Position[0],V[i].Position[1],V[i].Position[2]);
  Convex_Hull H = Quickhull(P,C); free(P); return H;
}

void Hull_Upload (const Convex_Hull *H) {
  Gpu_Hull Pk = {0};
  Pk.Count = (int)H->Vertex_Count; Pk.Radius = H->Bounding_Radius;
  Pk.Centroid[0]=H->Centroid.x; Pk.Centroid[1]=H->Centroid.y; Pk.Centroid[2]=H->Centroid.z;
  for (uint i=0;i<H->Vertex_Count;i++) {
    Pk.Vertices[i][0]=H->Vertices[i].x; Pk.Vertices[i][1]=H->Vertices[i].y;
    Pk.Vertices[i][2]=H->Vertices[i].z; Pk.Vertices[i][3]=0;
    memcpy(Pk.Adjacency[i],H->Adjacency[i],sizeof(int)*HULL_MAX_ADJ);
  }
  if (not Hull_Storage_Buffer.Buffer)
    Hull_Storage_Buffer = Buffer_Allocate(sizeof(Gpu_Hull),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  Buffer_Upload(Hull_Storage_Buffer,&Pk,sizeof Pk);
}

// ── §11C. GPU Physics Pipeline (with hull binding 4) ─────────────────────────────────────────────

void Physics_Pipeline_Create () {
  VkDescriptorSetLayoutBinding B[] = {
    {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},
    {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},  // Vertex buffer
    {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},  // Index buffer
    {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},  // Player state
    {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},  // Hull data
  };
  VK_CHECK (vkCreateDescriptorSetLayout (Device,
    &(VkDescriptorSetLayoutCreateInfo){.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 5, .pBindings = B}, NULL, &Physics_Descriptor_Layout));
  VkPushConstantRange Push = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Gpu_Input)};
  VK_CHECK (vkCreatePipelineLayout (Device,
    &(VkPipelineLayoutCreateInfo){.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1, .pSetLayouts = &Physics_Descriptor_Layout,
      .pushConstantRangeCount = 1, .pPushConstantRanges = &Push}, NULL, &Physics_Pipeline_Layout));
  VkShaderModule M = Shader_Module_Load (SHADER_PATH_PHYSICS_COMPUTE);
  VK_CHECK (vkCreateComputePipelines (Device, VK_NULL_HANDLE, 1,
    &(VkComputePipelineCreateInfo){.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_COMPUTE_BIT, M, "main", NULL},
      .layout = Physics_Pipeline_Layout}, NULL, &Physics_Pipeline));
  vkDestroyShaderModule (Device, M, NULL);
}

void Physics_Resources_Create (const Player *Init) {
  Player_State_Buffer = Buffer_Allocate (sizeof(Gpu_Player),
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  Gpu_Player S = {
    .Position = {Init->Position.x, Init->Position.y, Init->Position.z},
    .Yaw = Init->Yaw, .Pitch = Init->Pitch, .View_Height = DEFAULT_VIEW_HEIGHT,
    .Shape = SHAPE_CAPSULE, .Extents = {15,32,15}, .Spine = 17};
  Buffer_Upload (Player_State_Buffer, &S, sizeof S);

  // Initialize hull buffer with a 1-vertex dummy (replaced when a real hull is loaded)
  if (not Hull_Storage_Buffer.Buffer) {
    Hull_Storage_Buffer = Buffer_Allocate (sizeof(Gpu_Hull),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    Gpu_Hull Empty = {0}; Empty.Count = 1;
    Buffer_Upload (Hull_Storage_Buffer, &Empty, sizeof Empty);
  }

  VkDescriptorPoolSize Sz[] = {{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,1}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,4}};
  VK_CHECK (vkCreateDescriptorPool (Device,
    &(VkDescriptorPoolCreateInfo){.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 1, .poolSizeCount = 2, .pPoolSizes = Sz}, NULL, &Physics_Descriptor_Pool));
  VK_CHECK (vkAllocateDescriptorSets (Device,
    &(VkDescriptorSetAllocateInfo){.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = Physics_Descriptor_Pool, .descriptorSetCount = 1,
      .pSetLayouts = &Physics_Descriptor_Layout}, &Physics_Descriptor_Set));

  VkWriteDescriptorSetAccelerationStructureKHR Aw = {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
    .accelerationStructureCount = 1, .pAccelerationStructures = &Top_Level.Handle};
  VkDescriptorBufferInfo Vb = {Vertex_Buffer.Buffer, 0, Vertex_Buffer.Size};
  VkDescriptorBufferInfo Ib = {Index_Buffer.Buffer, 0, Index_Buffer.Size};
  VkDescriptorBufferInfo Pb = {Player_State_Buffer.Buffer, 0, Player_State_Buffer.Size};
  VkDescriptorBufferInfo Hb = {Hull_Storage_Buffer.Buffer, 0, Hull_Storage_Buffer.Size};
  VkWriteDescriptorSet W[] = {
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, &Aw,  Physics_Descriptor_Set, 0,0,1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, NULL, NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Physics_Descriptor_Set, 1,0,1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &Vb},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Physics_Descriptor_Set, 2,0,1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &Ib},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Physics_Descriptor_Set, 3,0,1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &Pb},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Physics_Descriptor_Set, 4,0,1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &Hb},
  };
  vkUpdateDescriptorSets (Device, 5, W, 0, NULL);
}

Player Physics_Dispatch (Input In, float Dt) {
  Gpu_Input G = {In.Forward, In.Back, In.Left, In.Right, In.Jump, In.Fire, In.Crouch, 0, In.Delta_X, In.Delta_Y, Dt, 0};

  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (Command_Buffer,
    &(VkCommandBufferBeginInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));
  vkCmdBindPipeline       (Command_Buffer, VK_PIPELINE_BIND_POINT_COMPUTE, Physics_Pipeline);
  vkCmdBindDescriptorSets (Command_Buffer, VK_PIPELINE_BIND_POINT_COMPUTE, Physics_Pipeline_Layout, 0, 1, &Physics_Descriptor_Set, 0, NULL);
  vkCmdPushConstants      (Command_Buffer, Physics_Pipeline_Layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof G, &G);
  vkCmdDispatch           (Command_Buffer, 1, 1, 1);

  // Memory barrier: compute writes → host read
  vkCmdPipelineBarrier (Command_Buffer,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
    1, &(VkMemoryBarrier){.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT, .dstAccessMask = VK_ACCESS_HOST_READ_BIT},
    0, NULL, 0, NULL);

  VK_CHECK (vkEndCommandBuffer (Command_Buffer));
  VK_CHECK (vkQueueSubmit (Queue, 1,
    &(VkSubmitInfo){.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &Command_Buffer}, VK_NULL_HANDLE));
  VK_CHECK (vkQueueWaitIdle (Queue));

  // Read back the updated player state from the GPU
  Gpu_Player *Mapped;
  vkMapMemory (Device, Player_State_Buffer.Memory, 0, sizeof(Gpu_Player), 0, (void**)&Mapped);
  Player Result = {
    .Position    = Make (Mapped->Position[0], Mapped->Position[1], Mapped->Position[2]),
    .Velocity    = Make (Mapped->Velocity[0], Mapped->Velocity[1], Mapped->Velocity[2]),
    .Yaw         = Mapped->Yaw,
    .Pitch       = Mapped->Pitch,
    .On_Ground   = Mapped->On_Ground,
    .View_Height = Mapped->View_Height};
  vkUnmapMemory (Device, Player_State_Buffer.Memory);
  return Result;
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §13. Shaders — GLSL Source (compiled offline to SPIR-V)
//
// The four shader stages (Ray_Generation, Closest_Hit, Primary_Miss, Shadow_Miss) and the physics
// compute shader (with hull support functions) are defined as glsl blocks in the specification.
// They are compiled to SPIR-V offline using glslangValidator and loaded at runtime via
// Shader_Module_Load from the SHADER_PATH_* macros.
//
// The physics compute shader includes:
//   - Binding 4: hull storage buffer (Gpu_Hull layout)
//   - hull_support_brute(): O(n) linear scan for small hulls (< 64 verts)
//   - hull_support_hill():  O(√n) amortized hill-climbing with adjacency table
//   - hull_support():       adaptive dispatcher (brute for n<64, hill for n≥64)
//   - shape_offset() case 5: delegates to hull_support() for SHAPE_HULL
//   - hull adds 2 extra sweep rays in the movement and anti-movement directions
//   - recover() uses hull support distance as expected clearance per cardinal direction
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §15. Main
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

int main (int Argc, char **Argv) {
  const char *Map_Path = (Argc > 1) ? Argv[1] : DEFAULT_MAP_PATH;

  // Initialize SDL2 with video subsystem and create a Vulkan-capable window
  SDL_Init (SDL_INIT_VIDEO);
  SDL_Window *Window = SDL_CreateWindow (ENGINE_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                         Width, Height, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
  SDL_SetRelativeMouseMode (SDL_TRUE);

  // Create the Vulkan instance, surface, pick a physical device, and create the logical device
  Vulkan_Create_Instance   (Window);
  SDL_Vulkan_CreateSurface (Window, Instance, &Surface);
  Vulkan_Pick_Physical_Device ();
  Vulkan_Create_Logical_Device ();

  // Create the swapchain and synchronization primitives
  Vulkan_Create_Swapchain ();
  Vulkan_Create_Synchronization ();

  // Create the ray tracing storage image (render target)
  Raytracing_Storage_Image = Image_Storage_Create (Width, Height, VK_FORMAT_R8G8B8A8_UNORM);
  Vulkan_Transition_Storage_Image ();

  // Allocate the camera uniform buffer
  Camera_Uniform_Buffer = Buffer_Allocate (sizeof (mat4) * 2 + 16,
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Load the BSP scene, spawn point, and collision map
  Spawn        Spawn_Point;
  Collision_Map Collision;
  Scene        Scene_Data = Scene_Load_From_BSP (Map_Path, &Spawn_Point, &Collision);

  // Load scene and weapon textures
  Scene_Load_Textures (&Scene_Data);
  Weapon_Instance Weapon = {0};
  Weapon_Model_Load (&Weapon);
  Weapon_Load_Textures (&Weapon);

  // Build acceleration structures (BLAS for world + weapon, then TLAS)
  Acceleration_Structure World_Bottom_Level = Build_World_Bottom_Level (&Scene_Data);
  Weapon_Bottom_Level_Initialize (&Weapon);
  Top_Level_Initialize (2);
  Top_Level_Rebuild (&World_Bottom_Level, &Weapon.Bottom_Level);

  // Create the ray tracing pipeline, SBT, and descriptors
  Raytracing_Pipeline_Create ();
  Shader_Binding_Table_Create ();
  Descriptor_Set_Create (&Weapon);

  // Create the GPU physics pipeline and resources (with hull binding)
  Physics_Pipeline_Create ();
  Player Initial_Player = {.Position = Spawn_Point.Origin, .Yaw = Spawn_Point.Angle * 3.14159f / 180.f};
  Physics_Resources_Create (&Initial_Player);

  // Optionally build a convex hull from the weapon model for hull-based collision testing
  // Convex_Hull Projectile_Hull = Hull_From_Vertices (Weapon.Model.Vertices, Weapon.Model.Vertex_Count);
  // Hull_Upload (&Projectile_Hull);

  printf ("[init] ready — entering game loop\n");

  // ── Game loop ──────────────────────────────────────────────────────────────────────────────────
  Camera    Cam    = {.Position = Spawn_Point.Origin, .Yaw = Spawn_Point.Angle * 3.14159f / 180.f};
  uint64_t  Last   = SDL_GetPerformanceCounter ();
  uint64_t  Freq   = SDL_GetPerformanceFrequency ();
  uint      Frame  = 0;
  Quit = 0;

  while (not Quit) {

    // Compute delta time from the high-resolution performance counter
    uint64_t Now = SDL_GetPerformanceCounter ();
    float    Dt  = (float)(Now - Last) / (float)Freq;
    if (Dt > 0.05f) Dt = 0.05f; // Clamp to 20fps minimum
    Last = Now;

    // Poll input and dispatch GPU physics
    Input In   = Poll_Input ();
    Player Phy = Physics_Dispatch (In, Dt);

    // Update the camera from the physics result
    Cam.Position = Phy.Position;
    Cam.Yaw      = Phy.Yaw;
    Cam.Pitch    = Phy.Pitch;
    Cam.Frame    = Frame;

    // Animate and rebuild the weapon viewmodel
    Weapon_Update (&Weapon, &Cam, Dt, In.Fire);
    Weapon_Bottom_Level_Rebuild (&Weapon);
    Top_Level_Rebuild (&World_Bottom_Level, &Weapon.Bottom_Level);

    // Upload the camera and dispatch ray tracing
    Camera_Upload (&Cam, 90.f, Weapon.Texture_Base_Index);
    Raytracing_Frame ();
    Frame++;
  }

  // ── Cleanup ────────────────────────────────────────────────────────────────────────────────────
  vkDeviceWaitIdle (Device);
  printf ("[shutdown] %u frames rendered\n", Frame);

  // Free scene data
  free (Scene_Data.Vertices);   free (Scene_Data.Indices);
  free (Scene_Data.Materials);  free (Scene_Data.Texture_Ids);
  free (Scene_Data.Texture_Names);
  if (Scene_Data.Lightmap_Atlas) free (Scene_Data.Lightmap_Atlas);

  // Free collision map
  free (Collision.Planes);   free (Collision.Nodes);
  free (Collision.Leafs);    free (Collision.Leaf_Brushes);
  free (Collision.Brushes);  free (Collision.Sides);
  free (Collision.Shader_Contents); free (Collision.Brush_Checks);

  // Destroy Vulkan objects (abbreviated — production code would destroy every handle)
  vkDestroyPipeline       (Device, Pipeline, NULL);
  vkDestroyPipelineLayout (Device, Pipeline_Layout, NULL);
  vkDestroyPipeline       (Device, Physics_Pipeline, NULL);
  vkDestroyPipelineLayout (Device, Physics_Pipeline_Layout, NULL);
  vkDestroyDescriptorPool (Device, Descriptor_Pool, NULL);
  vkDestroyDescriptorPool (Device, Physics_Descriptor_Pool, NULL);
  vkDestroyDescriptorSetLayout (Device, Descriptor_Set_Layout, NULL);
  vkDestroyDescriptorSetLayout (Device, Physics_Descriptor_Layout, NULL);
  vkDestroySemaphore (Device, Semaphore_Image_Available, NULL);
  vkDestroySemaphore (Device, Semaphore_Render_Finished, NULL);
  vkDestroyFence     (Device, Fence, NULL);
  vkDestroyCommandPool (Device, Command_Pool, NULL);
  vkDestroySwapchainKHR (Device, Swapchain, NULL);
  vkDestroyDevice    (Device, NULL);
  vkDestroySurfaceKHR (Instance, Surface, NULL);
  vkDestroyInstance   (Instance, NULL);
  SDL_DestroyWindow (Window);
  SDL_Quit ();
  return 0;
}
