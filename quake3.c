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

// Forward-declared constants used in type definitions below
#define BSP_LUMP_COUNT       17  // Total number of lumps in the BSP directory
#define MD3_MAX_SURFACES     3   // Maximum surfaces per weapon part (body, barrel, hand)
#define MD3_MAX_ANIM_FRAMES  30  // Maximum animation frames extracted from tag_weapon

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §2. Types
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Three-component floating-point vector for positions, directions, and velocities
typedef struct {float x, y, z;} vec3;

// Player bounding box minimum corner (symmetric on X/Z, asymmetric on Y for feet)
const vec3 PLAYER_MINIMUMS = {-15, -24, -15};

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
#define VK_CHECK(Call) do { \
  VkResult _Result = (Call); \
  if (_Result) {fprintf (stderr, "[vulkan] error %d at %s:%d\n", _Result, __FILE__, __LINE__); exit (1);} \
} while (0)

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
                                .ppEnabledLayerNames     = VALIDATION_LAYERS,
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

Vertex Convert_BSP_Vertex (const BSP_Vertex *Source) {

  // Swizzle from Quake 3's Z-up coordinate system to our Y-up system: (x, y, z) becomes (x, z, -y)
  return (Vertex){
    .Position    = {Source->Position[0],        Source->Position[2],       -Source->Position[1]},
    .Texture_Uv  = {Source->Texture_Coords[0],  Source->Texture_Coords[1]},
    .Lightmap_Uv = {Source->Lightmap_Coords[0], Source->Lightmap_Coords[1]},
    .Normal      = {Source->Normal[0],          Source->Normal[2],         -Source->Normal[1]}
  };
}

vec3 Bezier_Evaluate (vec3 Control_A, vec3 Control_B, vec3 Control_C, float Parameter) {

  // Evaluate the quadratic Bézier curve: B(t) = (1-t)²·A + 2(1-t)t·B + t²·C
  float Inverse = 1.f - Parameter;
  return Add (Add (Scale (Control_A, Inverse * Inverse),
                   Scale (Control_B, 2.f * Inverse * Parameter)),
              Scale (Control_C, Parameter * Parameter));
}

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

Scene Scene_Load_From_BSP (const char *Path, Spawn *Out_Spawn) {

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
  uint New_Total = Texture_Count + WEAPON_TEXTURE_COUNT;
  Texture_Images   = realloc (Texture_Images,   sizeof (VkImage)        * New_Total);
  Texture_Memories = realloc (Texture_Memories,  sizeof (VkDeviceMemory) * New_Total);
  Texture_Views    = realloc (Texture_Views,     sizeof (VkImageView)    * New_Total);

  // Load each weapon texture from TGA, or create a grey fallback pixel
  for (uint Index = 0; Index < WEAPON_TEXTURE_COUNT; Index++) {
    uint Width = 0, Height = 0;
    uint8_t *Pixels = TGA_Load (WEAPON_TEXTURE_PATHS[Index], &Width, &Height);
    if (Pixels and Width and Height) {
      Texture_Upload (Command_Buffer, Queue, Pixels, Width, Height,
                      &Texture_Images[Texture_Count], &Texture_Memories[Texture_Count], &Texture_Views[Texture_Count]);
      free (Pixels);
      printf ("[weapon] loaded texture %s (%ux%u)\n", WEAPON_TEXTURE_PATHS[Index], Width, Height);

    // Texture file not found or corrupt — use a neutral grey fallback
    } else {
      uint8_t Fallback[4] = {180, 180, 180, 255};
      Texture_Upload (Command_Buffer, Queue, Fallback, 1, 1,
                      &Texture_Images[Texture_Count], &Texture_Memories[Texture_Count], &Texture_Views[Texture_Count]);
      printf ("[weapon] fallback texture for %s\n", WEAPON_TEXTURE_PATHS[Index]);
    }
    Texture_Count++;
  }
  printf ("[weapon] textures: base=%u, count=%u\n", Weapon->Texture_Base_Index, WEAPON_TEXTURE_COUNT);
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
// §10. Physics
//
//   Six collider shapes (sphere, capsule, AABB, cylinder, ellipsoid, HULL), three ray strategies,
//   one grade cascade.  The support function s(d) maps unit directions to surface offsets:
//
//     SPHERE      s(d) = d * r                                   Projectiles, pickups
//     CAPSULE     s(d) = d * r + (0, sign(d.y) * spine, 0)      Player, NPCs
//     AABB        s(d) = sign(d) * extents                       Crates, elevators
//     CYLINDER    s(d) = (d.xz/||d.xz|| * r, sign(d.y) * h, 0) Barrels, columns
//     ELLIPSOID   s(d) = normalize(d / axes) * axes              Vehicles
//     HULL        s(d) = argmax(v . d) over vertex set           Arbitrary convex models
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ── §10A. Convex Hull Construction ────────────────────────────────────────────────────────────────

static float QH_Dist (vec3 P, vec3 A, vec3 B, vec3 C) {

  // Compute the signed distance from point P to the plane defined by triangle (A, B, C)
  vec3 Normal = Cross (Subtract (B, A), Subtract (C, A));
  float Length = sqrtf (Dot (Normal, Normal));
  return Length > 1e-8f ? Dot (Subtract (P, A), Scale (Normal, 1.f / Length)) : 0;
}

Convex_Hull Quickhull (const vec3 *Points, uint Count) {

  Convex_Hull Result = {0};

  // Degenerate case: fewer than 4 points cannot form a tetrahedron
  if (Count < 4) {
    for (uint Index = 0; Index < Count and Index < HULL_MAX_VERTS; Index++)
      Result.Vertices[Result.Vertex_Count++] = Points[Index];
    return Result;
  }

  // ── Find 6 extremal points (min/max along each axis) ──────────────────────────────────────────

  int Extremals[6] = {0, 0, 0, 0, 0, 0};
  for (uint Index = 1; Index < Count; Index++) {
    if (Points[Index].x < Points[Extremals[0]].x) Extremals[0] = Index;
    if (Points[Index].x > Points[Extremals[1]].x) Extremals[1] = Index;
    if (Points[Index].y < Points[Extremals[2]].y) Extremals[2] = Index;
    if (Points[Index].y > Points[Extremals[3]].y) Extremals[3] = Index;
    if (Points[Index].z < Points[Extremals[4]].z) Extremals[4] = Index;
    if (Points[Index].z > Points[Extremals[5]].z) Extremals[5] = Index;
  }

  // ── Select the most distant pair as the initial edge ──────────────────────────────────────────

  int Point_0 = Extremals[0], Point_1 = Extremals[1];
  float Best_Distance = 0;
  for (int I = 0; I < 6; I++)
    for (int J = I + 1; J < 6; J++) {
      float Distance = Dot (Subtract (Points[Extremals[I]], Points[Extremals[J]]),
                            Subtract (Points[Extremals[I]], Points[Extremals[J]]));
      if (Distance > Best_Distance) { Best_Distance = Distance; Point_0 = Extremals[I]; Point_1 = Extremals[J]; }
    }

  // ── Find the third point: most distant from the initial edge ──────────────────────────────────

  vec3  Edge        = Subtract (Points[Point_1], Points[Point_0]);
  float Edge_Length2 = Dot (Edge, Edge);
  int   Point_2     = -1;
  Best_Distance     = 0;

  for (uint Index = 0; Index < Count; Index++) {
    if ((int)Index == Point_0 or (int)Index == Point_1) continue;
    vec3  Vector    = Subtract (Points[Index], Points[Point_0]);
    float Parameter = Dot (Vector, Edge) / Edge_Length2;
    float Distance  = Dot (Subtract (Vector, Scale (Edge, Parameter)),
                           Subtract (Vector, Scale (Edge, Parameter)));
    if (Distance > Best_Distance) { Best_Distance = Distance; Point_2 = Index; }
  }
  if (Point_2 < 0) Point_2 = (Point_0 + 1) % Count;

  // ── Find the fourth point: most distant from the initial triangle ─────────────────────────────

  int Point_3    = -1;
  Best_Distance  = 0;
  for (uint Index = 0; Index < Count; Index++) {
    if ((int)Index == Point_0 or (int)Index == Point_1 or (int)Index == Point_2) continue;
    float Distance = fabsf (QH_Dist (Points[Index], Points[Point_0], Points[Point_1], Points[Point_2]));
    if (Distance > Best_Distance) { Best_Distance = Distance; Point_3 = Index; }
  }
  if (Point_3 < 0) Point_3 = (Point_2 + 1) % Count;

  // Ensure the tetrahedron has consistent winding (fourth point on the negative side)
  if (QH_Dist (Points[Point_3], Points[Point_0], Points[Point_1], Points[Point_2]) > 0) {
    int Temp = Point_0; Point_0 = Point_1; Point_1 = Temp;
  }

  // ── Initialize the face array with the 4 tetrahedron faces ────────────────────────────────────

  QH_Face Faces[HULL_MAX_FACES];
  int     Face_Count = 0;

  Faces[Face_Count++] = (QH_Face){Point_0, Point_1, Point_2, 0};
  Faces[Face_Count++] = (QH_Face){Point_0, Point_2, Point_3, 0};
  Faces[Face_Count++] = (QH_Face){Point_0, Point_3, Point_1, 0};
  Faces[Face_Count++] = (QH_Face){Point_1, Point_3, Point_2, 0};

  // ── Assign each remaining point to the face it lies farthest above ────────────────────────────

  int *Assignments = calloc (Count, sizeof (int));
  for (uint Index = 0; Index < Count; Index++) Assignments[Index] = -1;

  for (uint Index = 0; Index < Count; Index++) {
    if ((int)Index == Point_0 or (int)Index == Point_1 or (int)Index == Point_2 or (int)Index == Point_3) continue;
    float Best = 0;
    for (int Face = 0; Face < Face_Count; Face++) {
      if (Faces[Face].Dead) continue;
      float Distance = QH_Dist (Points[Index], Points[Faces[Face].A], Points[Faces[Face].B], Points[Faces[Face].C]);
      if (Distance > Best) { Best = Distance; Assignments[Index] = Face; }
    }
  }

  // ── Iteratively expand the hull ───────────────────────────────────────────────────────────────

  for (int Iteration = 0; Iteration < (int)Count and Face_Count < HULL_MAX_FACES - 20; Iteration++) {

    // Find the point with the greatest distance above its assigned face
    int   Best_Face  = -1, Best_Point = -1;
    Best_Distance = 0;
    for (uint Index = 0; Index < Count; Index++) {
      if (Assignments[Index] < 0 or Faces[Assignments[Index]].Dead) continue;
      float Distance = QH_Dist (Points[Index],
                                Points[Faces[Assignments[Index]].A],
                                Points[Faces[Assignments[Index]].B],
                                Points[Faces[Assignments[Index]].C]);
      if (Distance > Best_Distance) { Best_Distance = Distance; Best_Face = Assignments[Index]; Best_Point = Index; }
    }
    if (Best_Point < 0) break;

    // Find all faces visible from the selected point
    int Visible[HULL_MAX_FACES];
    int Visible_Count = 0;
    for (int Face = 0; Face < Face_Count; Face++) {
      if (Faces[Face].Dead) continue;
      if (QH_Dist (Points[Best_Point], Points[Faces[Face].A], Points[Faces[Face].B], Points[Faces[Face].C]) > 1e-6f)
        Visible[Visible_Count++] = Face;
    }

    // Extract the horizon edges (edges shared between one visible and one non-visible face)
    QH_Edge Horizon[HULL_MAX_FACES * 3];
    int     Horizon_Count = 0;

    for (int Vi = 0; Vi < Visible_Count; Vi++) {
      int Face_Index = Visible[Vi];
      int Triangle[3][2] = {{Faces[Face_Index].A, Faces[Face_Index].B},
                            {Faces[Face_Index].B, Faces[Face_Index].C},
                            {Faces[Face_Index].C, Faces[Face_Index].A}};

      for (int Edge = 0; Edge < 3; Edge++) {
        int Shared = 0;
        for (int Vj = 0; Vj < Visible_Count; Vj++) {
          if (Vj == Vi) continue;
          int Other = Visible[Vj];
          int Face_Vertices[3] = {Faces[Other].A, Faces[Other].B, Faces[Other].C};
          int Has_0 = 0, Has_1 = 0;
          for (int K = 0; K < 3; K++) { Has_0 |= (Face_Vertices[K] == Triangle[Edge][0]); Has_1 |= (Face_Vertices[K] == Triangle[Edge][1]); }
          if (Has_0 and Has_1) { Shared = 1; break; }
        }
        if (not Shared) Horizon[Horizon_Count++] = (QH_Edge){Triangle[Edge][0], Triangle[Edge][1], Face_Index};
      }
    }

    // Mark all visible faces as dead
    for (int Vi = 0; Vi < Visible_Count; Vi++) Faces[Visible[Vi]].Dead = 1;

    // Create new faces connecting each horizon edge to the new point
    int New_Start = Face_Count;
    for (int Hi = 0; Hi < Horizon_Count and Face_Count < HULL_MAX_FACES; Hi++)
      Faces[Face_Count++] = (QH_Face){Horizon[Hi].V1, Horizon[Hi].V0, Best_Point, 0};

    // Reassign orphaned points to the new faces
    Assignments[Best_Point] = -1;
    for (uint Index = 0; Index < Count; Index++) {
      if (Assignments[Index] < 0) continue;
      if (not Faces[Assignments[Index]].Dead) continue;
      Assignments[Index] = -1;
      float Best = 0;
      for (int Face = New_Start; Face < Face_Count; Face++) {
        float Distance = QH_Dist (Points[Index], Points[Faces[Face].A], Points[Faces[Face].B], Points[Faces[Face].C]);
        if (Distance > Best) { Best = Distance; Assignments[Index] = Face; }
      }
    }
  }
  free (Assignments);

  // ── Extract unique vertices from surviving faces ──────────────────────────────────────────────

  int Remap[HULL_MAX_FACES * 3];
  memset (Remap, -1, sizeof Remap);

  for (int Face = 0; Face < Face_Count; Face++) {
    if (Faces[Face].Dead) continue;
    int Triangle[3] = {Faces[Face].A, Faces[Face].B, Faces[Face].C};
    for (int K = 0; K < 3; K++)
      if (Triangle[K] >= 0 and Triangle[K] < (int)Count and Remap[Triangle[K]] < 0 and Result.Vertex_Count < HULL_MAX_VERTS) {
        Remap[Triangle[K]] = (int)Result.Vertex_Count;
        Result.Vertices[Result.Vertex_Count++] = Points[Triangle[K]];
      }
  }

  // ── Build per-vertex adjacency table ──────────────────────────────────────────────────────────

  memset (Result.Adjacency, -1, sizeof Result.Adjacency);
  for (int Face = 0; Face < Face_Count; Face++) {
    if (Faces[Face].Dead) continue;
    int Remapped[3] = {Remap[Faces[Face].A], Remap[Faces[Face].B], Remap[Faces[Face].C]};
    for (int Edge = 0; Edge < 3; Edge++) {
      int Vertex_0 = Remapped[Edge], Vertex_1 = Remapped[(Edge + 1) % 3];
      if (Vertex_0 < 0 or Vertex_1 < 0) continue;
      for (int Slot = 0; Slot < HULL_MAX_ADJ; Slot++) {
        if (Result.Adjacency[Vertex_0][Slot] == Vertex_1) break;
        if (Result.Adjacency[Vertex_0][Slot] == -1) { Result.Adjacency[Vertex_0][Slot] = Vertex_1; break; }
      }
      for (int Slot = 0; Slot < HULL_MAX_ADJ; Slot++) {
        if (Result.Adjacency[Vertex_1][Slot] == Vertex_0) break;
        if (Result.Adjacency[Vertex_1][Slot] == -1) { Result.Adjacency[Vertex_1][Slot] = Vertex_0; break; }
      }
    }
  }

  // ── Compute centroid and bounding radius ──────────────────────────────────────────────────────

  Result.Centroid = Make (0, 0, 0);
  for (uint Index = 0; Index < Result.Vertex_Count; Index++)
    Result.Centroid = Add (Result.Centroid, Result.Vertices[Index]);
  if (Result.Vertex_Count)
    Result.Centroid = Scale (Result.Centroid, 1.f / Result.Vertex_Count);

  Result.Bounding_Radius = 0;
  for (uint Index = 0; Index < Result.Vertex_Count; Index++) {
    float Distance_Sq = Dot (Subtract (Result.Vertices[Index], Result.Centroid),
                             Subtract (Result.Vertices[Index], Result.Centroid));
    if (Distance_Sq > Result.Bounding_Radius * Result.Bounding_Radius)
      Result.Bounding_Radius = sqrtf (Distance_Sq);
  }

  printf ("[hull] %u vertices, radius %.1f\n", Result.Vertex_Count, Result.Bounding_Radius);
  return Result;
}

Convex_Hull Hull_From_Vertices (const Vertex *Vertices, uint Count) {

  // Extract vec3 positions from the vertex array and delegate to Quickhull
  vec3 *Positions = malloc (sizeof (vec3) * Count);
  for (uint Index = 0; Index < Count; Index++)
    Positions[Index] = Make (Vertices[Index].Position[0], Vertices[Index].Position[1], Vertices[Index].Position[2]);
  Convex_Hull Hull = Quickhull (Positions, Count);
  free (Positions);
  return Hull;
}

void Hull_Upload (const Convex_Hull *Hull) {

  // Pack the CPU-side hull into the GPU-compatible layout
  Gpu_Hull Packed = {0};
  Packed.Count  = (int)Hull->Vertex_Count;
  Packed.Radius = Hull->Bounding_Radius;
  Packed.Centroid[0] = Hull->Centroid.x;
  Packed.Centroid[1] = Hull->Centroid.y;
  Packed.Centroid[2] = Hull->Centroid.z;

  for (uint Index = 0; Index < Hull->Vertex_Count; Index++) {
    Packed.Vertices[Index][0] = Hull->Vertices[Index].x;
    Packed.Vertices[Index][1] = Hull->Vertices[Index].y;
    Packed.Vertices[Index][2] = Hull->Vertices[Index].z;
    Packed.Vertices[Index][3] = 0;
    memcpy (Packed.Adjacency[Index], Hull->Adjacency[Index], sizeof (int) * HULL_MAX_ADJ);
  }

  // Allocate the hull storage buffer on first use, then upload the packed data
  if (not Hull_Storage_Buffer.Buffer)
    Hull_Storage_Buffer = Buffer_Allocate (sizeof (Gpu_Hull),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  Buffer_Upload (Hull_Storage_Buffer, &Packed, sizeof Packed);
}

// ── §10B. GPU Physics Pipeline ────────────────────────────────────────────────────────────────────

void Physics_Pipeline_Create () {

  // Define the 5 descriptor bindings for the physics compute pipeline
  VkDescriptorSetLayoutBinding Bindings[] = {
    {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // TLAS
    {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // Vertex buffer
    {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // Index buffer
    {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // Player state
    {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // Hull data
  };

  // Create the descriptor set layout with all 5 physics bindings
  VK_CHECK (vkCreateDescriptorSetLayout (Device,
    &(VkDescriptorSetLayoutCreateInfo){
      .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 5,
      .pBindings    = Bindings},
    NULL, &Physics_Descriptor_Layout));

  // Create the pipeline layout with push constants for per-frame Gpu_Input delivery
  VkPushConstantRange Push_Range = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof (Gpu_Input)};
  VK_CHECK (vkCreatePipelineLayout (Device,
    &(VkPipelineLayoutCreateInfo){
      .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount         = 1,
      .pSetLayouts            = &Physics_Descriptor_Layout,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges    = &Push_Range},
    NULL, &Physics_Pipeline_Layout));

  // Load the pre-compiled physics compute shader and create the compute pipeline
  VkShaderModule Physics_Module = Shader_Module_Load (SHADER_PATH_PHYSICS_COMPUTE);
  VK_CHECK (vkCreateComputePipelines (Device, VK_NULL_HANDLE, 1,
    &(VkComputePipelineCreateInfo){
      .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage  = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_COMPUTE_BIT, Physics_Module, "main", NULL},
      .layout = Physics_Pipeline_Layout},
    NULL, &Physics_Pipeline));
  vkDestroyShaderModule (Device, Physics_Module, NULL);
}

void Physics_Resources_Create (const Player *Initial_State) {

  // Allocate the host-visible player state buffer for GPU read-write access each frame
  Player_State_Buffer = Buffer_Allocate (sizeof (Gpu_Player),
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Initialize the GPU player state from the spawn point with a capsule collider
  Gpu_Player Initial_GPU_State = {
    .Position    = {Initial_State->Position.x, Initial_State->Position.y, Initial_State->Position.z},
    .Yaw         = Initial_State->Yaw,
    .Pitch       = Initial_State->Pitch,
    .View_Height = DEFAULT_VIEW_HEIGHT,
    .Shape       = SHAPE_CAPSULE,
    .Extents     = {PLAYER_HALF_EXTENTS[0], PLAYER_HALF_EXTENTS[1], PLAYER_HALF_EXTENTS[2]},
    .Spine       = PLAYER_CAPSULE_SPINE};
  Buffer_Upload (Player_State_Buffer, &Initial_GPU_State, sizeof Initial_GPU_State);

  // Initialize the hull storage buffer with a 1-vertex dummy (replaced when a real hull is loaded)
  if (not Hull_Storage_Buffer.Buffer) {
    Hull_Storage_Buffer = Buffer_Allocate (sizeof (Gpu_Hull),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    Gpu_Hull Empty = {0};
    Empty.Count = 1;
    Buffer_Upload (Hull_Storage_Buffer, &Empty, sizeof Empty);
  }

  // Allocate the physics descriptor pool and set
  VkDescriptorPoolSize Pool_Sizes[] = {
    {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             4}};
  VK_CHECK (vkCreateDescriptorPool (Device,
    &(VkDescriptorPoolCreateInfo){
      .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets       = 1,
      .poolSizeCount = 2,
      .pPoolSizes    = Pool_Sizes},
    NULL, &Physics_Descriptor_Pool));

  VK_CHECK (vkAllocateDescriptorSets (Device,
    &(VkDescriptorSetAllocateInfo){
      .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool     = Physics_Descriptor_Pool,
      .descriptorSetCount = 1,
      .pSetLayouts        = &Physics_Descriptor_Layout},
    &Physics_Descriptor_Set));

  // Write all 5 descriptor bindings: TLAS, vertex buffer, index buffer, player state, hull data
  VkWriteDescriptorSetAccelerationStructureKHR Acceleration_Write = {
    .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
    .accelerationStructureCount = 1,
    .pAccelerationStructures    = &Top_Level.Handle};
  VkDescriptorBufferInfo Vertex_Info  = {Vertex_Buffer.Buffer,       0, Vertex_Buffer.Size};
  VkDescriptorBufferInfo Index_Info   = {Index_Buffer.Buffer,        0, Index_Buffer.Size};
  VkDescriptorBufferInfo Player_Info  = {Player_State_Buffer.Buffer, 0, Player_State_Buffer.Size};
  VkDescriptorBufferInfo Hull_Info    = {Hull_Storage_Buffer.Buffer,  0, Hull_Storage_Buffer.Size};

  VkWriteDescriptorSet Writes[] = {
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, &Acceleration_Write, Physics_Descriptor_Set, 0, 0, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, NULL, NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,                Physics_Descriptor_Set, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL, &Vertex_Info},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,                Physics_Descriptor_Set, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL, &Index_Info},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,                Physics_Descriptor_Set, 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL, &Player_Info},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,                Physics_Descriptor_Set, 4, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL, &Hull_Info},
  };
  vkUpdateDescriptorSets (Device, 5, Writes, 0, NULL);
}

// ── §10C. Physics Dispatch ────────────────────────────────────────────────────────────────────────

Player Physics_Dispatch (Input In, float Dt) {

  // Pack the CPU input into the GPU push constant structure
  Gpu_Input GPU_Input = {
    In.Forward, In.Back, In.Left, In.Right,
    In.Jump, In.Fire, In.Crouch, 0,
    In.Delta_X, In.Delta_Y, Dt, 0};

  // Record a one-shot command buffer for the physics compute dispatch
  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (Command_Buffer,
    &(VkCommandBufferBeginInfo){
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));

  // Bind the physics pipeline and descriptors, push the input, dispatch a single workgroup
  vkCmdBindPipeline       (Command_Buffer, VK_PIPELINE_BIND_POINT_COMPUTE, Physics_Pipeline);
  vkCmdBindDescriptorSets (Command_Buffer, VK_PIPELINE_BIND_POINT_COMPUTE, Physics_Pipeline_Layout,
                           0, 1, &Physics_Descriptor_Set, 0, NULL);
  vkCmdPushConstants      (Command_Buffer, Physics_Pipeline_Layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof GPU_Input, &GPU_Input);
  vkCmdDispatch           (Command_Buffer, 1, 1, 1);

  // Memory barrier: ensure compute shader writes are visible to the host before readback
  vkCmdPipelineBarrier (Command_Buffer,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
    1, &(VkMemoryBarrier){
      .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_HOST_READ_BIT},
    0, NULL, 0, NULL);

  // Submit the command buffer and wait for the GPU to finish
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));
  VK_CHECK (vkQueueSubmit (Queue, 1,
    &(VkSubmitInfo){
      .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers    = &Command_Buffer},
    VK_NULL_HANDLE));
  VK_CHECK (vkQueueWaitIdle (Queue));

  // Read back the updated player state from the GPU buffer
  Gpu_Player *Mapped;
  vkMapMemory (Device, Player_State_Buffer.Memory, 0, sizeof (Gpu_Player), 0, (void **)&Mapped);
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
//   - hull_support_hill():  O(sqrt(n)) amortized hill-climbing with adjacency table
//   - hull_support():       adaptive dispatcher (brute for n<64, hill for n>=64)
//   - shape_offset() case 5: delegates to hull_support() for SHAPE_HULL
//   - hull adds 2 extra sweep rays in the movement and anti-movement directions
//   - recover() uses hull support distance as expected clearance per cardinal direction
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §14. Main
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

int main (int Argc, char **Argv) {

  // Determine which BSP map to load (default or from command-line argument)
  char Map_Path[256];
  snprintf (Map_Path, sizeof Map_Path, "%smaps/%s", ASSET_ROOT, (Argc > 1) ? Argv[1] : DEFAULT_MAP);

  // Initialize SDL2 with video subsystem and create a Vulkan-capable window
  SDL_Init (SDL_INIT_VIDEO);
  Window = SDL_CreateWindow (ENGINE_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             Width, Height, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
  SDL_SetRelativeMouseMode (SDL_TRUE);

  // Create the Vulkan instance, surface, pick a physical device, and create the logical device
  Vulkan_Create_Instance ();
  Vulkan_Pick_Physical_Device ();
  Vulkan_Create_Logical_Device ();

  // Create the swapchain and synchronization primitives
  Vulkan_Create_Swapchain ();
  Vulkan_Create_Synchronization ();

  // Create the ray tracing storage image (render target)
  Raytracing_Storage_Image = Image_Storage_Create (Width, Height);
  Vulkan_Transition_Storage_Image ();

  // Allocate the camera uniform buffer
  Camera_Uniform_Buffer = Buffer_Allocate (sizeof (mat4) * 2 + 16,
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Load the BSP scene and spawn point (no CPU collision map — GPU handles physics via TLAS)
  Spawn Spawn_Point;
  Scene Scene_Data = Scene_Load_From_BSP (Map_Path, &Spawn_Point);

  // Load scene and weapon textures
  Scene_Load_Textures (&Scene_Data);
  Weapon_Instance Weapon = {0};
  Weapon.Model = Weapon_Model_Load ();
  Weapon_Load_Textures (&Weapon);

  // Build acceleration structures (BLAS for world + weapon, then TLAS)
  Acceleration_Structure World_Bottom_Level = Build_World_Bottom_Level (&Scene_Data);
  Weapon_Bottom_Level_Initialize (&Weapon);
  Top_Level_Initialize (2);
  Top_Level_Rebuild (&World_Bottom_Level, &Weapon.Bottom_Level);

  // Create the ray tracing pipeline, shader binding table, and descriptors
  Raytracing_Pipeline_Create ();
  Shader_Binding_Table_Create ();
  Descriptor_Set_Create (&Weapon);

  // Create the GPU physics pipeline and resources (with hull binding)
  Physics_Pipeline_Create ();
  Player Initial_Player = {
    .Position = Spawn_Point.Origin,
    .Yaw      = Spawn_Point.Angle * 3.14159f / 180.f};
  Physics_Resources_Create (&Initial_Player);

  // Optionally build a convex hull from the weapon model for hull-based collision testing
  // Convex_Hull Projectile_Hull = Hull_From_Vertices (Weapon.Model.Vertices, Weapon.Model.Vertex_Count);
  // Hull_Upload (&Projectile_Hull);

  printf ("[init] ready — entering game loop\n");

  // ── Game loop ──────────────────────────────────────────────────────────────────────────────────

  Camera   Cam   = {.Position = Spawn_Point.Origin, .Yaw = Spawn_Point.Angle * 3.14159f / 180.f};
  uint64_t Last  = SDL_GetPerformanceCounter ();
  uint64_t Freq  = SDL_GetPerformanceFrequency ();
  uint     Frame = 0;
  Quit = 0;

  while (not Quit) {

    // Compute delta time from the high-resolution performance counter
    uint64_t Now = SDL_GetPerformanceCounter ();
    float    Dt  = (float)(Now - Last) / (float)Freq;
    if (Dt > MAX_DELTA_TIME) Dt = MAX_DELTA_TIME;
    Last = Now;

    // Poll input and dispatch GPU physics
    Input In         = Poll_Input ();
    Player Physics   = Physics_Dispatch (In, Dt);

    // Update the camera from the physics result
    Cam.Position = Physics.Position;
    Cam.Yaw      = Physics.Yaw;
    Cam.Pitch    = Physics.Pitch;
    Cam.Frame    = Frame;

    // Animate and rebuild the weapon viewmodel
    Weapon_Update (&Weapon, &Cam, Dt, In.Fire);
    Weapon_Bottom_Level_Rebuild (&Weapon);
    Top_Level_Rebuild (&World_Bottom_Level, &Weapon.Bottom_Level);

    // Upload the camera and dispatch ray tracing
    Camera_Upload (&Cam, FIELD_OF_VIEW, Weapon.Texture_Base_Index);
    Raytracing_Frame ();
    Frame++;
  }

  // ── Cleanup ────────────────────────────────────────────────────────────────────────────────────

  vkDeviceWaitIdle (Device);
  printf ("[shutdown] %u frames rendered\n", Frame);

  // Free scene data
  free (Scene_Data.Vertices);
  free (Scene_Data.Indices);
  free (Scene_Data.Materials);
  free (Scene_Data.Texture_Ids);
  free (Scene_Data.Texture_Names);
  if (Scene_Data.Lightmap_Atlas) free (Scene_Data.Lightmap_Atlas);

  // Destroy Vulkan objects
  vkDestroyPipeline            (Device, Pipeline, NULL);
  vkDestroyPipelineLayout      (Device, Pipeline_Layout, NULL);
  vkDestroyPipeline            (Device, Physics_Pipeline, NULL);
  vkDestroyPipelineLayout      (Device, Physics_Pipeline_Layout, NULL);
  vkDestroyDescriptorPool      (Device, Descriptor_Pool, NULL);
  vkDestroyDescriptorPool      (Device, Physics_Descriptor_Pool, NULL);
  vkDestroyDescriptorSetLayout (Device, Descriptor_Set_Layout, NULL);
  vkDestroyDescriptorSetLayout (Device, Physics_Descriptor_Layout, NULL);
  vkDestroySemaphore           (Device, Semaphore_Image_Available, NULL);
  vkDestroySemaphore           (Device, Semaphore_Render_Finished, NULL);
  vkDestroyFence               (Device, Fence, NULL);
  vkDestroyCommandPool         (Device, Command_Pool, NULL);
  vkDestroySwapchainKHR        (Device, Swapchain, NULL);
  vkDestroyDevice              (Device, NULL);
  vkDestroySurfaceKHR          (Instance, Surface, NULL);
  vkDestroyInstance            (Instance, NULL);
  SDL_DestroyWindow (Window);
  SDL_Quit ();
  return 0;
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §12. Shaders — Embedded GLSL Source
//
//   The following glsl shader blocks are extracted by nob.c at build time, compiled to SPIR-V via
//   glslangValidator --target-env vulkan1.3, and loaded at runtime via Shader_Module_Load.
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

glsl shader raygen rgen {
#version 460
#extension GL_EXT_ray_tracing : require

layout(binding = 0) uniform accelerationStructureEXT  Top_Level;
layout(binding = 1, rgba8) uniform image2D             Storage_Image;
layout(binding = 2) uniform Camera_Uniform { mat4 Inverse_View; mat4 Inverse_Projection; uint Frame; uint Weapon_Texture_Base; };

layout(location = 0) rayPayloadEXT vec3 Payload;

void main () {
  vec2 Pixel       = vec2 (gl_LaunchIDEXT.xy) + 0.5;
  vec2 Uv          = Pixel / vec2 (gl_LaunchSizeEXT.xy);
  vec2 Ndc         = Uv * 2.0 - 1.0;

  // Reconstruct the world-space ray from screen coordinates using inverse camera matrices
  vec4 Target      = Inverse_Projection * vec4 (Ndc.x, Ndc.y, 0.0, 1.0);
  vec4 Direction   = Inverse_View * vec4 (normalize (Target.xyz / Target.w), 0.0);

  Payload = vec3 (0.0);
  traceRayEXT (Top_Level,
               gl_RayFlagsOpaqueEXT,        // flags
               0xFF,                          // cull mask
               0,                             // SBT record offset
               0,                             // SBT record stride
               0,                             // miss index
               (Inverse_View * vec4 (0, 0, 0, 1)).xyz,  // origin
               0.001,                         // t_min
               Direction.xyz,                 // direction
               10000.0,                       // t_max
               0);                            // payload location

  imageStore (Storage_Image, ivec2 (gl_LaunchIDEXT.xy), vec4 (Payload, 1.0));
}
}

glsl shader closesthit rchit {
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

layout(binding = 0) uniform accelerationStructureEXT Top_Level;
layout(binding = 2) uniform Camera_Uniform { mat4 Inverse_View; mat4 Inverse_Projection; uint Frame; uint Weapon_Texture_Base; };

// Scene geometry
layout(binding = 3, std430) readonly buffer Vertex_Data   { vec4 Data[]; } Vertices;
layout(binding = 4, std430) readonly buffer Index_Data    { uint Data[]; } Indices;
layout(binding = 5, std430) readonly buffer Material_Data { vec4 Data[]; } Materials;
layout(binding = 6, std430) readonly buffer Tex_Id_Data   { uint Data[]; } Texture_Ids;
layout(binding = 7)         uniform sampler2D              Lightmap;

// Weapon geometry
layout(binding = 8,  std430) readonly buffer Weapon_Vertex_Data { vec4 Data[]; } Weapon_Vertices;
layout(binding = 9,  std430) readonly buffer Weapon_Index_Data  { uint Data[]; } Weapon_Indices;
layout(binding = 10, std430) readonly buffer Weapon_Tex_Id_Data { uint Data[]; } Weapon_Tex_Ids;

// Bindless texture array
layout(binding = 11) uniform sampler2D Textures[];

layout(location = 0) rayPayloadInEXT vec3 Payload;
layout(location = 1) rayPayloadEXT   float Shadow_Factor;

hitAttributeEXT vec2 Barycentrics;

// Read a vertex attribute (48 bytes = 12 floats per vertex) from the appropriate buffer
vec3 Read_Position   (uint I, bool W) { return W ? Weapon_Vertices.Data[I * 3 + 0].xyz : Vertices.Data[I * 3 + 0].xyz; }
vec2 Read_Tex_Uv     (uint I, bool W) { return W ? Weapon_Vertices.Data[I * 3 + 1].xy  : Vertices.Data[I * 3 + 1].xy;  }
vec2 Read_Lightmap_Uv(uint I, bool W) { return W ? Weapon_Vertices.Data[I * 3 + 1].zw  : Vertices.Data[I * 3 + 1].zw;  }
vec3 Read_Normal     (uint I, bool W) { return W ? Weapon_Vertices.Data[I * 3 + 2].xyz : Vertices.Data[I * 3 + 2].xyz; }

void main () {
  // Determine if we hit the weapon (instance 1) or world geometry (instance 0)
  bool  Is_Weapon     = (gl_InstanceCustomIndexEXT == 1);
  uint  Primitive     = gl_PrimitiveID;

  // Fetch triangle vertex indices
  uint I0 = Is_Weapon ? Weapon_Indices.Data[Primitive * 3 + 0] : Indices.Data[Primitive * 3 + 0];
  uint I1 = Is_Weapon ? Weapon_Indices.Data[Primitive * 3 + 1] : Indices.Data[Primitive * 3 + 1];
  uint I2 = Is_Weapon ? Weapon_Indices.Data[Primitive * 3 + 2] : Indices.Data[Primitive * 3 + 2];

  // Interpolate attributes using barycentric coordinates
  vec3 Bary      = vec3 (1.0 - Barycentrics.x - Barycentrics.y, Barycentrics.x, Barycentrics.y);
  vec3 Position  = Read_Position (I0, Is_Weapon) * Bary.x + Read_Position (I1, Is_Weapon) * Bary.y + Read_Position (I2, Is_Weapon) * Bary.z;
  vec2 Tex_Coord = Read_Tex_Uv  (I0, Is_Weapon) * Bary.x + Read_Tex_Uv  (I1, Is_Weapon) * Bary.y + Read_Tex_Uv  (I2, Is_Weapon) * Bary.z;
  vec2 Lm_Coord  = Read_Lightmap_Uv (I0, Is_Weapon) * Bary.x + Read_Lightmap_Uv (I1, Is_Weapon) * Bary.y + Read_Lightmap_Uv (I2, Is_Weapon) * Bary.z;
  vec3 Normal    = normalize (Read_Normal (I0, Is_Weapon) * Bary.x + Read_Normal (I1, Is_Weapon) * Bary.y + Read_Normal (I2, Is_Weapon) * Bary.z);

  // Fetch the texture ID for this triangle and sample the albedo
  uint Tex_Id = Is_Weapon
    ? Weapon_Tex_Ids.Data[Primitive] + Weapon_Texture_Base
    : Texture_Ids.Data[Primitive];
  vec3 Albedo  = texture (Textures[nonuniformEXT(Tex_Id)], Tex_Coord).rgb;

  // Lighting: lightmap for world geometry, simple directional for weapon
  vec3 Sun_Direction = normalize (vec3 (0.5, 0.8, 0.3));
  vec3 Lighting;
  if (Is_Weapon) {
    float Diffuse = max (dot (Normal, Sun_Direction), 0.0) * 0.6 + 0.4;
    Lighting = vec3 (Diffuse);
  } else {
    Lighting = texture (Lightmap, Lm_Coord).rgb;
  }

  // Trace a shadow ray toward the sun (skip weapon geometry via cull mask 0xFE)
  Shadow_Factor = 0.0;
  traceRayEXT (Top_Level,
               gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,
               0xFE,                         // cull mask (skip weapon instance)
               0,                             // SBT record offset
               0,                             // SBT record stride
               1,                             // miss index (shadow miss)
               Position + Normal * 0.01,      // origin (biased along normal)
               0.001,                         // t_min
               Sun_Direction,                 // direction
               10000.0,                       // t_max
               1);                            // payload location

  // Combine albedo, lighting, and shadow
  float Shadow = mix (0.3, 1.0, Shadow_Factor);
  Payload = Albedo * Lighting * Shadow;

  // Apply simple tone mapping (Reinhard)
  Payload = Payload / (Payload + 1.0);
}
}

glsl shader miss rmiss {
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 Payload;

void main () {
  // Procedural sky: gradient from pale horizon to deeper blue at zenith
  float Vertical  = max (gl_WorldRayDirectionEXT.y, 0.0);
  vec3  Horizon   = vec3 (0.7, 0.8, 0.95);
  vec3  Zenith    = vec3 (0.2, 0.4, 0.8);
  Payload = mix (Horizon, Zenith, pow (Vertical, 0.5));
}
}

glsl shader shadow_miss rmiss {
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 1) rayPayloadInEXT float Shadow_Factor;

void main () {
  // Shadow ray reached the light source without hitting anything — fully lit
  Shadow_Factor = 1.0;
}
}

glsl shader physics comp {
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_ray_query : require

// ── Descriptor bindings ────────────────────────────────────────────────────────────────────────

layout(binding = 0) uniform accelerationStructureEXT Top_Level;
layout(binding = 1, std430) readonly buffer Vertex_Data { vec4 Data[]; } Vertices;
layout(binding = 2, std430) readonly buffer Index_Data  { uint Data[]; } Indices;

layout(binding = 3, std430) buffer Player_Buffer {
  vec3  Position;     float Pad_A;
  vec3  Velocity;     float Pad_B;
  float Yaw, Pitch;
  int   On_Ground, Jump_Held;
  vec3  Ground_Normal; float Pad_C;
  int   Ground_Plane, Ducked;
  float View_Height, Stuck_Time;
  float Speed_Last;  int Shape;
  vec3  Extents;     float Pad_D;
  float Spine;       float Pad_E1, Pad_E2, Pad_E3;
} Player;

layout(binding = 4, std430) readonly buffer Hull_Buffer {
  vec4  Hull_Vertices [256];
  int   Hull_Adjacency[256][16];
  int   Hull_Count;
  float Hull_Radius;
  vec3  Hull_Centroid;
  int   Hull_Pad;
};

layout(push_constant) uniform Push {
  int   Forward, Back, Left, Right;
  int   Jump, Fire, Crouch, Pad;
  float Delta_X, Delta_Y, Dt, Pad2;
} Input;

layout(local_size_x = 1) in;

// ── Physics constants ──────────────────────────────────────────────────────────────────────────

const float GRAVITY              = 800.0;
const float GROUND_FRICTION      = 6.0;
const float STOP_SPEED           = 100.0;
const float GROUND_ACCELERATE    = 10.0;
const float AIR_ACCELERATE       = 1.0;
const float MAXIMUM_SPEED        = 320.0;
const float JUMP_VELOCITY        = 270.0;
const float STEP_SIZE            = 18.0;
const float MINIMUM_WALK_NORMAL  = 0.7;
const float OVERBOUNCE           = 1.001;
const int   MAXIMUM_CLIP_PLANES  = 5;
const float DEFAULT_VIEW_HEIGHT  = 26.0;
const float CROUCH_VIEW_HEIGHT   = 12.0;
const float MOUSE_SENSITIVITY    = 0.003;

// ── Collider shape constants ───────────────────────────────────────────────────────────────────

const int SHAPE_SPHERE    = 0;
const int SHAPE_CAPSULE   = 1;
const int SHAPE_AABB      = 2;
const int SHAPE_CYLINDER  = 3;
const int SHAPE_ELLIPSOID = 4;
const int SHAPE_HULL      = 5;

// ── Convex hull support functions ──────────────────────────────────────────────────────────────

// Brute-force support: O(n) linear scan over all hull vertices.  Best for small hulls (< 64 verts).
vec3 hull_support_brute (vec3 direction) {
  float best_dot = -1e30;
  int   best_idx = 0;
  for (int i = 0; i < Hull_Count; i++) {
    float d = dot (Hull_Vertices[i].xyz, direction);
    if (d > best_dot) { best_dot = d; best_idx = i; }
  }
  return Hull_Vertices[best_idx].xyz;
}

// Hill-climbing support: O(sqrt(n)) amortized using per-vertex adjacency table.
// Starts from vertex 0 and follows neighbors that increase dot(vertex, direction) until a
// local maximum is reached.  On a convex hull, the local maximum IS the global maximum.
vec3 hull_support_hill (vec3 direction) {
  int current = 0;
  float current_dot = dot (Hull_Vertices[0].xyz, direction);

  for (int iteration = 0; iteration < 256; iteration++) {
    int best_neighbor = -1;
    float best_dot    = current_dot;

    // Check all neighbors of the current vertex
    for (int slot = 0; slot < 16; slot++) {
      int neighbor = Hull_Adjacency[current][slot];
      if (neighbor < 0) break;
      float d = dot (Hull_Vertices[neighbor].xyz, direction);
      if (d > best_dot) { best_dot = d; best_neighbor = neighbor; }
    }

    // If no neighbor improves the dot product, we've found the support point
    if (best_neighbor < 0) break;
    current     = best_neighbor;
    current_dot = best_dot;
  }
  return Hull_Vertices[current].xyz;
}

// Adaptive dispatcher: brute-force for small hulls, hill-climbing for large ones
vec3 hull_support (vec3 direction) {
  if (Hull_Count < 64) return hull_support_brute (direction);
  return hull_support_hill (direction);
}

// ── Shape support function ─────────────────────────────────────────────────────────────────────

// Map a unit direction to the shape's surface offset (Minkowski support mapping)
vec3 shape_offset (vec3 d) {
  switch (Player.Shape) {
    case SHAPE_SPHERE:
      return d * Player.Extents.x;

    case SHAPE_CAPSULE:
      return d * Player.Extents.x + vec3 (0.0, sign(d.y) * Player.Spine, 0.0);

    case SHAPE_AABB:
      return sign(d) * Player.Extents;

    case SHAPE_CYLINDER: {
      vec2  xz    = d.xz;
      float len   = length (xz);
      vec2  disc  = (len > 1e-6) ? xz / len * Player.Extents.x : vec2(0.0);
      return vec3 (disc.x, sign(d.y) * Player.Extents.y, disc.y);
    }

    case SHAPE_ELLIPSOID: {
      vec3 scaled = d / Player.Extents;
      float len   = length (scaled);
      return (len > 1e-6) ? normalize(scaled) * Player.Extents : vec3(0.0);
    }

    case SHAPE_HULL:
      return hull_support (d);

    default:
      return d * Player.Extents.x;
  }
}

// ── Ray trace helper ───────────────────────────────────────────────────────────────────────────

struct Trace_Result {
  float Fraction;
  vec3  Normal;
  bool  Hit;
};

// Cast a swept shape from Origin along Direction for up to Distance units.
// We approximate the expanded shape by casting multiple rays offset by the support function
// in cardinal + diagonal directions, taking the nearest hit.
Trace_Result trace_shape (vec3 Origin, vec3 Direction, float Distance) {
  Trace_Result result;
  result.Fraction = 1.0;
  result.Normal   = vec3 (0.0, 1.0, 0.0);
  result.Hit      = false;

  if (Distance < 1e-6) return result;
  vec3 dir_norm = normalize (Direction);

  // 26 probe directions: 6 axes + 12 edge diagonals + 8 corner diagonals
  const vec3 probes[26] = vec3[26](
    vec3( 1, 0, 0), vec3(-1, 0, 0), vec3(0, 1, 0), vec3(0,-1, 0), vec3(0, 0, 1), vec3(0, 0,-1),
    vec3( 1, 1, 0), vec3( 1,-1, 0), vec3(-1, 1, 0), vec3(-1,-1, 0),
    vec3( 1, 0, 1), vec3( 1, 0,-1), vec3(-1, 0, 1), vec3(-1, 0,-1),
    vec3( 0, 1, 1), vec3( 0, 1,-1), vec3( 0,-1, 1), vec3( 0,-1,-1),
    vec3( 1, 1, 1), vec3( 1, 1,-1), vec3( 1,-1, 1), vec3( 1,-1,-1),
    vec3(-1, 1, 1), vec3(-1, 1,-1), vec3(-1,-1, 1), vec3(-1,-1,-1)
  );

  for (int i = 0; i < 26; i++) {
    vec3 offset = shape_offset (normalize(probes[i]));
    vec3 ray_origin = Origin + offset;

    rayQueryEXT rq;
    rayQueryInitializeEXT (rq, Top_Level, gl_RayFlagsOpaqueEXT, 0xFF,
                           ray_origin, 0.0, dir_norm, Distance);

    while (rayQueryProceedEXT (rq)) {}

    if (rayQueryGetIntersectionTypeEXT (rq, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
      float t = rayQueryGetIntersectionTEXT (rq, true);
      if (t < result.Fraction * Distance) {
        // Reconstruct the triangle normal from vertices
        uint prim = rayQueryGetIntersectionPrimitiveIndexEXT (rq, true);
        uint i0 = Indices.Data[prim * 3 + 0];
        uint i1 = Indices.Data[prim * 3 + 1];
        uint i2 = Indices.Data[prim * 3 + 2];
        vec3 v0 = Vertices.Data[i0 * 3].xyz;
        vec3 v1 = Vertices.Data[i1 * 3].xyz;
        vec3 v2 = Vertices.Data[i2 * 3].xyz;
        vec3 n  = normalize (cross (v1 - v0, v2 - v0));

        // Ensure the normal faces toward the ray origin
        if (dot (n, dir_norm) > 0.0) n = -n;

        result.Fraction = t / Distance;
        result.Normal   = n;
        result.Hit      = true;
      }
    }
  }

  // Additionally cast a ray offset by the movement direction's support (catches head-on impacts)
  vec3 move_offset = shape_offset (dir_norm);
  {
    rayQueryEXT rq;
    rayQueryInitializeEXT (rq, Top_Level, gl_RayFlagsOpaqueEXT, 0xFF,
                           Origin + move_offset, 0.0, dir_norm, Distance);
    while (rayQueryProceedEXT (rq)) {}
    if (rayQueryGetIntersectionTypeEXT (rq, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
      float t = rayQueryGetIntersectionTEXT (rq, true);
      if (t < result.Fraction * Distance) {
        uint prim = rayQueryGetIntersectionPrimitiveIndexEXT (rq, true);
        uint i0 = Indices.Data[prim * 3 + 0];
        uint i1 = Indices.Data[prim * 3 + 1];
        uint i2 = Indices.Data[prim * 3 + 2];
        vec3 v0 = Vertices.Data[i0 * 3].xyz;
        vec3 v1 = Vertices.Data[i1 * 3].xyz;
        vec3 v2 = Vertices.Data[i2 * 3].xyz;
        vec3 n  = normalize (cross (v1 - v0, v2 - v0));
        if (dot (n, dir_norm) > 0.0) n = -n;
        result.Fraction = t / Distance;
        result.Normal   = n;
        result.Hit      = true;
      }
    }
  }

  // And the anti-movement direction's support (catches backward collisions from broad shapes)
  vec3 anti_offset = shape_offset (-dir_norm);
  {
    rayQueryEXT rq;
    rayQueryInitializeEXT (rq, Top_Level, gl_RayFlagsOpaqueEXT, 0xFF,
                           Origin + anti_offset, 0.0, dir_norm, Distance);
    while (rayQueryProceedEXT (rq)) {}
    if (rayQueryGetIntersectionTypeEXT (rq, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
      float t = rayQueryGetIntersectionTEXT (rq, true);
      if (t < result.Fraction * Distance) {
        uint prim = rayQueryGetIntersectionPrimitiveIndexEXT (rq, true);
        uint i0 = Indices.Data[prim * 3 + 0];
        uint i1 = Indices.Data[prim * 3 + 1];
        uint i2 = Indices.Data[prim * 3 + 2];
        vec3 v0 = Vertices.Data[i0 * 3].xyz;
        vec3 v1 = Vertices.Data[i1 * 3].xyz;
        vec3 v2 = Vertices.Data[i2 * 3].xyz;
        vec3 n  = normalize (cross (v1 - v0, v2 - v0));
        if (dot (n, dir_norm) > 0.0) n = -n;
        result.Fraction = t / Distance;
        result.Normal   = n;
        result.Hit      = true;
      }
    }
  }

  return result;
}

// ── Ground trace ───────────────────────────────────────────────────────────────────────────────

// Cast a short ray downward to detect ground contact
void ground_trace () {
  vec3 down_offset = shape_offset (vec3 (0, -1, 0));
  vec3 origin      = Player.Position + down_offset;
  float dist       = 0.5;

  rayQueryEXT rq;
  rayQueryInitializeEXT (rq, Top_Level, gl_RayFlagsOpaqueEXT, 0xFF,
                         origin, 0.0, vec3 (0, -1, 0), dist);
  while (rayQueryProceedEXT (rq)) {}

  if (rayQueryGetIntersectionTypeEXT (rq, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
    uint prim = rayQueryGetIntersectionPrimitiveIndexEXT (rq, true);
    uint i0 = Indices.Data[prim * 3 + 0];
    uint i1 = Indices.Data[prim * 3 + 1];
    uint i2 = Indices.Data[prim * 3 + 2];
    vec3 v0 = Vertices.Data[i0 * 3].xyz;
    vec3 v1 = Vertices.Data[i1 * 3].xyz;
    vec3 v2 = Vertices.Data[i2 * 3].xyz;
    vec3 n  = normalize (cross (v1 - v0, v2 - v0));
    if (n.y < 0.0) n = -n;

    if (n.y >= MINIMUM_WALK_NORMAL) {
      Player.On_Ground    = 1;
      Player.Ground_Normal = n;
      Player.Ground_Plane  = 1;
    } else {
      Player.On_Ground    = 0;
      Player.Ground_Plane  = 0;
    }
  } else {
    Player.On_Ground    = 0;
    Player.Ground_Plane  = 0;
  }
}

// ── Clip velocity ──────────────────────────────────────────────────────────────────────────────

vec3 clip_velocity (vec3 vel, vec3 normal) {
  float backoff = dot (vel, normal) * OVERBOUNCE;
  return vel - normal * backoff;
}

// ── Slide move ─────────────────────────────────────────────────────────────────────────────────

void slide_move () {
  vec3  planes[5];
  int   plane_count = 0;
  vec3  vel         = Player.Velocity;
  float time_left   = Input.Dt;

  // If on ground, add the ground plane as the first clip plane
  if (Player.On_Ground == 1) {
    planes[plane_count++] = Player.Ground_Normal;
    vel = clip_velocity (vel, Player.Ground_Normal);
  }

  // Iteratively trace and clip against contact planes
  for (int bump = 0; bump < 4 && time_left > 0.001; bump++) {
    vec3  move_dir  = vel * time_left;
    float move_dist = length (move_dir);
    if (move_dist < 0.001) break;

    Trace_Result trace = trace_shape (Player.Position, move_dir, move_dist);

    if (trace.Fraction > 0.0)
      Player.Position += normalize(move_dir) * move_dist * trace.Fraction;

    if (!trace.Hit) break;

    time_left *= (1.0 - trace.Fraction);

    // Avoid duplicating a plane we've already clipped against
    bool duplicate = false;
    for (int p = 0; p < plane_count; p++)
      if (dot (trace.Normal, planes[p]) > 0.99) { duplicate = true; break; }
    if (duplicate) continue;

    if (plane_count < MAXIMUM_CLIP_PLANES)
      planes[plane_count++] = trace.Normal;

    // Clip velocity against all accumulated planes
    vel = clip_velocity (vel, trace.Normal);

    // If velocity points into a previously established plane, clip against both
    for (int p = 0; p < plane_count; p++) {
      if (dot (vel, planes[p]) >= 0.0) continue;
      vel = clip_velocity (vel, planes[p]);

      // If still heading into another plane, slide along the crease
      for (int q = 0; q < plane_count; q++) {
        if (q == p || dot (vel, planes[q]) >= 0.0) continue;
        vec3 crease = cross (planes[p], planes[q]);
        float len   = length (crease);
        if (len > 1e-6) {
          crease /= len;
          vel = crease * dot (vel, crease);
        }
      }
      break;
    }
  }

  Player.Velocity = vel;
}

// ── Step move ──────────────────────────────────────────────────────────────────────────────────

void step_move () {
  // Save state before the step attempt
  vec3 start_pos = Player.Position;
  vec3 start_vel = Player.Velocity;

  // Try a normal slide move first
  slide_move ();
  vec3 flat_pos = Player.Position;

  // Reset and try stepping up
  Player.Position = start_pos;
  Player.Velocity = start_vel;

  // Step up
  Trace_Result up = trace_shape (Player.Position, vec3(0, STEP_SIZE, 0), STEP_SIZE);
  if (up.Fraction > 0.0)
    Player.Position.y += STEP_SIZE * up.Fraction;

  // Slide forward from the raised position
  slide_move ();

  // Step down to find the ground
  Trace_Result down = trace_shape (Player.Position, vec3(0, -STEP_SIZE, 0), STEP_SIZE);
  if (down.Hit && down.Normal.y >= MINIMUM_WALK_NORMAL) {
    Player.Position.y -= STEP_SIZE * down.Fraction;

    // Keep the stepped result only if it moved us farther horizontally
    vec2 step_delta = Player.Position.xz - start_pos.xz;
    vec2 flat_delta = flat_pos.xz - start_pos.xz;
    if (dot (step_delta, step_delta) <= dot (flat_delta, flat_delta)) {
      Player.Position = flat_pos;
      Player.Velocity = start_vel;
      slide_move ();
    }
  } else {
    Player.Position = flat_pos;
  }
}

// ── Stuck recovery ─────────────────────────────────────────────────────────────────────────────

void recover () {
  // Cast rays in 6 cardinal directions and nudge the player away from walls
  vec3 dirs[6] = vec3[6](
    vec3(1,0,0), vec3(-1,0,0), vec3(0,1,0), vec3(0,-1,0), vec3(0,0,1), vec3(0,0,-1));

  for (int i = 0; i < 6; i++) {
    vec3 offset  = shape_offset (dirs[i]);
    float expect = length (offset);

    rayQueryEXT rq;
    rayQueryInitializeEXT (rq, Top_Level, gl_RayFlagsOpaqueEXT, 0xFF,
                           Player.Position, 0.0, dirs[i], expect);
    while (rayQueryProceedEXT (rq)) {}

    if (rayQueryGetIntersectionTypeEXT (rq, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
      float t = rayQueryGetIntersectionTEXT (rq, true);
      if (t < expect) {
        float penetration = expect - t;
        Player.Position -= dirs[i] * (penetration + 0.125);
      }
    }
  }
}

// ── Main physics entry point ───────────────────────────────────────────────────────────────────

void main () {

  // ── Mouse look ────────────────────────────────────────────────────────────────────────────────
  Player.Yaw   -= Input.Delta_X * MOUSE_SENSITIVITY;
  Player.Pitch -= Input.Delta_Y * MOUSE_SENSITIVITY;
  Player.Pitch  = clamp (Player.Pitch, -1.5, 1.5);

  // ── Build a movement basis from yaw ───────────────────────────────────────────────────────────
  float cy = cos (Player.Yaw), sy = sin (Player.Yaw);
  vec3 forward = vec3 (-sy, 0, -cy);
  vec3 right   = vec3 ( cy, 0, -sy);

  // ── Compute the wish direction and speed from keyboard input ──────────────────────────────────
  vec3 wish = vec3 (0.0);
  if (Input.Forward == 1) wish += forward;
  if (Input.Back    == 1) wish -= forward;
  if (Input.Right   == 1) wish += right;
  if (Input.Left    == 1) wish -= right;
  float wish_speed = MAXIMUM_SPEED;
  if (length (wish) > 0.001) wish = normalize (wish); else wish_speed = 0.0;

  // ── Crouch handling ───────────────────────────────────────────────────────────────────────────
  float target_view = DEFAULT_VIEW_HEIGHT;
  if (Input.Crouch == 1) {
    Player.Ducked = 1;
    target_view = CROUCH_VIEW_HEIGHT;
  } else {
    Player.Ducked = 0;
  }

  // ── Ground trace ──────────────────────────────────────────────────────────────────────────────
  ground_trace ();

  // ── Apply ground or air movement ──────────────────────────────────────────────────────────────
  if (Player.On_Ground == 1) {

    // Friction
    float speed = length (Player.Velocity);
    if (speed > 0.1) {
      float control = max (speed, STOP_SPEED);
      float drop    = control * GROUND_FRICTION * Input.Dt;
      float scale   = max (speed - drop, 0.0) / speed;
      Player.Velocity *= scale;
    }

    // Ground acceleration (Quake 3 style)
    float current_speed = dot (Player.Velocity, wish);
    float add_speed     = wish_speed - current_speed;
    if (add_speed > 0.0) {
      float accel_speed = GROUND_ACCELERATE * wish_speed * Input.Dt;
      if (accel_speed > add_speed) accel_speed = add_speed;
      Player.Velocity += wish * accel_speed;
    }

    // Jump
    if (Input.Jump == 1 && Player.Jump_Held == 0) {
      Player.Velocity.y = JUMP_VELOCITY;
      Player.On_Ground  = 0;
    }
  } else {
    // Air acceleration (enables strafe-jumping)
    float current_speed = dot (Player.Velocity, wish);
    float add_speed     = wish_speed - current_speed;
    if (add_speed > 0.0) {
      float accel_speed = AIR_ACCELERATE * wish_speed * Input.Dt;
      if (accel_speed > add_speed) accel_speed = add_speed;
      Player.Velocity += wish * accel_speed;
    }

    // Gravity
    Player.Velocity.y -= GRAVITY * Input.Dt;
  }

  // Track jump key state (prevent auto-bunny-hopping)
  Player.Jump_Held = Input.Jump;

  // ── Move and collide ──────────────────────────────────────────────────────────────────────────
  if (Player.On_Ground == 1)
    step_move ();
  else
    slide_move ();

  // ── Stuck recovery ────────────────────────────────────────────────────────────────────────────
  recover ();

  // ── Re-check ground after movement ────────────────────────────────────────────────────────────
  ground_trace ();

  // ── Smoothly interpolate the view height toward the target ────────────────────────────────────
  float delta = target_view - Player.View_Height;
  if (abs(delta) < 0.1) Player.View_Height = target_view;
  else Player.View_Height += delta * min (Input.Dt * 10.0, 1.0);

  // ── Track speed for debugging ─────────────────────────────────────────────────────────────────
  Player.Speed_Last = length (Player.Velocity.xz);
}
}
