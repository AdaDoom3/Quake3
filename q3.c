// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
//                                                                 Q 3 . C
//          
//                                        Vulkan-based Id Tech inspired hybrid raytracing game engine     
//
// §1. Settings              
// §2. Types                 
// §3. Context               
// §4. Math                  
// §5. Memory                
// §6. Textures              
// §7. Models                
// §8. Scene                 
// §9. Acceleration Structures
// §10. Physics               
// §11. Pipeline              
// §12. Shaders               
// §13. Render                
// §14. Main                  
// §15. Assets                 
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
//                                                       S P E C I F I C A T I O N
//                 
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Standard Libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

// Media Layer
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

// Graphics
#include <vulkan/vulkan.h>

// Audio
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/efx.h>
  
// Language Extensions
#include <iso646.h>

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §1. Settings
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Engine id and info strings
const char *ENGINE_NAME    = "q3";
const char *ENGINE_VERSION = "0.1.0";              

// Aspect ratio constraints
#define ASPECT_NARROW_X     21
#define ASPECT_NARROW_Y     9
#define ASPECT_WIDE_X       4
#define ASPECT_WIDE_Y       3
#define MINIMUM_WINDOW_SIZE 256

// Windowing and viewport settings
#define FIELD_OF_VIEW  90.f    // Vertical field-of-view in degrees
#define NEAR_CLIP      0.1f    // Near clip plane distance
#define FAR_CLIP       10000.f // Far clip plane distance
#define MAX_DELTA_TIME 0.05f   // Clamp to 20 fps minimum (prevents physics tunneling)

// Player physics constants designed to mirror the Quake 3 movement parameters. The GPU physics compute shader (§10) references these same
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

// Capsule spine half-length: half height minus radius
#define PLAYER_CAPSULE_SPINE 17.f // For a 32-unit tall, 15-unit radius capsule: 32 - 15 = 17 units.

// Projectile constants
#define MAX_PROJECTILES 64    // Maximum simultaneous projectiles in flight
#define ROCKET_SPEED    900.f // Rocket projectile speed (units/second)
#define ROCKET_DAMAGE   100   // Direct hit damage
#define ROCKET_SPLASH   120.f // Splash damage radius
#define ROCKET_LIFETIME 10.f  // Seconds before projectile expires
#define FIRE_COOLDOWN   0.8f  // Minimum seconds between shots

// Vulkan limits and versioning
#define VULKAN_API_VERSION       VK_API_VERSION_1_3
#define SWAPCHAIN_MAX_IMAGES     8                  
#define DESCRIPTOR_TEXTURE_SLOTS 1536               
#define RAY_RECURSION_DEPTH      2    

// Enable the Khronos validation layer for debug builds
typedef unsigned int uint; // For sanity...
const uint  VALIDATION_LAYER_COUNT = 1;
const char *VALIDATION_LAYERS[]    = {"VK_LAYER_KHRONOS_validation"};

// Required device extensions
const uint DEVICE_EXTENSION_COUNT = 6;
const char *DEVICE_EXTENSIONS[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                   VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                                   VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                                   VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                                   VK_KHR_RAY_QUERY_EXTENSION_NAME,
                                   VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME};

// Visual style knobs define the artistic look of the game and are independent of quality/performance tier
typedef struct {
  float Vignette;       // Vignette darkening intensity
  float Bloom_Strength; // Bloom glow intensity
  float Exposure;       // Tonemapping exposure multiplier

  // Should the following be added ???
  // Fog_Density      
  // Fog_Color     
  // Ambient_Sky    
  // Ambient_Ground 
  // Sun_Radiance    
  // Shadow_Floor    
  // Lightmap_Mult  
  // Contrast_Power   
  // Saturation_Boost
  // Color_Grade      
} Visual_Style;
const Visual_Style STYLE = {
  .Vignette       = 0.35f,  // Moderate-strong vignette
  .Bloom_Strength = 0.08f,  // Subtle bloom
  .Exposure       = 1.75f}; // Slightly lower exposure - just enough to deepen without losing detail

// Quality knobs
typedef enum {
  QUALITY_ULTRA, 
  QUALITY_HIGH,  
  QUALITY_MEDIUM,   
  QUALITY_LOW,     
  QUALITY_POTATO, 

  QUALITY_COUNT
} Quality_Level;
typedef struct {
  const char *Name;
  int         Width, Height;  // Window resolution
  float       Render_Scale;   // Internal RT render resolution multiplier
  int         SPP;            // Ray count samples per pixel
  int         Parallax;       // Enable parallax occlusion mapping
  bool        Denoise_Passes; // A-trous wavelet denoise iterations
  bool        Checkerboard;   // Temporal checkerboard optimization for ray reduction
} Quality_Preset;
const Quality_Preset QUALITY_PRESETS [QUALITY_COUNT] = {
  //                            Res        Scale  SPP  POM  DN   CB
  [QUALITY_ULTRA]  = {"Crysis", 3840,2160, 1.00f,  4,  1,   2,   1}, 
  [QUALITY_HIGH]   = {"High",   2560,1440, 1.00f,  2,  1,   2,   1}, 
  [QUALITY_MEDIUM] = {"Medium", 1920,1080, 1.00f,  1,  1,   2,   1},
  [QUALITY_LOW]    = {"Low",    1600, 900, 1.00f,  1,  1,   1,   1},
  [QUALITY_POTATO] = {"Potato",  854, 480, 0.35f,  1,  0,   0,   1},
};

// Convex Hull Limits
#define HULL_MAX_VERTS    256 // Per-hull vertex cap (matches GPU array size in Gpu_Hull)
#define HULL_MAX_ADJ      16  // Maximum adjacency entries per vertex (for hill-climb support)
#define HULL_MAX_FACES    512 // Quickhull internal face cap during construction
#define HULL_MAX_ENTITIES 32  // Maximum simultaneous hull collider instances

// Player bounding box half-extents (x, y, z) used by the capsule collider
const float PLAYER_HALF_EXTENTS[3] = {15.f, 32.f, 15.f};

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §15. Assets
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Body-part damage multiplier maps: grayscale TGA textures UV-mapped to player models
#define DAMAGE_CACHE_MAX 64
typedef struct {
  const char *Model_Name;       // Player model directory name
  const char *Damage_Maps[6];   // Up to 6 damage map TGA paths per model (NULL-terminated)
  int         Damage_Map_Count; // Number of damage maps for this model
} Model_Damage_Entry;

// Asset paths
#define ASSET_ROOT "assets/" // Root directory for all game assets

// Default BSP map to load when no command-line argument is given
const char *DEFAULT_MAP = "oa_dm1.bsp";

// Paths to the weapon model's diffuse textures (body and sight)
#define WEAPON_TEXTURE_COUNT 2
const char *WEAPON_TEXTURE_PATHS[] = {ASSET_ROOT "models/weapons2/machinegun/mgun.tga",
                                      ASSET_ROOT "models/weapons2/machinegun/sight.tga"};

// Paths to the three-part machinegun weapon model (body, barrel, hand)
#define WEAPON_BODY_PATH   ASSET_ROOT "models/weapons2/machinegun/machinegun.md3"
#define WEAPON_BARREL_PATH ASSET_ROOT "models/weapons2/machinegun/machinegun_barrel.md3"
#define WEAPON_HAND_PATH   ASSET_ROOT "models/weapons2/machinegun/machinegun_hand.md3"

// Damage or "hit" texture manifest
#define DAMAGE_MODEL_COUNT 15
const Model_Damage_Entry DAMAGE_MAP_REGISTRY[DAMAGE_MODEL_COUNT] = {
  {"grism",    {ASSET_ROOT "models/players/grism/enkiskin_dmg.tga"}, 1},
  {"sarge",    {ASSET_ROOT "models/players/grism/enkiskin_dmg.tga"}, 1},
  {"liz",      {ASSET_ROOT "models/players/liz/h_head_dmg.tga",
                ASSET_ROOT "models/players/liz/u_torso_dmg.tga",
                ASSET_ROOT "models/players/liz/l_legs_dmg.tga"}, 3},
  {"major",    {ASSET_ROOT "models/players/major/head_dmg.tga",
                ASSET_ROOT "models/players/major/torso_dmg.tga",
                ASSET_ROOT "models/players/major/lower_dmg.tga"}, 3},
  {"tony",     {ASSET_ROOT "models/players/tony/head_dmg.tga",
                ASSET_ROOT "models/players/tony/suit_dmg.tga"}, 2},
  {"assassin", {ASSET_ROOT "models/players/assassin/upper_dmg.tga",
                ASSET_ROOT "models/players/assassin/lower_dmg.tga"}, 2},
  {"smarine",  {ASSET_ROOT "models/players/smarine/2h_head_dmg.tga",
                ASSET_ROOT "models/players/smarine/2u_torso_dmg.tga",
                ASSET_ROOT "models/players/smarine/2l_legs_dmg.tga"}, 3},
  {"beret",    {ASSET_ROOT "models/players/beret/skin1_dmg.tga",
                ASSET_ROOT "models/players/beret/skin2_dmg.tga"}, 2},
  {"gargoyle", {ASSET_ROOT "models/players/gargoyle/bared_dmg.tga"}, 1},
  {"penguin",  {ASSET_ROOT "models/players/penguin/skin_dmg.tga"}, 1},
  {"sergei",   {ASSET_ROOT "models/players/sergei/face_dmg.tga",
                ASSET_ROOT "models/players/sergei/hairs_dmg.tga",
                ASSET_ROOT "models/players/sergei/skin_dmg.tga"}, 3},
  {"skelebot", {ASSET_ROOT "models/players/skelebot/skin1_dmg.tga",
                ASSET_ROOT "models/players/skelebot/skin2_dmg.tga"}, 2},
  {"merman",   {ASSET_ROOT "models/players/merman/skin_dmg.tga",
                ASSET_ROOT "models/players/merman/fins_dmg.tga",
                ASSET_ROOT "models/players/merman/brac_dmg.tga"}, 3},
  {"sorceress", {ASSET_ROOT "models/players/sorceress/drowhead_dmg.tga",
                 ASSET_ROOT "models/players/sorceress/drowbody_dmg.tga",
                 ASSET_ROOT "models/players/sorceress/rings_dmg.tga"}, 3},
  {"kyonshi",  {ASSET_ROOT "models/players/kyonshi/skin_dmg.tga",
                ASSET_ROOT "models/players/kyonshi/torso_dmg.tga",
                ASSET_ROOT "models/players/kyonshi/hair_dmg.tga",
                ASSET_ROOT "models/players/kyonshi/eyes_dmg.tga",
                ASSET_ROOT "models/players/kyonshi/lower_dmg.tga"}, 5}};

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

// Windowing and Cursor
typedef enum {GAME_PLAYING,    GAME_MENU}                      Game_Mode_Kind;
typedef enum {FULLSCREEN_MODE, WINDOWED_MODE}                  Window_Mode_Kind;
typedef enum {CURSOR_SYSTEM,   CURSOR_ACTIVE, CURSOR_INACTIVE} Cursor_Kind;
typedef enum {
  OTHER_ACTIVATED,     // Alt-tab or focus gained normally
  CLICK_ACTIVATED,     // Window activated by mouse click
  OTHER_DEACTIVATED,   // Focus lost to another window
  MINIMIZE_DEACTIVATED // Window was minimized
} Activated_Kind;

// Projectile
typedef struct {
  vec3  Position;     // World position
  float Pad_A;
  vec3  Velocity;     // Units per second
  float Lifetime;     // Seconds remaining before expiry
  int   Active;       // 1 = live, 0 = dead
  int   Material_Hit; // Surface material on impact (for sound selection)
  float Radius;       // Collision radius
  float Damage;       // Base damage on impact (before body-part multiplier)
  float Hit_U, Hit_V; // UV coordinates at impact point (for damage map sampling)
  int   Instance_Hit; // TLAS instance index of hit object (-1 = none, 0 = world, 1 = weapon, >=2 = player)
  int   Pad_B;        // Alignment padding
} Projectile;

typedef struct {
  Projectile Slots[MAX_PROJECTILES]; // Fixed-size projectile array
  int        Count;                  // Active projectile count
  float      Fire_Cooldown;          // Time until next shot allowed
  float      Pad[2];
} Projectile_Pool;

// GPU-side mirror of Projectile_Pool (std430, uploaded to physics compute)
typedef struct {
  float Position[3]; float Pad_A;
  float Velocity[3]; float Lifetime;
  int   Active;      int   Material_Hit;
  float Radius;      float Damage;
  float Hit_U, Hit_V; // UV at impact point (written by GPU on hit)
  int   Instance_Hit; // TLAS instance that was hit (-1 = none)
  int   Pad_B;
} Gpu_Projectile;

typedef struct {
  Gpu_Projectile Slots[MAX_PROJECTILES];
  int   Count;
  float Fire_Cooldown;
  float Pad[2];
} Gpu_Projectile_Pool;

// Material System
typedef struct {
  int   Type;         // MATERIAL_DEFAULT, MATERIAL_METAL, etc.
  float Damage_Scale; // 0.0 (armored) to 1.0 (exposed)
  char  Name[32];     // Human-readable name
} Material;

#define MAX_AUDIO_BUFFERS 32
#define MAX_AUDIO_SOURCES 16

typedef struct {
  ALCdevice  *Device;
  ALCcontext *Context;
  ALuint      Buffers[MAX_AUDIO_BUFFERS];
  int         Buffer_Count;
  ALuint      Sources[MAX_AUDIO_SOURCES];
  int         Source_Count;
  int         Sound_Shoot;  
  int         Sound_Explode;   
  int         Sound_Step_Stone; // Footstep on stone
  int         Sound_Step_Metal; // Footstep on metal
  int         Sound_Jump;
  int         Sound_Land;       // Land after jump
  float       Step_Accumulator; // Distance accumulator for footstep timing
  int         Was_On_Ground;    // Previous frame ground state (for land detection)
} Audio_System;

// GPU-resident buffer with its backing memory and optional device address
typedef struct {
  VkBuffer        Buffer; 
  VkDeviceMemory  Memory;  // Device memory allocation backing the buffer
  VkDeviceAddress Address; // Buffer device address for shader access (zero if not requested)
  uint64_t        Size;    // Allocation size in bytes
} Gpu_Buffer;

// GPU-resident image with its backing memory, view, and format metadata
typedef struct {
  VkImage        Image; 
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

// Quickhull internal types used during hull construction
typedef struct {int A, B, C; int Dead;} Quickhull_Face;
typedef struct {int V0, V1, Face;}      Quickhull_Edge;

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §3. Context
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// List of required ray tracing extension functions
#define VULKAN_FUNCTIONS(_) \
  _(vkCreateAccelerationStructureKHR,           vkCreateAccelerationStructure)           /* Creates a BLAS or TLAS */ \
  _(vkDestroyAccelerationStructureKHR,          vkDestroyAccelerationStructure)          /* Destroys an acceleration structure */ \
  _(vkGetAccelerationStructureBuildSizesKHR,    vkGetAccelerationStructureBuildSizes)    /* Queries scratch and result buf sizes */ \
  _(vkCmdBuildAccelerationStructuresKHR,        vkCmdBuildAccelerationStructures)        /* Records a build or update command */ \
  _(vkGetAccelerationStructureDeviceAddressKHR, vkGetAccelerationStructureDeviceAddress) /* Retrieves the device address */ \
  _(vkCreateRayTracingPipelinesKHR,             vkCreateRayTracingPipelines)             /* Creates a ray tracing pipeline */ \
  _(vkGetRayTracingShaderGroupHandlesKHR,       vkGetRayTracingShaderGroupHandles)       /* Retrieves shader group handles (SBT) */ \
  _(vkCmdTraceRaysKHR,                          vkCmdTraceRays)                          /* Records a ray dispatch command */

// Convenience macro for declaring Vulkan function pointer variables from their spec names
#define DECLARE_VK(vk, alias) PFN_##vk alias;

// Convenience macro for loading Vulkan function pointers from the logical device at runtime
#define LOAD_VK(vk, alias) alias = (PFN_##vk) vkGetDeviceProcAddr (Device, #vk);

// Declare all ray tracing function pointers as globals
VULKAN_FUNCTIONS (DECLARE_VK)

// Assertion to validate Vulkan return values; prints the error code, file, and line number then exits
#define VK_CHECK(Call) do { \
  VkResult _Result = (Call); \
  if (_Result) {fprintf (stderr, "[vulkan] error %d at %s:%d\n", _Result, __FILE__, __LINE__); exit (1);} \
} while (0)

// Command recording and CPU-GPU synchronization
VkCommandPool   Command_Pool;              // Command pool for allocating command buffers
VkCommandBuffer Command_Buffer;            // Single reusable command buffer for all GPU work
VkFence         Fence;                     // CPU-GPU synchronization fence for frame serialization
VkSemaphore     Semaphore_Image_Available; // Signals when a swapchain image is ready
VkSemaphore     Semaphore_Render_Finished; // Signals when rendering is complete for presentation

// Swapchain state
VkSwapchainKHR Swapchain;                               // Presentation swapchain
VkImage        Swapchain_Images [SWAPCHAIN_MAX_IMAGES]; // Swapchain image handles (up to 8 for triple+ buffering)
VkImageView    Swapchain_Views  [SWAPCHAIN_MAX_IMAGES]; // Image views corresponding to each swapchain image
uint           Swapchain_Image_Count;                   // Actual number of swapchain images acquired
VkFormat       Swapchain_Format;                        // Surface format of the swapchain (e.g. B8G8R8A8_SRGB)
VkExtent2D     Swapchain_Extent;                        // Swapchain resolution in pixels

// Diffuse texture array
VkImage        *Texture_Images;   // Array of diffuse texture images
VkDeviceMemory *Texture_Memories; // Backing memory for each texture image
VkImageView    *Texture_Views;    // Image views for shader sampling of each texture
VkSampler       Texture_Sampler;  // Shared sampler with linear filtering and repeat wrap
uint            Texture_Count;    // Total number of texture slots allocated
uint            Textures_Loaded;  // Number of textures successfully loaded from disk
uint            PBR_Stride;       // Stride between PBR map blocks (= scene material count)

// Lightmap atlas
VkImage        Lightmap_Image;   // Packed lightmap atlas image
VkDeviceMemory Lightmap_Memory;  // Backing memory for the lightmap image
VkImageView    Lightmap_View;    // Image view for lightmap sampling
VkSampler      Lightmap_Sampler; // Sampler for lightmap lookups (linear, clamp-to-edge)

// Ray tracing pipeline and shader binding table
VkPipelineCache  Pipeline_Cache;              // Shared pipeline cache - amortizes SPIR-V>ISA compilation
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

// Post-processing pipeline
VkPipeline            Postprocess_Pipeline;
VkPipelineLayout      Postprocess_Pipeline_Layout;
VkDescriptorSetLayout Postprocess_Descriptor_Layout;
VkDescriptorPool      Postprocess_Descriptor_Pool;
VkDescriptorSet       Postprocess_Descriptor_Set;
Gpu_Image             Depth_Image;               // R32F depth output from ray tracing
Gpu_Image             History_Image;             // Previous frame for temporal accumulation (TAA)
Gpu_Image             Postprocess_Output_Image;  // Final post-processed output
int                   Frame_Count = 0;           // Frame counter for TAA convergence
int                   Current_Budget_Byte = 0;   // 0-255, set each frame for denoiser gating
mat4                  Prev_View_Matrix;          // Previous frame's view matrix for TAA reprojection

// A-trous wavelet denoiser
VkPipeline            Denoise_Pipeline;
VkPipelineLayout      Denoise_Pipeline_Layout;
VkDescriptorSetLayout Denoise_Descriptor_Layout;
VkDescriptorPool      Denoise_Descriptor_Pool;
VkDescriptorSet       Denoise_Descriptor_Sets[2];  // Ping-pong: [0] reads A writes B, [1] reads B writes A
Gpu_Image             Denoise_Ping_Image;           // Ping-pong buffer for spatial denoising

// Shader binding table (SBT) alignment and handle sizes
VkPhysicalDeviceRayTracingPipelinePropertiesKHR Raytracing_Properties; 

// BLAS for world geometry and TLAS combining all instances
Acceleration_Structure Bottom_Level, Top_Level; 

// GPU physics pipeline state
VkPipeline            Physics_Pipeline;          // Compute pipeline for physics simulation
VkPipelineLayout      Physics_Pipeline_Layout;   // Pipeline layout with push constants for Gpu_Input
VkDescriptorSetLayout Physics_Descriptor_Layout; // Layout
VkDescriptorPool      Physics_Descriptor_Pool;   // Pool for the physics descriptor set
VkDescriptorSet       Physics_Descriptor_Set;    // Descriptor set binding physics resources
Gpu_Buffer            Player_State_Buffer;       // SSBO holding the Gpu_Player state (read-write each frame)
Gpu_Buffer            Hull_Storage_Buffer;       // SSBO holding Gpu_Hull vertex + adjacency data
Gpu_Buffer            Projectile_Buffer;         // SSBO holding Gpu_Projectile_Pool

// Central rendering context holding all Vulkan state, GPU resources, and synchronization objects
int              Width;                 // Window width in pixels (output/display resolution)
int              Height;                // Window height in pixels
float            Active_Render_Scale;   // Internal RT render scale
int              Active_Denoise_Passes; // A-trous denoise iterations
int              Active_Checkerboard;   // Half-width dispatch (e.g. checkerboard)
int              Render_Width;          // Internal RT render resolution (Width *  Render_Scale)
int              Render_Height;         // Internal RT render resolution (Height * Render_Scale)
VkInstance       Instance;              // Vulkan instance with validation layers
VkSurfaceKHR     Surface;               // Window surface for presentation
VkPhysicalDevice Physical_Device;       // Selected GPU with ray tracing support
VkDevice         Device;                // Logical device created from the physical device
VkQueue          Queue;                 // Universal queue for graphics, compute, and transfer
uint             Queue_Family_Index;    // Index of the queue family supporting all operations

// GPU storage images and scene data buffers
Gpu_Buffer Top_Level_Instance_Buffer;                    // Host-visible instance buffer for writing TLAS instance descriptors each frame
Gpu_Buffer Top_Level_Scratch_Buffer;                     // Persistent scratch memory reused across per-frame TLAS rebuilds
Gpu_Image  Raytracing_Storage_Image;                     // Storage image written by ray generation shader
Gpu_Buffer Camera_Uniform_Buffer;                        // Uniform buffer for the Camera struct
Gpu_Buffer Vertex_Buffer, Index_Buffer, Material_Buffer; // Scene geometry and material data on GPU
Gpu_Buffer Texture_Id_Buffer;                            // Per-triangle texture index buffer

// Audio
Audio_System Audio;

// Projectile pool (CPU-side)
Projectile_Pool Projectiles;

// Application state
int   Quit;       // Non-zero when the application should exit
float Delta_Time; // Time elapsed since the previous frame in seconds

// Runtime mode flags
int Skip_Postprocess; // Non-zero to bypass the post-processing compute pass

// Windowing state and settings
Quality_Level    Active_Quality      = QUALITY_POTATO;
Cursor_Kind      Current_Cursor_Kind = CURSOR_SYSTEM;
Activated_Kind   Current_Activated   = OTHER_ACTIVATED;
Game_Mode_Kind   Current_Game_Mode   = GAME_PLAYING;
Window_Mode_Kind Current_Window_Mode = WINDOWED_MODE;
int              Input_Active        = 1;        // Process input when active (disabled on deactivation)
int              Cursor_Centering    = 0;        // Center cursor each frame (game mode)
int              In_Menu             = 0;        // 1 = menu mode, 0 = game mode
int              Swapchain_Dirty     = 0;        // Non-zero when swapchain needs recreation
int              Saved_Cursor_X, Saved_Cursor_Y; // Cursor position saved across mode transitions
int              Windowed_X, Windowed_Y;         // Saved window position before fullscreen
int              Windowed_W, Windowed_H;         // Saved window size before fullscreen
SDL_Window      *Window;                         // SDL window for presentation and input
SDL_Cursor      *SDL_Cursor_Arrow;               // System arrow cursor (menu default)
SDL_Cursor      *SDL_Cursor_Hand;                // Active cursor (hovering interactive UI)
SDL_Cursor      *SDL_Cursor_Crosshair;           // Inactive cursor (menu, not hovering)

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

// Material surface types for footstep and impact sounds
#define MATERIAL_DEFAULT 0
#define MATERIAL_METAL   1
#define MATERIAL_STONE   2
#define MATERIAL_WOOD    3
#define MATERIAL_FLESH   4
#define MATERIAL_WATER   5
#define MATERIAL_COUNT   6

// Load a TGA image file and decode it into RGBA8 pixel data
uint8_t *TGA_Load (const char *Path, uint *Out_Width, uint *Out_Height);

// Upload raw RGBA pixel data to a device-local texture image via staging buffer
void Texture_Upload_With_Format (VkCommandBuffer Command_Buffer, VkQueue Queue,
                                 const uint8_t *Pixels, uint Width, uint Height, VkFormat Format,
                                 VkImage *Out_Image, VkDeviceMemory *Out_Memory, VkImageView *Out_View);

// Convenience wrapper that uploads a texture as SRGB
void Texture_Upload (VkCommandBuffer Command_Buffer, VkQueue Queue,
                     const uint8_t *Pixels, uint Width, uint Height,
                     VkImage *Out_Image, VkDeviceMemory *Out_Memory, VkImageView *Out_View);

// Create a device-local 2D image suitable for use as a ray tracing storage target. The image is RGBA16F with storage and
// transfer-source usage bits.
Gpu_Image Image_Storage_Create (uint Width, uint Height);

// Insert a pipeline barrier that transitions an image between layouts, specifying the source and destination access masks and pipeline
// stages for proper synchronization. 
void Image_Layout_Barrier (VkCommandBuffer      Command_Buffer, VkImage              Image,
                           VkImageLayout        Old_Layout,     VkImageLayout        New_Layout,
                           VkAccessFlags        Source_Access,  VkAccessFlags        Destination_Access,
                           VkPipelineStageFlags Source_Stage,   VkPipelineStageFlags Destination_Stage);

// Create a sampler with linear filtering and repeating address mode on all axes
VkSampler Sampler_Create_Repeating ();

// Create a sampler with linear filtering and clamp-to-edge on all axes
VkSampler Sampler_Create_Clamping ();

// Load a damage map TGA and sample it at normalized UV coordinates 

// Loaded damage mapping
typedef struct {
  char     Path[256];
  uint8_t *Pixels;
  uint     Width, Height;
} Damage_Map_Cache_Entry;

// Global damage map collection
int                    Damage_Cache_Count = 0;
Damage_Map_Cache_Entry Damage_Cache[DAMAGE_CACHE_MAX];

// Free all cached damage map pixel data
void Damage_Cache_Free ();

// Look up the damage map path for a given model name and body part index (0=head, 1=upper, 2=lower)
const char *Damage_Map_For_Model (const char *Model_Name, int Part_Index);

// Returns a damage multiplier in [0.0, 1.0] where 0.0 = fully armored, 1.0 = critical.
// The damage map is loaded on demand and cached in a table.
float Damage_Map_Sample (const char *Path, float U, float V);

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §7. Models
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Quake 3 model format
#define MD3_MAGIC            0x33504449u // "IDP3" as a 32-bit little-endian integer
#define MD3_MAX_SURFACES     3           // Maximum surfaces per weapon part (body, barrel, hand)
#define MD3_MAX_ANIM_FRAMES  30          // Maximum animation frames extracted from tag_weapon

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

// Forward declarations — these reference types defined later (Vertex, Weapon_Model, Entity, Scene, Spawn)
// so they are placed here as documentation. The implementations appear after the type definitions.
//
//   void MD3_Parse_Surface (const uint8_t *Surface_Data,
//                           Vertex **Inout_Vertices, uint *Inout_Vertex_Count,
//                           uint **Inout_Indices, uint *Inout_Index_Count,
//                           uint **Inout_Texture_Ids, uint *Inout_Triangle_Count,
//                           uint Assigned_Texture_Index, const float *Transform);
//
//   void MD3_Parse_Surface_At_Frame (const uint8_t *Surface_Data, int Frame,
//                                    Vertex **Inout_Vertices, uint *Inout_Vertex_Count,
//                                    uint **Inout_Indices, uint *Inout_Index_Count,
//                                    uint **Inout_Texture_Ids, uint *Inout_Triangle_Count,
//                                    uint Assigned_Texture_Index, const float *Transform);
//
//   Weapon_Model Weapon_Model_Load ();
//   Entity Entity_Load (Scene *S, Spawn Spawn_Point);
//   void Entity_Bottom_Level_Initialize (Entity *Enemy);
//   void Entity_Bottom_Level_Rebuild (Entity *Enemy);
//   void Entity_Assemble_Frame (int Legs_Frame, int Torso_Frame,
//                               uint Body_Mat, uint Gun_Mat, const float World[12],
//                               Vertex **Out_Verts, uint *Out_Vert_Count,
//                               uint **Out_Indices, uint *Out_Index_Count,
//                               uint **Out_Tex_Ids, uint *Out_Tri_Count);

// Compose two tag transforms (each float[12]: origin[3] + axis[9])
void Tag_Compose (const float *A, const float *B, float *C);

// Returns 1 if found, 0 if not. Writes the 12-float transform (origin[3] + axis[9]) to Out
int MD3_Find_Tag_At_Frame (const uint8_t *Data, int Tag_Count, int Tags_Offset,
                                   int Frame, const char *Name, float Out[12]);

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §8. Scene
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Quake 3 BSP format
#define BSP_MAGIC           0x50534249u // "IBSP" as a 32-bit little-endian integer
#define BSP_VERSION         46          // Quake 3 BSP format version number
#define BSP_LUMP_COUNT      17          // Total number of lumps in the BSP directory
#define BSP_ENTITIES        0           // Lump index: entity definitions (key-value text)
#define BSP_SHADERS         1           // Lump index: shader/material name table
#define BSP_PLANES          2           // Lump index: splitting planes for BSP tree and brushes
#define BSP_NODES           3           // Lump index: interior BSP tree nodes
#define BSP_LEAFS           4           // Lump index: BSP tree leaf nodes
#define BSP_LEAF_SURFACES   5           // Lump index: per-leaf surface index lists
#define BSP_LEAF_BRUSHES    6           // Lump index: per-leaf brush index lists
#define BSP_BRUSHES         8           // Lump index: convex brush volumes for collision
#define BSP_BRUSH_SIDES     9           // Lump index: bounding planes for each brush
#define BSP_VERTICES        10          // Lump index: vertex positions, UVs, normals, colors
#define BSP_INDICES         11          // Lump index: triangle mesh element indices
#define BSP_FACES           13          // Lump index: face/surface descriptors
#define BSP_LIGHTMAPS       14          // Lump index: 128×128 RGB lightmap pages
#define SURFACE_TYPE_PLANAR 1           // Face type: flat polygon rendered from indices
#define SURFACE_TYPE_PATCH  2           // Face type: Bézier patch (tessellated at load time)
#define SURFACE_TYPE_MESH   3           // Face type: triangle mesh (e.g. models in BSP)
#define TESSELLATION_LEVEL  5           // Number of subdivisions per Bézier patch edge
#define LIGHTMAP_PAGE_SIZE  128         // Width and height in texels of each lightmap page

// BSP lump directory entry: byte offset and length of a data lump within the file
typedef struct {int Offset, Length;} BSP_Lump;

// BSP file header: magic number, version, and the 17-entry lump directory
typedef struct {
  uint     Magic, Version;         // Magic (0x50534249 = "IBSP") and format version (46 for Quake 3)
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

// Per-scene environment settings
typedef struct {
  vec3   Sun_Direction;      // Normalized world-space sun direction
  vec3   Sun_Color;          // Sun radiance (linear HDR)
  float  Sun_Angular_Radius; // Disk angular radius in radians
  float  Sun_Intensity;      // Radiance multiplier
  vec3   Sky_Zenith;         // Sky color at zenith
  vec3   Sky_Horizon;        // Sky color at horizon
  float  Sky_Intensity;      // Sky ambient multiplier
  vec3   Ambient_Up;         // Ambient irradiance from above
  vec3   Ambient_Down;       // Ambient irradiance from below
  vec3   Fog_Color;          // Fog/haze color
  float  Fog_Density;        // Exponential fog density factor
  float  Sun_Disc_Size;      // Visual angular size of sun disc
  float  Sun_Disc_Intensity; // Brightness of the sun disc in the sky
} Scene_Environment;

const Scene_Environment DEFAULT_ENVIRONMENT = {
  .Sun_Direction      = {0.5f, 0.7f,  0.5f},   // High sun angle
  .Sun_Color          = {1.0f, 0.95f, 0.85f},  // Warm daylight
  .Sun_Angular_Radius = 0.02f,                 // ~1.1 degrees (Earth sun ≈ 0.53°)
  .Sun_Intensity      = 4.0f,                  // Strong direct light
  .Sky_Zenith         = {0.15f, 0.25f, 0.55f}, // Deep blue zenith
  .Sky_Horizon        = {0.4f,  0.5f,  0.6f},  // Hazy lighter blue at horizon
  .Sky_Intensity      = 1.2f,                  // Sky brightness multiplier
  .Ambient_Up         = {0.16f, 0.19f, 0.24f}, // Moderate cool fill - dark shadows but not black
  .Ambient_Down       = {0.12f, 0.10f, 0.08f}, // Subtle warm ground bounce
  .Fog_Color          = {0.35f, 0.38f, 0.42f}, // Subtle atmospheric haze
  .Fog_Density        = 0.00008f,              // Barely visible distance haze
  .Sun_Disc_Size      = 0.03f,                 // Visual disc angular radius
  .Sun_Disc_Intensity = 8.0f,                  // Bright sun disc in sky
};
Scene_Environment Active_Environment;

// BSP Entity System
//
// The entity lump in BSP files is text key/value records. We tokenize and map known keys into
// typed fields, then discard the raw pairs. The discriminant union is the authoritative runtime
// representation. Unknown keys are silently ignored.
//
typedef enum {
  NO_ENTITY = 0,

  // World origin
  ENTITY_WORLD,

  // Player / camera / navigation
  ENTITY_INFO_PLAYER_START,        // info_player_start - single-player spawn
  ENTITY_INFO_PLAYER_SPAWN,        // info_player_deathmatch / team spawn (aliases map here)
  ENTITY_INFO_PLAYER_INTERMISSION, // info_player_intermission - post-match camera
  ENTITY_INFO_CAMERA,              // Fixed camera
  ENTITY_INFO_CAMERA_PATH,         // Camera path node
  ENTITY_INFO_NAV_NODE,            // AI/path node
  ENTITY_INFO_OBJECTIVE_NODE,      // Objective marker node

  // Rendering / environment
  ENTITY_LIGHT,           // Omnidirectional light
  ENTITY_LIGHT_SPOT,      // Spot light
  ENTITY_ENV_FOG,         // Fog volume / exponential fog
  ENTITY_ENV_WIND,        // Wind vector and gusting
  ENTITY_ENV_SKY,         // Sky / sun / skybox controls
  ENTITY_ENV_POSTPROCESS, // Tonemap/bloom/exposure controls
  ENTITY_DECAL,           // Projected decal
  ENTITY_PARTICLE_SYSTEM, // Particle emitter
  ENTITY_SOUND_EMITTER,   // Ambient or positional sound (target_speaker)

  // Static and dynamic props
  ENTITY_PROP_STATIC,  // Static model instance (misc_model)
  ENTITY_PROP_DYNAMIC, // Animated/dynamic prop
  ENTITY_PROP_PHYSICS, // Physics-enabled prop

  // Triggers
  ENTITY_TRIGGER,          // Generic trigger volume
  ENTITY_TRIGGER_ONCE,     // Fires once then disables
  ENTITY_TRIGGER_MULTI,    // Re-fires after wait
  ENTITY_TRIGGER_HURT,     // Damage volume
  ENTITY_TRIGGER_TELEPORT, // Teleporter volume
  ENTITY_TRIGGER_PUSH,     // Jump pad / push volume
  ENTITY_TRIGGER_LADDER,   // Ladder volume
  ENTITY_TRIGGER_WATER,    // Water volume trigger
  ENTITY_TRIGGER_SCRIPT,   // Scripted trigger volume

  // Targets and links
  ENTITY_TARGET_POSITION,    // Destination point
  ENTITY_TARGET_RELAY,       // Relay event
  ENTITY_TARGET_DELAY,       // Delay proxy
  ENTITY_TARGET_RANDOM,      // Random choice proxy
  ENTITY_TARGET_CHANGELEVEL, // Exit transition node

  // Movers
  ENTITY_DOOR_SLIDING,  // Linear door
  ENTITY_DOOR_ROTATING, // Hinged door
  ENTITY_BUTTON,        // Press button
  ENTITY_PLATFORM,      // Up/down lift
  ENTITY_ELEVATOR,      // Multi-stop lift
  ENTITY_TRAIN,         // Path-based mover
  ENTITY_ROTATING,      // Rotating mover
  ENTITY_CONVEYOR,      // Conveyor mover
  ENTITY_BREAKABLE,     // Breakable brush/prop
  ENTITY_EXPLOSIVE,     // Explosive object

  // Items
  ENTITY_ITEM_GENERIC, // General pickup
  ENTITY_ITEM_WEAPON,  // Weapon pickup
  ENTITY_ITEM_AMMO,    // Ammo pickup
  ENTITY_ITEM_HEALTH,  // Health pickup
  ENTITY_ITEM_ARMOR,   // Armor pickup
  ENTITY_ITEM_POWERUP, // Timed powerup pickup
  ENTITY_ITEM_KEY,     // Access pickup

  // Combat and spawners
  ENTITY_PROJECTILE_SPAWNER, // Spawns projectiles periodically or on trigger
  ENTITY_NPC_SPAWNER,        // Spawns NPCs
  ENTITY_ITEM_SPAWNER,       // Spawns items
  ENTITY_TURRET,             // Mounted turret
  ENTITY_VEHICLE,            // Vehicle actor
  ENTITY_NPC,                // NPC actor

  // Objectives and gameflow
  ENTITY_OBJECTIVE,  // Objective entity (capture/defend/use/collect)
  ENTITY_GAME_RULES, // Mode rules, score limits, time limits

  // Pure logic blocks
  ENTITY_LOGIC_RELAY,    // Relay
  ENTITY_LOGIC_TIMER,    // Periodic fire
  ENTITY_LOGIC_COUNTER,  // Count threshold
  ENTITY_LOGIC_COMPARE,  // Compare values
  ENTITY_LOGIC_BRANCH,   // If/else
  ENTITY_LOGIC_RANDOM,   // Random chance
  ENTITY_LOGIC_SEQUENCE, // Sequence of outputs
  ENTITY_SCRIPT,         // Script controller

  ENTITY_KIND_COUNT
} Entity_Kind;

// Weapon archetype identifiers (generic, engine-side)
typedef enum {
  WEAPON_NONE = 0,
  WEAPON_MELEE,   // Gauntlet
  WEAPON_PISTOL,
  WEAPON_SMG,     // Machinegun
  WEAPON_SHOTGUN,
  WEAPON_RIFLE,
  WEAPON_LMG,
  WEAPON_SNIPER,
  WEAPON_GRENADE, // Grenade launcher
  WEAPON_ROCKET,  // Rocket launcher
  WEAPON_ENERGY,  // Plasma gun
  WEAPON_LIGHTNING,
  WEAPON_RAIL,    // Railgun
  WEAPON_BFG,
  WEAPON_KIND_COUNT
} Weapon_Kind;

// Ammo identifiers 
typedef enum {
  AMMO_NONE = 0,
  AMMO_BULLETS,
  AMMO_SHELLS,
  AMMO_SLUGS,
  AMMO_GRENADES,
  AMMO_ROCKETS,
  AMMO_CELLS,
  AMMO_ENERGY,
  AMMO_KIND_COUNT
} Ammo_Kind;

// Powerup identifiers 
typedef enum {
  POWERUP_NONE = 0,
  POWERUP_QUAD_DAMAGE,
  POWERUP_HASTE,
  POWERUP_INVISIBILITY,
  POWERUP_REGENERATION,
  POWERUP_FLIGHT,
  POWERUP_ENV_SUIT,
  POWERUP_INVULNERABLE,
  POWERUP_KIND_COUNT
} Powerup_Kind;

// Objective identifiers )
typedef enum {
  OBJECTIVE_NONE = 0,
  OBJECTIVE_USE,
  OBJECTIVE_CAPTURE,
  OBJECTIVE_DEFEND,
  OBJECTIVE_COLLECT,
  OBJECTIVE_DESTROY,
  OBJECTIVE_ESCAPE,
  OBJECTIVE_HOLD,
  OBJECTIVE_KIND_COUNT
} Objective_Kind;

// Common entity attributes
typedef struct {

  // Identity / linkage
  char Name    [64]; // Graph node name (e.g. "targetname")
  char Target  [64]; // Primary target id
  char Target2 [64]; // Secondary target id
  char Parent  [64]; // Parent attachment id

  // Transform
  vec3  Origin; // World-space position
  vec3  Angles; // Pitch/Yaw/Roll degrees
  float Scale;  // Uniform scale

  // Bounds for volumes and brush entities
  vec3  Mins;   // Local or world mins
  vec3  Maxs;   // Local or world maxs
  float Radius; // Radius for spherical volumes
  float Height; // Height for cylindrical volumes

  // Rendering assets
  char Model    [96];  // Model path (mesh, brush model token, etc)
  char Material [96];  // Material/shader path
  char Sound    [96];  // Sound file path
  char Script   [96];  // Script path
  char Message  [128]; // UI/message string
  vec3 Color;          // RGB (0..1) where applicable
  float Alpha;         // Opacity (1.0 default)

  // Gameplay and physics
  int   Spawnflags; // Generic spawnflags bitfield
  int   Flags;      // Generic runtime flags (engine-defined)
  int   Team;       // Team or faction id
  int   Health;     // Hit points (0 if not damageable)
  int   Armor;      // Armor points (0 if unused)
  int   Damage;     // Damage (for hurt/explosive/etc)
  int   Count;      // Generic count (quantity, etc)
  float Speed;      // Generic speed (doors/movers/projectiles)
  float Accel;    
  float Decel;  
  float Wait;       // Seconds
  float Delay;      // Seconds
  float Random;     // Random variance (seconds or scalar)
} Entity_Common;

// BSP Entity discriminate union
typedef struct {
  Entity_Kind   Kind;   // Discriminant tag
  Entity_Common Common; // Shared attributes 
  union { // case Kind is

    // when ENTITY_WORLD =>
    struct {
      float Gravity;       // World gravity scalar (0 = engine default 800)
      float Time_Limit;    // Match time limit (0 = none)
      int   Score_Limit;   // Score limit (0 = none)
      float Ambient_Light; // Scalar ambient floor
    } world;

    // when ENTITY_INFO_PLAYER_ =>
    struct {
      int   Player_Class;  // Class index (0 = default)
      int   Loadout;       // Loadout id (0 = default)
      float Fov;           // Suggested FOV (0 = engine default)
      float View_Height;   // Standing view height (0 = engine default)
      float Crouch_Height; // Crouch view height (0 = engine default)
    } player;

    // when ENTITY_LIGHT | ENTITY_LIGHT_SPOT =>
    struct {
      float Intensity;    // Luminous intensity / radius scalar
      float Range;        // Explicit range (0 = derived from intensity)
      float Inner_Angle;  // Spot inner cone degrees (spot only)
      float Outer_Angle;  // Spot outer cone degrees (spot only)
      int   Cast_Shadows; // Non-zero = shadow caster
      float Falloff;      // 1 = linear, 2 = quadratic, etc
    } light;

    // when ENTITY_SOUND_EMITTER =>
    struct {
      float Volume;       // 0..1
      float Pitch;        // 1 = normal
      float Min_Distance; // Full volume within this distance
      float Max_Distance; // Silence beyond this distance
      int   Looping;      // Non-zero = loop
    } sound;

    // when ENTITY_DECAL =>
    struct {
      float Size_X, Size_Y; // Projected size
      float Rotation;       // Degrees
      float Fade_Time;      // Seconds until fully faded (0 = never)
    } decal;

    // when ENTITY_PARTICLE_SYSTEM =>
    struct {
      float Rate;     // Particles per second
      float Lifetime; // Seconds
      float Spread;   // Cone spread scalar
      float Velocity; // Initial speed scalar
    } particle;

    // when ENTITY_TRIGGER* =>
    struct {
      int   Enabled;      // Non-zero = active
      int   Filter_Team;  // 0 = any, else team id
      int   Filter_Class; // 0 = any, else class id
      int   Fire_Count;   // How many times it can fire (0 = infinite)
    } trigger;

    // when ENTITY_TRIGGER_TELEPORT =>
    struct {
      int   Preserve_Velocity; // Non-zero = keep incoming velocity
    } teleport;

    // when ENTITY_TRIGGER_PUSH =>
    struct {
      vec3  Push_Dir;   // Direction (unit or non-unit; engine normalizes)
      float Push_Speed; // Magnitude
    } push;

    // when movers/doors/platform/train =>
    struct {
      vec3  Move_Dir;    // Movement direction (normalized by loader)
      float Lip;         // Remaining overlap at end of travel
      float Distance;    // Travel distance (0 = derived from bounds)
      float Open_Angle;  // For rotating doors
      int   Toggle;      // Non-zero = toggle behavior
      int   Starts_Open; // Non-zero = initial state open/active
    } mover;

    // when ENTITY_BREAKABLE | ENTITY_EXPLOSIVE =>
    struct {
      float Explosion_Radius; // Blast radius
      float Explosion_Force;  // Impulse scalar
      int   Gib_Count;        // Debris count
    } breakable;

    // when ENTITY_ITEM_ =>
    struct {
      Weapon_Kind  Weapon;   // For ENTITY_ITEM_WEAPON
      Ammo_Kind    Ammo;     // For ENTITY_ITEM_AMMO
      Powerup_Kind Powerup;  // For ENTITY_ITEM_POWERUP
      int          Key_Id;   // For ENTITY_ITEM_KEY
      int          Respawn;  // Respawn seconds (0 = default)
      float        Duration; // Powerup duration seconds (0 = default)
    } item;

    // when ENTITY_PROJECTILE_SPAWNER =>
    struct {
      float Fire_Rate;        // Shots per second
      float Projectile_Speed; // Speed scalar
      float Spread;           // Spread scalar
      int   Burst;            // Shots per burst
    } projectile_spawner;

    // when ENTITY_TURRET =>
    struct {
      float Yaw_Rate;         // Degrees/sec
      float Pitch_Rate;       // Degrees/sec
      float Yaw_Min, Yaw_Max; // Limits
      float Pitch_Min, Pitch_Max;
      Weapon_Kind Weapon;
    } turret;

    // when ENTITY_VEHICLE =>
    struct {
      float Mass;         // kg scalar
      float Engine_Power; // generic power scalar
      float Turn_Rate;    // degrees/sec
      int   Seats;        // seat count
    } vehicle;

    // when ENTITY_NPC =>
    struct {
      int   Npc_Class;    // class index
      float Aggro_Radius; // detection range
      float Walk_Speed;   // units/sec
      float Run_Speed;    // units/sec
    } npc;

    // when ENTITY_OBJECTIVE =>
    struct {
      Objective_Kind Obj_Kind;
      int   Required_Count; // e.g. collect N items
      float Hold_Time;      // hold/capture seconds
      int   Obj_Team;       // owning team (0 = neutral)
    } objective;

    // when ENTITY_LOGIC_* =>
    struct {
      int   Value_A;
      int   Value_B;
      int   Threshold;
      float Interval;
      float Chance; // 0..1
    } logic;
  };
} BSP_Entity;

#define MAX_BSP_ENTITIES 4096

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
  BSP_Entity *Entities;                     // Parsed entities from the BSP entity lump (heap-allocated)
  uint        Entity_Count;                 // Number of valid entities
} Scene;   

// Single spawn point parsed from the BSP entity lump
typedef struct {vec3 Origin; float Angle;} Spawn; // World-space origin and facing angle in degrees

// Parsed weapon model assembled from multiple MD3 surfaces
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

// Animated entity with pre-computed per-frame vertex data for BLAS refit
#define ENTITY_MAX_FRAMES 16
typedef struct {
  Vertex *Frame_Vertices[ENTITY_MAX_FRAMES]; // Pre-computed world-space vertices for each animation frame
  uint    Frame_Count;                       // Number of animation frames (LEGS_IDLE = 10)
  float   Frame_FPS;                         // Animation playback rate (from animation.cfg)
  float   Animation_Time;                    // Elapsed time accumulator
  uint    Vertex_Count, Index_Count, Triangle_Count;
  uint   *Indices;                           // Shared index array (topology identical across frames)
  uint   *Texture_Ids;                       // Per-triangle global texture indices
  Vertex *Current_Vertices;                  // Pointer to the active frame's vertex data
  Gpu_Buffer             Vertex_Buffer;      // Host-visible, re-uploaded each frame
  Gpu_Buffer             Index_Buffer;       // Device-local, static
  Gpu_Buffer             Texture_Id_Buffer;  // Device-local, static
  Acceleration_Structure Bottom_Level;       // BLAS (refit each frame)
  Gpu_Buffer             Bottom_Level_Scratch;
  vec3                   GL_Origin;          // World-space position in GL Y-up coordinates (for TLAS transform)
  float                  GL_Yaw;             // Entity yaw angle in GL space (radians)
} Entity;

// Player movement state
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

// Parse the BSP entity lump to find the first info_player_deathmatch spawn point. Returns the origin (swizzled to Y-up) and facing angle.
Spawn BSP_Find_Spawn (const uint8_t *File_Data, const BSP_Header *Header);

// Parse all entities from the BSP entity lump into an array of discriminated records. Stores results in Out_Entities and returns the count.
uint BSP_Parse_Entities (const uint8_t *File_Data, const BSP_Header *Header,
                         BSP_Entity *Out_Entities, uint Max_Entities);

// Builds per-scene environment settings by examining BSP shader names
Scene_Environment Environment_Infer_From_Scene (const Scene *S);

// Load a complete scene from a Quake 3 BSP file
Scene Scene_Load_From_BSP (const char *Path, Spawn *Out_Spawn);

// Load textures for every material in the scene. Attempts to load TGA files from the assets
// directory; materials without a texture file fall back to a 1×1 solid-color pixel derived from the hashed shader name.
void Scene_Load_Textures (const Scene *Scene_Data);

// Load the weapon model's TGA textures and append them to the global texture array.
void Weapon_Load_Textures (Weapon_Instance *Weapon);

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §9. Acceleration Structures
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Build the world geometry's bottom-level acceleration structure (BLAS). Uploads the scene
// vertex, index, and material buffers to the GPU, then constructs a single BLAS geometry
// entry covering all triangles. Uses PREFER_FAST_TRACE since the world is static.
Acceleration_Structure Build_World_Bottom_Level (const Scene *Scene_Data);

// Initialize the weapon's BLAS with host-visible vertex buffer (for per-frame updates)
// and ALLOW_UPDATE flag for fast rebuilds. Scratch memory is kept alive for reuse.
void Weapon_Bottom_Level_Initialize (Weapon_Instance *Weapon);

// Rebuild the weapon BLAS from scratch after CPU vertex transformation.
// Re-uploads the vertex buffer and performs a full (non-update) rebuild.
void Weapon_Bottom_Level_Rebuild (Weapon_Instance *Weapon);

// Pre-allocate the top-level acceleration structure (TLAS) for up to Maximum_Instances instance entries
void Top_Level_Initialize (uint Maximum_Instances);

// Rebuild the TLAS each frame with the world BLAS 
void Top_Level_Rebuild (Acceleration_Structure *World, Acceleration_Structure *Weapon, Acceleration_Structure *Enemy,
                        const float *Player_Body_Transform);

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §10. Physics
//
//   GPU-only physics via ray tracing against the TLAS. The compute shader traces rays from the player's expanded shape against world
//   geometry, resolves contacts via a slide-move algorithm, and writes back the updated Gpu_Player state.
//
//   Six collider shapes, each defining a support function s(d̂) : S² > ℝ³:
//     SPHERE      s(d̂) = d̂ · r                               Projectiles, pickups
//     CAPSULE     s(d̂) = d̂ · r + (0, sign(d̂.y) · spine, 0)   Player, NPCs
//     AABB        s(d̂) = sign(d̂) ⊙ extents                  Crates, elevators
//     CYLINDER    s(d̂) = (d̂.xz/‖d̂.xz‖ · r, sign(d̂.y) · h, 0) Barrels, columns
//     ELLIPSOID   s(d̂) = normalize(d̂ ⊘ axes) ⊙ axes · ‖...‖ Vehicles
//     HULL        s(d̂) = argmax(v · d̂) over vertex set       Arbitrary convex models
//
//   Convex hull support uses hill-climbing with adjacency for O(√n) amortized queries on hulls with ≥64 vertices, falling back to O(n)
//   brute-force for smaller hulls.
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Collider Shape Enumeration
//
// Six collider shapes, each defining a support function s(d̂) : S² > ℝ³ from unit directions to
// surface offsets. The GPU physics compute shader switches on this enum to select the appropriate Minkowski support mapping.
//
enum Collider_Shape {SHAPE_SPHERE,    // Projectiles:       s(d̂) = d̂ · r                                   
                     SHAPE_CAPSULE,   // Player, NPCs:      s(d̂) = d̂ · r + (0, sign(d̂.y) · spine, 0)      
                     SHAPE_AABB,      // Crates, elevators: s(d̂) = sign(d̂) ⊙ extents                     
                     SHAPE_CYLINDER,  // Barrels, columns:  s(d̂) = (d̂.xz/‖d̂.xz‖ · r, sign(d̂.y) · h, 0)   
                     SHAPE_ELLIPSOID, // Vehicles:          s(d̂) = normalize(d̂ ⊘ axes) ⊙ axes · ‖...‖     
                     SHAPE_HULL};     // Arbitrary models   s(d̂) = argmax(v · d̂) over vertex set      

// GPU-resident player state uploaded to the physics compute shader
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

// fp16 RLE packing for push constants
uint16_t Float_To_Half (float Value) {
  uint32_t Bits; memcpy (&Bits, &Value, 4);
  uint32_t Sign = (Bits >> 16) & 0x8000;
  int      Exp  = ((Bits >> 23) & 0xFF) - 127 + 15;
  uint32_t Mant = (Bits >> 13) & 0x03FF;
  if (Exp <= 0)  return (uint16_t)Sign;             // Underflow > ~0
  if (Exp >= 31) return (uint16_t)(Sign | 0x7C00);  // Overflow > ~inf
  return (uint16_t)(Sign | ((uint32_t)Exp << 10) | Mant);
}
uint32_t Pack_Half2x16 (float A, float B) {
  return (uint32_t)Float_To_Half (A) | ((uint32_t)Float_To_Half (B) << 16);
}
void Pack_Mat4_Half (const mat4 *M, uint32_t Out[8]) {
  for (int I = 0; I < 8; I++)
    Out[I] = Pack_Half2x16 (M->E[I * 2], M->E[I * 2 + 1]);
}

// Push constants for the post-processing compute shader (56 bytes)
typedef struct {
  float    Time;            // Full-precision seconds since start
  uint32_t Dt_Frame;        // [15:0] = half(Delta_Time), [31:16] = Frame_Count
  uint32_t Velocity;        // [15:0] = half(Velocity_X), [31:16] = half(Velocity_Z)
  uint32_t Speed_Exposure;  // [15:0] = half(Speed),       [31:16] = half(Exposure)
  uint32_t Bloom_Vignette;  // [15:0] = half(Bloom_Strength), [31:16] = half(Vignette_Strength)
  uint32_t Reproject[8];    // packHalf2x16-compressed 4×4 reprojection matrix (Proj * Prev_View * Inv_View)
  uint32_t Inv_Proj_Diag;   // [15:0] = half(InvProj[0][0]), [31:16] = half(InvProj[1][1])
  uint32_t Sun_Screen_Pos;  // [15:0] = half(Sun_Screen_U), [31:16] = half(Sun_Screen_V) - for god rays
  uint32_t Sun_Params;      // [15:0] = half(God_Ray_Intensity), [31:16] = half(Sun_On_Screen) (0 or 1)
} Gpu_Postprocess_Push;

// CPU-side convex hull produced by the Quickhull algorithm. Stores vertex positions and per-vertex
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

// Build a convex hull from a point cloud using the Quickhull algorithm. Returns the hull
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
void Denoise_Pipeline_Create (void);

// Initialize the Gpu_Player state buffer from a CPU-side Player, allocate the hull storage
// buffer (with a 1-vertex dummy if no hull has been uploaded yet), create the descriptor pool and set, and bind all physics resources.
void Physics_Resources_Create (const Player *Initial_State);

// Dispatch the physics compute shader for one frame: push the current input, execute a single
// workgroup, wait for completion, then read back the updated Gpu_Player state into a CPU-side.
Player Physics_Dispatch (Input In, float Delta_Time);

void Projectile_Pool_Readback ();
void Projectile_Pool_Upload ();
void Projectile_Spawn (vec3 Origin, vec3 Direction);

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §11. Pipeline
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Create the ray tracing pipeline with four shader stages
void Raytracing_Pipeline_Create ();

// Build the shader binding table (SBT) by querying shader group 
void Shader_Binding_Table_Create ();

// Allocate the descriptor pool and set, then write the descriptor bindings for the ray tracing pipeline 
void Descriptor_Set_Create (Weapon_Instance *Weapon, Entity *Enemy);

// A-trous wavelet spatial denoiser
void Denoise_Pipeline_Create ();

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §13. Audio
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// Each material is modeled as a bank of damped resonators (modes). An impact excitation
// (short force pulse) drives all modes simultaneously. The resonator bank produces emergent
// timbre from the object's physical resonances - not from hand-crafted oscillators:
//
//   - Modal frequencies and T60 decay times per material
//   - Contact impulse shapes the excitation
//   - Acceleration noise restores high-frequency realism
//

#define MODAL_SAMPLE_RATE 22050
#define MODAL_MAX_MODES   12

typedef struct {
  float a1, a2; // Feedback coefficients (from frequency and damping)
  float b0;     // Input gain (mode excitation weight)
  float y1, y2; // Filter state
} Mode_Resonator;

void Mode_Init (Mode_Resonator *M, float Freq_Hz, float T60, float Gain);
float Mode_Tick (Mode_Resonator *M, float X);

// Material mode tables: {frequency_Hz, T60_seconds, gain}. Derived from measured rigid-body resonances (SIGGRAPH modal synthesis
// literature)
typedef struct {float Freq; float T60; float Gain;} Mode_Spec;

// Stone: dense, high frequencies, medium-long decay
const Mode_Spec Modes_Stone[] = {
  {1200, 0.08,  0.35}, {2400,  0.05, 0.25}, {3600, 0.03, 0.15},
  {4800, 0.02,  0.10}, { 800,  0.10, 0.30}, {1800, 0.06, 0.20},
  {5500, 0.015, 0.08}, {6200,  0.01, 0.05}, { 950, 0.09, 0.25},
  {3200, 0.04,  0.12}, {7000, 0.008, 0.03}, { 420, 0.12, 0.18}};

// Metal: bright, ringing, long decay
const Mode_Spec Modes_Metal[] = {
  { 880, 0.40, 0.30}, {1760, 0.30, 0.25}, {2640, 0.20, 0.18},
  {3520, 0.15, 0.12}, {4400, 0.10, 0.08}, {5280, 0.08, 0.05},
  { 440, 0.50, 0.35}, {1320, 0.35, 0.22}, {6600, 0.05, 0.03},
  {2200, 0.25, 0.15}, {7700, 0.04, 0.02}, { 660, 0.45, 0.28}};

// Wood: warm, short decay, lower partials
const Mode_Spec Modes_Wood[] = {
  { 350, 0.06,  0.40}, { 700, 0.04,  0.30}, {1400, 0.03, 0.20},
  {2100, 0.02,  0.12}, {2800, 0.015, 0.08}, { 500, 0.05, 0.35},
  {1050, 0.035, 0.25}, {3500, 0.01,  0.05}, { 175, 0.08, 0.30},
  {1750, 0.025, 0.15}, {4200, 0.008, 0.03}, { 280, 0.07, 0.22}};

// Flesh: very dull, extremely short decay
const Mode_Spec Modes_Flesh[] = {
  {200, 0.02,  0.50}, {400, 0.015, 0.30}, {600, 0.01,  0.15},
  {150, 0.025, 0.40}, {300, 0.018, 0.25}, {500, 0.012, 0.18},
  {100, 0.03,  0.35}, {250, 0.02,  0.22}, {700, 0.008, 0.10},
  {350, 0.016, 0.20}, {800, 0.006, 0.05}, {450, 0.013, 0.12}};

const Mode_Spec *Material_Modes[] = {Modes_Stone,  // MATERIAL_DEFAULT = stone-like
                                     Modes_Metal,  // MATERIAL_METAL
                                     Modes_Stone,  // MATERIAL_STONE
                                     Modes_Wood,   // MATERIAL_WOOD
                                     Modes_Flesh,  // MATERIAL_FLESH
                                     Modes_Stone}; // MATERIAL_WATER (splashy, use stone as base)

// Generate a PCM buffer from a modal resonator bank excited by a contact impulse.
// Impulse_Strength controls excitation energy (0-1), Duration is output length.
ALuint Audio_Generate_Modal_Impact (int Material, float Impulse_Strength,
                                    float Duration, float Volume);

// Load a WAV file from disk into an OpenAL buffer. Supports 8/16-bit mono/stereo PCM. Returns 0 on failure.
ALuint Audio_Load_WAV (const char *Path);

// Weapon fire = sharp metallic transient (bolt mechanism) + propellant gas expansion.
// Uses metal modes for the mechanism and broadband noise for the gas.
ALuint Audio_Generate_Weapon_Fire (float Volume);

// Explosion = broadband transient + N debris modal impacts (per SIGGRAPH 2008 scaling work).
// The initial shock is a burst of all-mode excitation, followed by randomized sub-impacts.
ALuint Audio_Generate_Explosion_Modal (float Duration, float Volume);

// Try to load a WAV from disk; if missing, fall back to modal synthesis.
// This layering lets us use real recordings when available and physically-based synthesis as a fallback - the best of both worlds.
ALuint Audio_Load_WAV_Or_Modal (const char *Path, int Material, float Impulse,
                                        float Duration, float Volume);

void Audio_Shutdown ();
void Audio_Update_Footsteps (Player *P, float Dt);
void Audio_Play (int Sound_Index, float Volume);
void Audio_Init ();

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §12. Shaders
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Read a SPIR-V binary file from disk and wrap it in a Vulkan shader module.
VkShaderModule Shader_Module_Load (const char *Path);

// Closest-hit shader (rchit). Interpolates vertex attributes at the hit point using barycentric coordinates, etc
glsl rchit Closest_Hit;

// Primary miss shader (rmiss). Called when a ray from the ray generation shader misses all geometry
glsl rmiss Primary_Miss;

// Shadow miss shader (rmiss, index 1). Called when a shadow ray reaches the sun without hitting any occluder
glsl rmiss Shadow_Miss;

// A-Trous Wavelet Spatial Denoiser 
//
// This filter cleans up:
//   - Shadow noise from stochastic shadow sampling
//   - Reflection noise from single-bounce traces
//   - General 1-spp ray tracing noise
// without the temporal lag/ghosting that TAA introduces.
//
glsl comp Denoise;

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §13. Engine
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Upload the camera uniform buffer with the inverse view and projection matrices computed from
// the current player position, yaw, pitch, field-of-view, and aspect ratio.
void Camera_Upload (Camera *State, float Field_Of_View, uint Weapon_Texture_Base, uint PBR_Stride_Value, uint Active_SPP);

// Update the weapon viewmodel's vertex positions each frame based on the camera orientation
void Weapon_Update (Weapon_Instance *Weapon, const Camera *Camera_Data, float Delta_Time, int Fire);

// Sample the current keyboard and mouse state from SDL, returning the frame's input snapshot
Input Poll_Input ();

// Record and submit one frame of ray tracing: bind the pipeline and descriptors
void Raytracing_Frame (Gpu_Postprocess_Push Postprocess);

// Ensures window aspect ratio stays between our constraints
void Constrain_Aspect_Ratio (int *W, int *H);

// Change the visible cursor style in menu mode 
void Set_Menu_Cursor (Cursor_Kind Kind);

// Switch from game to menu: show cursor, unclip, stop centering
void Enter_Menu_Mode ();

// Switch from menu to game: hide cursor, clip to window, center each frame
void Enter_Game_Mode ();

// Saves/restores window position and size across transitions
void Toggle_Fullscreen ();

// Handles focus gain/loss, minimize, and click-activate transitions
void Handle_Activation (Activated_Kind New_State);

// Process SDL events and sample keyboard/mouse state
Input Poll_Input ();

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
void Vulkan_Recreate_Swapchain ();
void Vulkan_Create_Synchronization ();
void Vulkan_Transition_Storage_Image ();

// Destroy old swapchain and create a new one matching the current surface size.
// Called on window resize, fullscreen toggle, or VK_ERROR_OUT_OF_DATE_KHR.
void Vulkan_Recreate_Swapchain ();

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

// ═════════════
//   Mat4_Mul
// ═════════════

mat4 Mat4_Mul (mat4 A, mat4 B) {
  mat4 Result = {0};
  for (int Row = 0; Row < 4; Row++)
    for (int Col = 0; Col < 4; Col++)
      for (int K = 0; K < 4; K++)
        Result.E[Col * 4 + Row] += A.E[K * 4 + Row] * B.E[Col * 4 + K];
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
  Gpu_Image Result = {.Format = VK_FORMAT_R16G16B16A16_SFLOAT};

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

  // Bind image memory
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

  // Allocate device memory and bind it to the image
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

  // We do 16x anisotropic filtering since Hardware anisotropic is essentially free on modern GPUs
  VK_CHECK (vkCreateSampler (/*device      =>*/ Device,
                             /*pCreateInfo =>*/ &(VkSamplerCreateInfo){
                               .sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                               .magFilter        = VK_FILTER_LINEAR,
                               .minFilter        = VK_FILTER_LINEAR,
                               .addressModeU     = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                               .addressModeV     = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                               .addressModeW     = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                               .mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                               .anisotropyEnable = VK_TRUE,
                               .maxAnisotropy    = 16.f,
                               .maxLod           = 12.f},
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
                               .sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                               .magFilter        = VK_FILTER_LINEAR,
                               .minFilter        = VK_FILTER_LINEAR,
                               .addressModeU     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                               .addressModeV     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                               .addressModeW     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                               .mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                               .anisotropyEnable = VK_TRUE,
                               .maxAnisotropy    = 16.f,
                               .maxLod           = 12.f},
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
  size_t Raw_Read_ = fread (Raw, 1, Length, File); (void)Raw_Read_;
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

  // Free the raw file data and return decoded pixels
  free (Raw);
  return Output;

} // Tga_Load

// ═════════════════════
//   Damage_Map_Sample
// ═════════════════════

float Damage_Map_Sample (const char *Path, float U, float V) {
  // Find or load the damage map
  Damage_Map_Cache_Entry *Entry = NULL;
  for (int I = 0; I < Damage_Cache_Count; I++) {
    if (strcmp (Damage_Cache[I].Path, Path) == 0) { Entry = &Damage_Cache[I]; break; }
  }

  // Load the damage map from disk if not cached
  if (not Entry and Damage_Cache_Count < DAMAGE_CACHE_MAX) {
    Entry = &Damage_Cache[Damage_Cache_Count++];
    strncpy (Entry->Path, Path, sizeof (Entry->Path) - 1);
    Entry->Pixels = TGA_Load (Path, &Entry->Width, &Entry->Height);
    if (not Entry->Pixels) { Damage_Cache_Count--; return 0.5f; }
  }
  if (not Entry or not Entry->Pixels) return 0.5f;

  // Wrap UVs to [0,1] and sample the grayscale damage value
  U = U - floorf (U); V = V - floorf (V);
  uint X = (uint)(U * (Entry->Width  - 1));
  uint Y = (uint)(V * (Entry->Height - 1));
  if (X >= Entry->Width)  X = Entry->Width  - 1;
  if (Y >= Entry->Height) Y = Entry->Height - 1;

  // TGA pixels are RGBA, damage map is grayscale so just read R channel
  uint Idx = (Y * Entry->Width + X) * 4;
  float Brightness = Entry->Pixels[Idx] / 255.0f;
  return Brightness; // 0.0 = armored, 1.0 = critical
}

// Look up the damage map path for a given model name and body part index (0=head, 1=upper, 2=lower)
const char *Damage_Map_For_Model (const char *Model_Name, int Part_Index) {
  for (int I = 0; I < DAMAGE_MODEL_COUNT; I++) {
    if (strcmp (DAMAGE_MAP_REGISTRY[I].Model_Name, Model_Name) == 0) {
      if (Part_Index < DAMAGE_MAP_REGISTRY[I].Damage_Map_Count)
        return DAMAGE_MAP_REGISTRY[I].Damage_Maps[Part_Index];
      return DAMAGE_MAP_REGISTRY[I].Damage_Maps[0]; // Fallback to first map
    }
  }
  return NULL; // Unknown model
}

// ═════════════════════
//   Damage_Cache_Free
// ═════════════════════

void Damage_Cache_Free () {
  for (int I = 0; I < Damage_Cache_Count; I++) free (Damage_Cache[I].Pixels);
  Damage_Cache_Count = 0;
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §7. Models
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ═════════════════════
//   MD3_Parse_Surface
// ═════════════════════

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

// ══════════════════════════════
//   MD3_Parse_Surface_At_Frame
// ══════════════════════════════

void MD3_Parse_Surface_At_Frame (const uint8_t *Surface_Data, int Frame,
                                  Vertex **Inout_Vertices,    uint *Inout_Vertex_Count,
                                  uint   **Inout_Indices,     uint *Inout_Index_Count,
                                  uint   **Inout_Texture_Ids, uint *Inout_Triangle_Count,
                                  uint Assigned_Texture_Index, const float *Transform) {

  // Cast raw bytes to the surface header
  const MD3_Surface *Surface = (const MD3_Surface *)Surface_Data;
  uint Base_Vertex = *Inout_Vertex_Count;

  // Clamp frame to valid range
  if (Frame >= Surface->Number_Of_Frames) Frame = 0;

  // Copy triangle indices (topology is shared across all frames)
  const int *Triangles = (const int *)(Surface_Data + Surface->Triangles_Offset);
  *Inout_Indices = realloc (*Inout_Indices, sizeof (uint) * (*Inout_Index_Count + Surface->Number_Of_Triangles * 3));
  for (int I = 0; I < Surface->Number_Of_Triangles * 3; I++)
    (*Inout_Indices)[*Inout_Index_Count + I] = Base_Vertex + (uint)Triangles[I];

  // Assign texture index to every triangle
  *Inout_Texture_Ids = realloc (*Inout_Texture_Ids, sizeof (uint) * (*Inout_Triangle_Count + Surface->Number_Of_Triangles));
  for (int T = 0; T < Surface->Number_Of_Triangles; T++)
    (*Inout_Texture_Ids)[*Inout_Triangle_Count + T] = Assigned_Texture_Index;

  // Vertex data at the specified animation frame (each packed vertex = 8 bytes)
  const uint8_t *Vertex_Data = Surface_Data + Surface->Vertices_Offset
                              + Frame * Surface->Number_Of_Vertices * 8;
  const float *Tex_Coords = (const float *)(Surface_Data + Surface->Texture_Coordinates_Offset);

  // Grow the vertex array for decoded vertices
  *Inout_Vertices = realloc (*Inout_Vertices, sizeof (Vertex) * (*Inout_Vertex_Count + Surface->Number_Of_Vertices));

  // Decode each vertex at the requested frame
  for (int V = 0; V < Surface->Number_Of_Vertices; V++) {
    const int16_t *Coords = (const int16_t *)(Vertex_Data + V * 8);
    float PX = Coords[0] / 64.f, PY = Coords[1] / 64.f, PZ = Coords[2] / 64.f;

    // Decode spherical normal from latitude/longitude byte pair
    uint8_t Lat = Vertex_Data[V * 8 + 6];
    uint8_t Lon = Vertex_Data[V * 8 + 7];
    float LA = Lat * (2.f * (float)M_PI / 255.f);
    float LO = Lon * (2.f * (float)M_PI / 255.f);
    float NX = cosf (LA) * sinf (LO);
    float NY = sinf (LA) * sinf (LO);
    float NZ = cosf (LO);

    // Apply tag transform to position and normal if provided
    if (Transform) {
      float TX  = Transform[3]*PX + Transform[6]*PY + Transform[9]*PZ  + Transform[0];
      float TY  = Transform[4]*PX + Transform[7]*PY + Transform[10]*PZ + Transform[1];
      float TZ  = Transform[5]*PX + Transform[8]*PY + Transform[11]*PZ + Transform[2];
      float TNX = Transform[3]*NX + Transform[6]*NY + Transform[9]*NZ;
      float TNY = Transform[4]*NX + Transform[7]*NY + Transform[10]*NZ;
      float TNZ = Transform[5]*NX + Transform[8]*NY + Transform[11]*NZ;
      PX = TX; PY = TY; PZ = TZ;
      NX = TNX; NY = TNY; NZ = TNZ;
    }

    // Read texture coordinates
    float U = Tex_Coords[V * 2], Tv = Tex_Coords[V * 2 + 1];

    // Assemble the final vertex with Q3 Z-up to Y-up swizzle
    (*Inout_Vertices)[*Inout_Vertex_Count + V] = (Vertex){
      .Position   = {PX, PZ, -PY},   // Q3 Z-up > Y-up swizzle
      .Normal     = {NX, NZ, -NY},
      .Texture_Uv = {U,  Tv},
    };
  }

  // Advance the running totals
  *Inout_Vertex_Count   += Surface->Number_Of_Vertices;
  *Inout_Index_Count    += Surface->Number_Of_Triangles * 3;
  *Inout_Triangle_Count += Surface->Number_Of_Triangles;

} // MD3_Parse_Surface_At_Frame

// ══════════════════════
//   Weapon_Model_Load
// ══════════════════════

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
  size_t Body_Read_ = fread (Body_Data, 1, File_Size, File); (void)Body_Read_;
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
    size_t Barrel_Read_ = fread (Barrel_Data, 1, File_Size, File); (void)Barrel_Read_;
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
    size_t Hand_Read_ = fread (Hand_Data, 1, File_Size, File); (void)Hand_Read_;
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

// ═════════════════════════
//   MD3_Find_Tag_At_Frame
// ═════════════════════════

int MD3_Find_Tag_At_Frame (const uint8_t *Data, int Tag_Count, int Tags_Offset,
                                   int Frame, const char *Name, float Out[12]) {
  const MD3_Tag *Tags = (const MD3_Tag *)(Data + Tags_Offset + Frame * Tag_Count * sizeof (MD3_Tag));
  for (int I = 0; I < Tag_Count; I++) {
    if (strncmp (Tags[I].Name, Name, 64) == 0) {
      memcpy (Out,     Tags[I].Origin, 3 * sizeof (float));
      memcpy (Out + 3, Tags[I].Axis,   9 * sizeof (float));
      return 1;
    }
  }
  return 0;
}

// ═══════════════
//   Tag_Compose
// ═══════════════

void Tag_Compose (const float *A, const float *B, float *C) {
  // C.origin = R_A * B.origin + A.origin
  C[0] = A[3]*B[0] + A[6]*B[1] + A[9]*B[2]  + A[0];
  C[1] = A[4]*B[0] + A[7]*B[1] + A[10]*B[2] + A[1];
  C[2] = A[5]*B[0] + A[8]*B[1] + A[11]*B[2] + A[2];

  // C.axis[j] = R_A * B.axis[j]  (3 column vectors, each 3 floats)
  for (int J = 0; J < 3; J++) {
    int S = 3 + J * 3;
    C[S+0] = A[3]*B[S+0] + A[6]*B[S+1] + A[9]*B[S+2];
    C[S+1] = A[4]*B[S+0] + A[7]*B[S+1] + A[10]*B[S+2];
    C[S+2] = A[5]*B[S+0] + A[8]*B[S+1] + A[11]*B[S+2];
  }
}

// ═════════════════
//   MD3_Load_File
// ═════════════════

uint8_t *MD3_Load_File (const char *Path, long *Out_Size) {
  FILE *F = fopen (Path, "rb");
  if (not F) return NULL;
  fseek (F, 0, SEEK_END);
  *Out_Size = ftell (F);
  rewind (F);
  uint8_t *Data = malloc (*Out_Size);
  size_t Data_Read_ = fread (Data, 1, *Out_Size, F); (void)Data_Read_;
  fclose (F);
  if (*(uint *)Data != MD3_MAGIC) { free (Data); return NULL; }
  return Data;
}

// ═════════════════════════
//   Entity_Assemble_Frame
// ═════════════════════════

void Entity_Assemble_Frame (int Legs_Frame, int Torso_Frame,
                                   uint Body_Mat, uint Gun_Mat, const float World[12],
                                   Vertex **Out_Verts, uint *Out_Vert_Count,
                                   uint **Out_Indices, uint *Out_Index_Count,
                                   uint **Out_Tex_Ids, uint *Out_Tri_Count) {
  *Out_Verts = NULL; *Out_Vert_Count = 0;
  *Out_Indices = NULL; *Out_Index_Count = 0;
  *Out_Tex_Ids = NULL; *Out_Tri_Count = 0;

  // Scratch variable for file sizes
  long File_Size;

  // Lower body (legs)
  uint8_t *Lower_Data = MD3_Load_File (ASSET_ROOT "models/players/sarge/lower.md3", &File_Size);
  if (not Lower_Data) return;
  int Lower_Tag_N  = *(int *)(Lower_Data + 80);
  int Lower_Surf_N = *(int *)(Lower_Data + 84);
  int Lower_Tag_O  = *(int *)(Lower_Data + 96);
  int Lower_Surf_O = *(int *)(Lower_Data + 100);

  // Find the torso attachment tag at the current leg frame
  float Tag_Torso[12] = {0,0,0, 1,0,0, 0,1,0, 0,0,1};
  MD3_Find_Tag_At_Frame (Lower_Data, Lower_Tag_N, Lower_Tag_O, Legs_Frame, "tag_torso", Tag_Torso);

  // Parse each lower-body surface at this leg frame
  const uint8_t *Surf_Cursor = Lower_Data + Lower_Surf_O;
  for (int I = 0; I < Lower_Surf_N; I++) {
    MD3_Parse_Surface_At_Frame (Surf_Cursor, Legs_Frame,
      Out_Verts, Out_Vert_Count, Out_Indices, Out_Index_Count,
      Out_Tex_Ids, Out_Tri_Count, Body_Mat, World);
    Surf_Cursor += ((const MD3_Surface *)Surf_Cursor)->End_Offset;
  }
  free (Lower_Data);

  // Upper body (torso)
  uint8_t *Upper_Data = MD3_Load_File (ASSET_ROOT "models/players/sarge/upper.md3", &File_Size);
  if (not Upper_Data) return;
  int Upper_Tag_N  = *(int *)(Upper_Data + 80);
  int Upper_Surf_N = *(int *)(Upper_Data + 84);
  int Upper_Tag_O  = *(int *)(Upper_Data + 96);
  int Upper_Surf_O = *(int *)(Upper_Data + 100);

  // Compose world and torso transforms for upper body placement
  float Upper_Xform[12];
  Tag_Compose (World, Tag_Torso, Upper_Xform);

  // Find head and weapon attachment tags at the torso frame
  float Tag_Head[12]   = {0,0,0, 1,0,0, 0,1,0, 0,0,1};
  float Tag_Weapon[12] = {0,0,0, 1,0,0, 0,1,0, 0,0,1};
  MD3_Find_Tag_At_Frame (Upper_Data, Upper_Tag_N, Upper_Tag_O, Torso_Frame, "tag_head",   Tag_Head);
  MD3_Find_Tag_At_Frame (Upper_Data, Upper_Tag_N, Upper_Tag_O, Torso_Frame, "tag_weapon", Tag_Weapon);

  // Parse each upper-body surface at this torso frame
  Surf_Cursor = Upper_Data + Upper_Surf_O;
  for (int I = 0; I < Upper_Surf_N; I++) {
    MD3_Parse_Surface_At_Frame (Surf_Cursor, Torso_Frame,
      Out_Verts, Out_Vert_Count, Out_Indices, Out_Index_Count,
      Out_Tex_Ids, Out_Tri_Count, Body_Mat, Upper_Xform);
    Surf_Cursor += ((const MD3_Surface *)Surf_Cursor)->End_Offset;
  }
  free (Upper_Data);

  // Head
  uint8_t *Head_Data = MD3_Load_File (ASSET_ROOT "models/players/sarge/head.md3", &File_Size);
  if (Head_Data) {
    int Head_Surf_N = *(int *)(Head_Data + 84);
    int Head_Surf_O = *(int *)(Head_Data + 100);
    float Head_Xform[12];
    Tag_Compose (Upper_Xform, Tag_Head, Head_Xform);
    Surf_Cursor = Head_Data + Head_Surf_O;
    for (int I = 0; I < Head_Surf_N; I++) {
      MD3_Parse_Surface_At_Frame (Surf_Cursor, 0,
        Out_Verts, Out_Vert_Count, Out_Indices, Out_Index_Count,
        Out_Tex_Ids, Out_Tri_Count, Body_Mat, Head_Xform);
      Surf_Cursor += ((const MD3_Surface *)Surf_Cursor)->End_Offset;
    }
    free (Head_Data);
  }

  // Machinegun body
  uint8_t *Gun_Data = MD3_Load_File (ASSET_ROOT "models/weapons2/machinegun/machinegun.md3", &File_Size);
  if (Gun_Data) {
    int Gun_Surf_N = *(int *)(Gun_Data + 84);
    int Gun_Tag_N  = *(int *)(Gun_Data + 80);
    int Gun_Tag_O  = *(int *)(Gun_Data + 96);
    int Gun_Surf_O = *(int *)(Gun_Data + 100);
    float Gun_Xform[12];
    Tag_Compose (Upper_Xform, Tag_Weapon, Gun_Xform);
    float Tag_Barrel[12] = {0,0,0, 1,0,0, 0,1,0, 0,0,1};
    MD3_Find_Tag_At_Frame (Gun_Data, Gun_Tag_N, Gun_Tag_O, 0, "tag_barrel", Tag_Barrel);
    Surf_Cursor = Gun_Data + Gun_Surf_O;
    for (int I = 0; I < Gun_Surf_N; I++) {
      MD3_Parse_Surface_At_Frame (Surf_Cursor, 0,
        Out_Verts, Out_Vert_Count, Out_Indices, Out_Index_Count,
        Out_Tex_Ids, Out_Tri_Count, Gun_Mat, Gun_Xform);
      Surf_Cursor += ((const MD3_Surface *)Surf_Cursor)->End_Offset;
    }
    free (Gun_Data);

    // Barrel
    uint8_t *Barrel_Data = MD3_Load_File (ASSET_ROOT "models/weapons2/machinegun/machinegun_barrel.md3", &File_Size);
    if (Barrel_Data) {
      int Barrel_Surf_N = *(int *)(Barrel_Data + 84);
      int Barrel_Surf_O = *(int *)(Barrel_Data + 100);
      float Barrel_Xform[12];
      Tag_Compose (Gun_Xform, Tag_Barrel, Barrel_Xform);
      Surf_Cursor = Barrel_Data + Barrel_Surf_O;
      for (int I = 0; I < Barrel_Surf_N; I++) {
        MD3_Parse_Surface_At_Frame (Surf_Cursor, 0,
          Out_Verts, Out_Vert_Count, Out_Indices, Out_Index_Count,
          Out_Tex_Ids, Out_Tri_Count, Gun_Mat, Barrel_Xform);
        Surf_Cursor += ((const MD3_Surface *)Surf_Cursor)->End_Offset;
      }
      free (Barrel_Data);
    }
  }
} // Entity_Assemble_Frame

// ═══════════════
//   Entity_Load
// ═══════════════

Entity Entity_Load (Scene *S, Spawn Spawn_Point) {
  Entity E = {0};

  // Add material entries for entity body skin + gun metal
  uint Body_Mat = S->Material_Count;
  uint Gun_Mat  = S->Material_Count + 1;
  S->Material_Count += 2;
  S->Materials     = realloc (S->Materials,     sizeof (vec4) * S->Material_Count);
  S->Texture_Names = realloc (S->Texture_Names, 64 * S->Material_Count);
  S->Materials[Body_Mat] = (vec4){.x = 0.6f, .y = 0.5f, .z = 0.4f, .w = 1.f};
  S->Materials[Gun_Mat]  = (vec4){.x = 0.3f, .y = 0.3f, .z = 0.3f, .w = 1.f};
  snprintf (S->Texture_Names[Body_Mat], 64, "models/players/grism/enkiskin");
  snprintf (S->Texture_Names[Gun_Mat],  64, "models/weapons2/machinegun/mgun");

  // Compute world placement in Q3 Z-up space
  float Player_Yaw = 1.5707963f - Spawn_Point.Angle * 3.14159f / 180.f;
  vec3 Fwd = {sinf (Player_Yaw), 0.f, -cosf (Player_Yaw)};
  vec3 Enemy_Pos = {
    Spawn_Point.Origin.x + Fwd.x * 80.f,
    Spawn_Point.Origin.y,
    Spawn_Point.Origin.z + Fwd.z * 80.f};
  float QP[3] = {Enemy_Pos.x, -Enemy_Pos.z, Enemy_Pos.y};
  float EA = (Spawn_Point.Angle + 180.f) * 3.14159f / 180.f;
  float CA = cosf (EA), SA = sinf (EA);
  float World[12] = {
    QP[0], QP[1], QP[2],
    CA, SA, 0,
    -SA, CA, 0,
    0, 0, 1};

  // Store GL-space origin and yaw for TLAS instancing (player body reuses this BLAS)
  E.GL_Origin = Enemy_Pos; // Already in GL Y-up coordinates
  E.GL_Yaw    = EA;        // Entity yaw in GL space (rotation around Y-axis)

  // Determine animation frame range
  long Tmp;
  uint8_t *Lower_Check = MD3_Load_File (ASSET_ROOT "models/players/sarge/lower.md3", &Tmp);
  if (not Lower_Check) { printf ("[enemy] lower.md3 not found\n"); return E; }
  int Lower_Frames = *(int *)(Lower_Check + 76);
  free (Lower_Check);

  // Configure idle leg animation frame range and playback rate
  int Legs_Base = 171;  // MD3 frame for first LEGS_IDLE frame
  int Legs_Num  = 10;   // Number of LEGS_IDLE frames
  E.Frame_FPS   = 10.f; // LEGS_IDLE fps
  if (Legs_Base + Legs_Num > Lower_Frames) Legs_Num = Lower_Frames - Legs_Base;
  if (Legs_Num < 1) Legs_Num = 1;
  if (Legs_Num > ENTITY_MAX_FRAMES) Legs_Num = ENTITY_MAX_FRAMES;
  E.Frame_Count = Legs_Num;

  // Set the torso to its standing pose frame
  int Torso_Frame = 151;

  // Pre-compute vertex data for each animation frame
  for (uint F = 0; F < E.Frame_Count; F++) {
    Vertex *Verts = NULL; uint VC = 0;
    uint *Idx = NULL; uint IC = 0;
    uint *Tex = NULL; uint TC = 0;

    // Build the composite model at this animation frame
    Entity_Assemble_Frame (Legs_Base + F, Torso_Frame, Body_Mat, Gun_Mat, World,
                          &Verts, &VC, &Idx, &IC, &Tex, &TC);

    // Cache vertex data for this frame
    E.Frame_Vertices[F] = Verts;

    // Store shared topology from the first frame or free duplicate arrays
    if (F == 0) {
      // First frame establishes the shared topology
      E.Vertex_Count   = VC;
      E.Index_Count    = IC;
      E.Triangle_Count = TC;
      E.Indices        = Idx;
      E.Texture_Ids    = Tex;

    // Subsequent frames must match topology - free duplicate index/texture arrays
    } else {
      free (Idx);
      free (Tex);
    }
  }

  // Initialize animation state to the first frame
  E.Current_Vertices = E.Frame_Vertices[0];
  E.Animation_Time   = 0.f;

  // Log entity statistics
  printf ("[enemy] sarge loaded: %u verts, %u tris, %u animation frames @ %.0f fps\n",
          E.Vertex_Count, E.Triangle_Count, E.Frame_Count, E.Frame_FPS);
  return E;

} // Entity_Load

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §8. Scene - Setup
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ══════════════════════════
//   Vulkan_Create_Instance
// ══════════════════════════

void Vulkan_Create_Instance () {

  // Gather the instance extensions required by SDL for Vulkan surface presentation
  uint Extension_Count;
  SDL_Vulkan_GetInstanceExtensions (Window, &Extension_Count, NULL);
  const char **Extensions = malloc (sizeof (char *) * (Extension_Count + 1));
  SDL_Vulkan_GetInstanceExtensions (Window, &Extension_Count, Extensions);

  // Check if the validation layer is available before requesting it
  uint Layer_Count = 0;
  vkEnumerateInstanceLayerProperties (&Layer_Count, NULL);
  VkLayerProperties *Layers = malloc (sizeof (VkLayerProperties) * Layer_Count);
  vkEnumerateInstanceLayerProperties (&Layer_Count, Layers);

  bool Have_Validation = false;
  for (uint L = 0; L < Layer_Count; L++) {
    if (strcmp (Layers[L].layerName, VALIDATION_LAYERS[0]) == 0) {
      Have_Validation = true;
      break;
    }
  }
  free (Layers);

  // Add the debug utils extension only when validation is present
  uint Ext_Count = Extension_Count;
  if (Have_Validation) Extensions[Ext_Count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

  // Create the Vulkan instance targeting API version 1.3
  VK_CHECK (vkCreateInstance (/*pCreateInfo =>*/ &(VkInstanceCreateInfo){
                                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                .pApplicationInfo = &(VkApplicationInfo){
                                  .sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                  .pApplicationName = "quake3rt",
                                  .apiVersion       = VK_API_VERSION_1_3},
                                .enabledLayerCount       = Have_Validation ? VALIDATION_LAYER_COUNT : 0,
                                .ppEnabledLayerNames     = VALIDATION_LAYERS,
                                .enabledExtensionCount   = Ext_Count,
                                .ppEnabledExtensionNames = Extensions},
                              /*pAllocator  =>*/ NULL,
                              /*pInstance   =>*/ &Instance));

  if (not Have_Validation) printf ("[vulkan] validation layer not found - running without validation\n");

  // Release the temporary extensions array now that the instance owns the data
  free (Extensions);

  // Create the platform window surface via SDL's Vulkan integration
  SDL_Vulkan_CreateSurface (Window, Instance, &Surface);
}

// ═══════════════════════════════
//   Vulkan_Pick_Physical_Device
// ═══════════════════════════════

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

// ═══════════════════════════════════
//   Vulkan_Create_Logical_Device
// ═══════════════════════════════════

void Vulkan_Create_Logical_Device () {

  // Chain together the feature structures for acceleration structure and ray tracing pipeline
  VkPhysicalDeviceAccelerationStructureFeaturesKHR Acceleration_Structure_Features = {
    .sType                  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
    .accelerationStructure  = VK_TRUE};

  // Enable ray query features for the physics compute shader's TLAS ray queries
  VkPhysicalDeviceRayQueryFeaturesKHR Ray_Query_Features = {
    .sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
    .pNext    = &Acceleration_Structure_Features,
    .rayQuery = VK_TRUE};

  // Enable ray tracing pipeline features chained to the ray query features
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR Raytracing_Pipeline_Features = {
    .sType              = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
    .pNext              = &Ray_Query_Features,
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

  // Specify device extensions: core RT + Mesa/Intel optimizations
  const char *Device_Extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                     VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                                     VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                                     VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                                     VK_KHR_RAY_QUERY_EXTENSION_NAME,
                                     VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME};

  // Set queue priority to maximum (1.0) for the single graphics queue
  float Priority = 1.f;

  // Enable core 1.0 features: samplerAnisotropy for 16x anisotropic texture filtering
  VkPhysicalDeviceFeatures Core_Features = {.samplerAnisotropy = VK_TRUE};

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
                              .enabledExtensionCount   = 6,
                              .ppEnabledExtensionNames = Device_Extensions,
                              .pEnabledFeatures        = &Core_Features},
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

// ════════════════════════════
//   Vulkan_Create_Swapchain
// ════════════════════════════

void Vulkan_Create_Swapchain () {
  VkSurfaceCapabilitiesKHR Capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR (Physical_Device, Surface, &Capabilities);

  // When currentExtent is 0xFFFFFFFF the surface size is undefined - use the window size instead
  if (Capabilities.currentExtent.width == 0xFFFFFFFF) {
    int W, H;
    SDL_Vulkan_GetDrawableSize (Window, &W, &H);
    Swapchain_Extent = (VkExtent2D){(uint)W, (uint)H};
    if (Swapchain_Extent.width  < Capabilities.minImageExtent.width)  Swapchain_Extent.width  = Capabilities.minImageExtent.width;
    if (Swapchain_Extent.height < Capabilities.minImageExtent.height) Swapchain_Extent.height = Capabilities.minImageExtent.height;
    if (Swapchain_Extent.width  > Capabilities.maxImageExtent.width)  Swapchain_Extent.width  = Capabilities.maxImageExtent.width;
    if (Swapchain_Extent.height > Capabilities.maxImageExtent.height) Swapchain_Extent.height = Capabilities.maxImageExtent.height;
  } else {
    Swapchain_Extent = Capabilities.currentExtent;
  }

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

// ═════════════════════════════
//   Vulkan_Recreate_Swapchain
// ═════════════════════════════

void Vulkan_Recreate_Swapchain () {
  vkDeviceWaitIdle (Device);
  VkSwapchainKHR Old = Swapchain;

  // Query current surface capabilities for the new swapchain
  VkSurfaceCapabilitiesKHR Capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR (Physical_Device, Surface, &Capabilities);

  // When currentExtent is 0xFFFFFFFF the surface size is undefined - use the window size instead
  if (Capabilities.currentExtent.width == 0xFFFFFFFF) {
    int W, H;
    SDL_Vulkan_GetDrawableSize (Window, &W, &H);
    Swapchain_Extent = (VkExtent2D){(uint)W, (uint)H};
    if (Swapchain_Extent.width  < Capabilities.minImageExtent.width)  Swapchain_Extent.width  = Capabilities.minImageExtent.width;
    if (Swapchain_Extent.height < Capabilities.minImageExtent.height) Swapchain_Extent.height = Capabilities.minImageExtent.height;
    if (Swapchain_Extent.width  > Capabilities.maxImageExtent.width)  Swapchain_Extent.width  = Capabilities.maxImageExtent.width;
    if (Swapchain_Extent.height > Capabilities.maxImageExtent.height) Swapchain_Extent.height = Capabilities.maxImageExtent.height;
  } else {
    Swapchain_Extent = Capabilities.currentExtent;
  }

  // Request one more image than the minimum to avoid stalling
  uint Image_Count = Capabilities.minImageCount + 1;
  if (Capabilities.maxImageCount and Image_Count > Capabilities.maxImageCount)
    Image_Count = Capabilities.maxImageCount;

  // Create the new swapchain, chaining from the old one
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
                                    .clipped          = VK_TRUE,
                                    .oldSwapchain     = Old},
                                  /*pAllocator  =>*/ NULL,
                                  /*pSwapchain  =>*/ &Swapchain));

  // Destroy the old swapchain and retrieve new image handles
  vkDestroySwapchainKHR (Device, Old, NULL);
  vkGetSwapchainImagesKHR (Device, Swapchain, &Swapchain_Image_Count, NULL);
  vkGetSwapchainImagesKHR (Device, Swapchain, &Swapchain_Image_Count, Swapchain_Images);
  printf ("[window] swapchain recreated %ux%u\n", Swapchain_Extent.width, Swapchain_Extent.height);

} // Vulkan_Recreate_Swapchain

// ══════════════════════════════════
//   Vulkan_Create_Synchronization
// ══════════════════════════════════

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

// ══════════════════════════════════════
//   Vulkan_Transition_Storage_Image
// ══════════════════════════════════════

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
// §8. Scene - BSP Loading
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ══════════════════════
//   Convert_BSP_Vertex  
// ══════════════════════

Vertex Convert_BSP_Vertex (const BSP_Vertex *Source) {

  // Swizzle from Id Software's Z-up coordinate system to our Y-up system: (x, y, z) becomes (x, z, -y)
  return (Vertex){
    .Position    = {Source->Position[0],        Source->Position[2],       -Source->Position[1]},
    .Texture_Uv  = {Source->Texture_Coords[0],  Source->Texture_Coords[1]},
    .Lightmap_Uv = {Source->Lightmap_Coords[0], Source->Lightmap_Coords[1]},
    .Normal      = {Source->Normal[0],          Source->Normal[2],         -Source->Normal[1]}
  };
}

// ═══════════════════
//   Bezier_Evaluate
// ═══════════════════

vec3 Bezier_Evaluate (vec3 Control_A, vec3 Control_B, vec3 Control_C, float Parameter) {

  // Evaluate the quadratic Bézier curve: B(t) = (1-t)²·A + 2(1-t)t·B + t²·C
  float Inverse = 1.f - Parameter;
  return Add (Add (Scale (Control_A, Inverse * Inverse),
                   Scale (Control_B, 2.f * Inverse * Parameter)),
              Scale (Control_C, Parameter * Parameter));
}

// ════════════════════════
//   BSP_Tessellate_Patch
// ════════════════════════

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

// ═══════════════════════
//   Scene_Load_From_BSP
// ═══════════════════════

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
  size_t File_Read_ = fread (File_Data, 1, File_Size, File); (void)File_Read_;

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

  // Parse all entities from the BSP entity lump into discriminated records
  Scene Result = {
    .Vertices        = Vertices,       .Vertex_Count    = Vertex_Count,
    .Indices         = Indices,        .Index_Count     = Index_Count,
    .Materials       = Materials,      .Material_Count  = Material_Count,
    .Texture_Ids     = Texture_Ids,    .Texture_Names   = Texture_Names,
    .Lightmap_Atlas  = Lightmap_Atlas, .Lightmap_Width  = Atlas_Width,
    .Lightmap_Height = Atlas_Height,   .Triangle_Count  = Triangle_Count};

  Result.Entities     = calloc (MAX_BSP_ENTITIES, sizeof (BSP_Entity));
  Result.Entity_Count = BSP_Parse_Entities (File_Data, Header, Result.Entities, MAX_BSP_ENTITIES);

  // Release the raw BSP file buffer and return the assembled scene
  free (File_Data);
  printf ("[bsp] %s: %u vertices, %u triangles, %u shaders\n", Path, Vertex_Count, Triangle_Count, Raw_Shader_Count);

  // Return result
  return Result;

} // Scene_Load_From_BSP

// ════════════════════════════════
//   Environment_Infer_From_Scene
// ════════════════════════════════

Scene_Environment Environment_Infer_From_Scene (const Scene *S) {
  Scene_Environment Env = DEFAULT_ENVIRONMENT;

  // 1. Scan shader names for sky-related textures
  char Sky_Shader_Name[64] = {0};
  for (uint I = 0; I < S->Material_Count and S->Texture_Names; I++) {
    const char *Name = S->Texture_Names[I];

    // Q3 sky shaders typically contain "sky" or "skies" in the path
    int Has_Sky = 0;
    for (int C = 0; Name[C] and Name[C + 1] and Name[C + 2]; C++)
      if ((Name[C] == 's' or Name[C] == 'S') and
          (Name[C+1] == 'k' or Name[C+1] == 'K') and
          (Name[C+2] == 'y' or Name[C+2] == 'Y')) { Has_Sky = 1; break; }
    if (Has_Sky) {
      memcpy (Sky_Shader_Name, Name, 63);
      printf ("[environment] found sky shader: %s\n", Sky_Shader_Name);
      break;
    }
  }

  // 2. Try to load the sky texture and extract dominant colors
  if (Sky_Shader_Name[0]) {
    char Sky_Path[256];
    snprintf (Sky_Path, sizeof (Sky_Path), "assets/%s.tga", Sky_Shader_Name);

    uint W = 0, H = 0;
    uint8_t *Pixels = TGA_Load (Sky_Path, &W, &H);
    if (Pixels and W > 0 and H > 0) {
      // Sample sky texture: top quarter = zenith, middle band = horizon. Compute average color for each region (sRGB > linear)
      double Zenith_R = 0, Zenith_G = 0, Zenith_B = 0;
      double Horiz_R = 0, Horiz_G = 0, Horiz_B = 0;
      int Zenith_Count = 0, Horiz_Count = 0;

      // Sample sky texture pixels to compute average zenith and horizon colors
      for (uint Y = 0; Y < H; Y++) {
        float V = (float)Y / (float)H;  // 0=top, 1=bottom
        for (uint X = 0; X < W; X += 4) {  // Sample every 4th pixel for speed
          uint8_t *P = Pixels + (Y * W + X) * 4;

          // sRGB > linear approximation
          float R = powf (P[0] / 255.0f, 2.2f);
          float G = powf (P[1] / 255.0f, 2.2f);
          float B = powf (P[2] / 255.0f, 2.2f);

          if (V < 0.25f) {  // Top quarter = zenith
            Zenith_R += R; Zenith_G += G; Zenith_B += B; Zenith_Count++;
          } else if (V > 0.35f and V < 0.65f) {  // Middle band = horizon
            Horiz_R += R; Horiz_G += G; Horiz_B += B; Horiz_Count++;
          }
        }
      }

      // Set sky zenith color from averaged top-quarter samples
      if (Zenith_Count > 0) {
        Env.Sky_Zenith.x = (float)(Zenith_R / Zenith_Count);
        Env.Sky_Zenith.y = (float)(Zenith_G / Zenith_Count);
        Env.Sky_Zenith.z = (float)(Zenith_B / Zenith_Count);
        printf ("[environment] sky zenith from texture: (%.3f, %.3f, %.3f)\n",
                Env.Sky_Zenith.x, Env.Sky_Zenith.y, Env.Sky_Zenith.z);
      }
      if (Horiz_Count > 0) {
        Env.Sky_Horizon.x = (float)(Horiz_R / Horiz_Count);
        Env.Sky_Horizon.y = (float)(Horiz_G / Horiz_Count);
        Env.Sky_Horizon.z = (float)(Horiz_B / Horiz_Count);
        printf ("[environment] sky horizon from texture: (%.3f, %.3f, %.3f)\n",
                Env.Sky_Horizon.x, Env.Sky_Horizon.y, Env.Sky_Horizon.z);
      }

      // Derive ambient hemisphere from sky colors
      float Sky_Lum = Env.Sky_Zenith.x * 0.2126f + Env.Sky_Zenith.y * 0.7152f + Env.Sky_Zenith.z * 0.0722f;
      float Hor_Lum = Env.Sky_Horizon.x * 0.2126f + Env.Sky_Horizon.y * 0.7152f + Env.Sky_Horizon.z * 0.0722f;
      float Avg_Lum = (Sky_Lum + Hor_Lum) * 0.5f;

      // Compute ambient fill intensity clamped to a usable range
      float Fill = fmaxf (Avg_Lum * 1.5f, 0.15f);
      Fill = fminf (Fill, 0.5f);
      float Inv_Lum = Fill / fmaxf (Avg_Lum, 0.01f);
      Env.Ambient_Up.x = (Env.Sky_Zenith.x * 0.6f + Env.Sky_Horizon.x * 0.4f) * Inv_Lum;
      Env.Ambient_Up.y = (Env.Sky_Zenith.y * 0.6f + Env.Sky_Horizon.y * 0.4f) * Inv_Lum;
      Env.Ambient_Up.z = (Env.Sky_Zenith.z * 0.6f + Env.Sky_Horizon.z * 0.4f) * Inv_Lum;
      Env.Ambient_Down.x = Env.Ambient_Up.x * 0.7f + 0.05f;
      Env.Ambient_Down.y = Env.Ambient_Up.y * 0.6f + 0.03f;
      Env.Ambient_Down.z = Env.Ambient_Up.z * 0.5f + 0.02f;

      // Infer sun color from the brightest region of the sky texture (the bright area near the sun is typically warm/white)
      float Max_Bright = 0;
      float Sun_R = 0, Sun_G = 0, Sun_B = 0;
      for (uint Y = 0; Y < H; Y++) {
        for (uint X = 0; X < W; X += 8) {
          uint8_t *P = Pixels + (Y * W + X) * 4;
          float Lum = P[0] * 0.2126f + P[1] * 0.7152f + P[2] * 0.0722f;
          if (Lum > Max_Bright) {
            Max_Bright = Lum;
            Sun_R = powf (P[0] / 255.0f, 2.2f);
            Sun_G = powf (P[1] / 255.0f, 2.2f);
            Sun_B = powf (P[2] / 255.0f, 2.2f);
          }
        }
      }
      if (Max_Bright > 100) {  // Bright enough to be a sun-like source
        float Norm = 1.0f / fmaxf (fmaxf (Sun_R, Sun_G), fmaxf (Sun_B, 0.01f));
        Env.Sun_Color.x = Sun_R * Norm; Env.Sun_Color.y = Sun_G * Norm; Env.Sun_Color.z = Sun_B * Norm;
        printf ("[environment] sun color from texture: (%.3f, %.3f, %.3f)\n",
                Env.Sun_Color.x, Env.Sun_Color.y, Env.Sun_Color.z);
      }

      // Release the sky texture pixel data
      free (Pixels);
    } else {
      printf ("[environment] sky texture not found at %s - using defaults\n", Sky_Path);
    }
  } else {
    printf ("[environment] no sky shader found - using default environment\n");
  }

  // 3. Check worldspawn entity for any explicit overrides
  for (uint I = 0; I < S->Entity_Count; I++) {
    if (S->Entities[I].Kind == ENTITY_WORLD) {
      // Q3 worldspawn can have _sun_angle, _sun_color, etc. in custom maps. For now, just log the worldspawn presence
      printf ("[environment] worldspawn entity found\n");
      break;
    }
  }

  // Log the final environment parameters
  printf ("[environment] sun direction: (%.2f, %.2f, %.2f) intensity: %.1f\n",
          Env.Sun_Direction.x, Env.Sun_Direction.y, Env.Sun_Direction.z, Env.Sun_Intensity);
  printf ("[environment] ambient up: (%.3f, %.3f, %.3f) down: (%.3f, %.3f, %.3f)\n",
          Env.Ambient_Up.x, Env.Ambient_Up.y, Env.Ambient_Up.z,
          Env.Ambient_Down.x, Env.Ambient_Down.y, Env.Ambient_Down.z);
  return Env;

} // Environment_Infer_From_Scene

// ══════════════════
//   BSP_Find_Spawn
// ══════════════════

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

  // Fall back to world origin if no spawn entity was found
  printf ("[bsp] no spawn found, using origin\n");
  return Result;

} // BSP_Find_Spawn

// ══════════════════════
//   BSP_Parse_Entities
// ══════════════════════

void Classify_Entity (const char *Classname, int Length, BSP_Entity *E) {

  // Macros for concise string matching - populate Kind and sub-kind in one step
  #define MATCH(STR, KIND) \
    if (Length == (int)sizeof(STR) - 1 and memcmp (Classname, STR, sizeof(STR) - 1) == 0) { E->Kind = KIND; return; }
  #define MATCH_WEAPON(STR, WK, CNT) \
    if (Length == (int)sizeof(STR) - 1 and memcmp (Classname, STR, sizeof(STR) - 1) == 0) { \
      E->Kind = ENTITY_ITEM_WEAPON; E->item.Weapon = WK; E->Common.Count = CNT; return; }
  #define MATCH_AMMO(STR, AK, CNT) \
    if (Length == (int)sizeof(STR) - 1 and memcmp (Classname, STR, sizeof(STR) - 1) == 0) { \
      E->Kind = ENTITY_ITEM_AMMO; E->item.Ammo = AK; E->Common.Count = CNT; return; }
  #define MATCH_HEALTH(STR, AMT) \
    if (Length == (int)sizeof(STR) - 1 and memcmp (Classname, STR, sizeof(STR) - 1) == 0) { \
      E->Kind = ENTITY_ITEM_HEALTH; E->Common.Health = AMT; return; }
  #define MATCH_ARMOR(STR, AMT) \
    if (Length == (int)sizeof(STR) - 1 and memcmp (Classname, STR, sizeof(STR) - 1) == 0) { \
      E->Kind = ENTITY_ITEM_ARMOR; E->Common.Armor = AMT; return; }
  #define MATCH_POWERUP(STR, PK) \
    if (Length == (int)sizeof(STR) - 1 and memcmp (Classname, STR, sizeof(STR) - 1) == 0) { \
      E->Kind = ENTITY_ITEM_POWERUP; E->item.Powerup = PK; E->item.Duration = 30.f; return; }

  // Spawn points
  MATCH ("info_player_deathmatch",  ENTITY_INFO_PLAYER_SPAWN);
  MATCH ("info_player_start",       ENTITY_INFO_PLAYER_START);
  MATCH ("info_player_intermission",ENTITY_INFO_PLAYER_INTERMISSION);

  // Weapons (Q3 classname > generic Weapon_Kind + default ammo count)
  MATCH_WEAPON ("weapon_gauntlet",        WEAPON_MELEE,      0);
  MATCH_WEAPON ("weapon_shotgun",         WEAPON_SHOTGUN,    10);
  MATCH_WEAPON ("weapon_machinegun",      WEAPON_SMG,        40);
  MATCH_WEAPON ("weapon_grenadelauncher", WEAPON_GRENADE,    10);
  MATCH_WEAPON ("weapon_rocketlauncher",  WEAPON_ROCKET,     10);
  MATCH_WEAPON ("weapon_lightning",       WEAPON_LIGHTNING,   100);
  MATCH_WEAPON ("weapon_railgun",         WEAPON_RAIL,       10);
  MATCH_WEAPON ("weapon_plasmagun",       WEAPON_ENERGY,     50);
  MATCH_WEAPON ("weapon_bfg",             WEAPON_BFG,        20);

  // Ammo (Q3 classname > generic Ammo_Kind + count)
  MATCH_AMMO ("ammo_shells",     AMMO_SHELLS,    10);
  MATCH_AMMO ("ammo_bullets",    AMMO_BULLETS,   50);
  MATCH_AMMO ("ammo_grenades",   AMMO_GRENADES,  5);
  MATCH_AMMO ("ammo_cells",      AMMO_CELLS,     30);
  MATCH_AMMO ("ammo_lightning",  AMMO_ENERGY,    60);
  MATCH_AMMO ("ammo_rockets",    AMMO_ROCKETS,   5);
  MATCH_AMMO ("ammo_slugs",      AMMO_SLUGS,     10);
  MATCH_AMMO ("ammo_bfg",        AMMO_CELLS,     15);

  // Health
  MATCH_HEALTH ("item_health_small", 5);
  MATCH_HEALTH ("item_health",       25);
  MATCH_HEALTH ("item_health_large", 50);
  MATCH_HEALTH ("item_health_mega",  100);

  // Armor
  MATCH_ARMOR ("item_armor_shard",  5);
  MATCH_ARMOR ("item_armor_combat", 50);
  MATCH_ARMOR ("item_armor_body",   100);

  // Powerups
  MATCH_POWERUP ("item_quad",    POWERUP_QUAD_DAMAGE);
  MATCH_POWERUP ("item_enviro",  POWERUP_ENV_SUIT);
  MATCH_POWERUP ("item_haste",   POWERUP_HASTE);
  MATCH_POWERUP ("item_invis",   POWERUP_INVISIBILITY);
  MATCH_POWERUP ("item_regen",   POWERUP_REGENERATION);
  MATCH_POWERUP ("item_flight",  POWERUP_FLIGHT);

  // Holdables > generic items
  MATCH ("holdable_teleporter",     ENTITY_ITEM_GENERIC);
  MATCH ("holdable_medkit",         ENTITY_ITEM_GENERIC);

  // Map geometry & logic
  MATCH ("trigger_teleport",        ENTITY_TRIGGER_TELEPORT);
  MATCH ("trigger_push",            ENTITY_TRIGGER_PUSH);
  MATCH ("target_position",         ENTITY_TARGET_POSITION);
  MATCH ("target_speaker",          ENTITY_SOUND_EMITTER);
  MATCH ("misc_model",              ENTITY_PROP_STATIC);
  MATCH ("light",                   ENTITY_LIGHT);
  MATCH ("worldspawn",              ENTITY_WORLD);

  #undef MATCH
  #undef MATCH_WEAPON
  #undef MATCH_AMMO
  #undef MATCH_HEALTH
  #undef MATCH_ARMOR
  #undef MATCH_POWERUP
}

// ══════════════════════
//   BSP_Parse_Entities
// ══════════════════════

uint BSP_Parse_Entities (const uint8_t *File_Data, const BSP_Header *Header,
                         BSP_Entity *Out_Entities, uint Max_Entities) {
  const char *Text = (const char *)(File_Data + Header->Lumps[BSP_ENTITIES].Offset);
  const char *End  = Text + Header->Lumps[BSP_ENTITIES].Length;
  uint Count = 0;

  while (Text < End and Count < Max_Entities) {
    // Find opening brace
    while (Text < End and *Text != '{') Text++;
    if (Text >= End) break;
    Text++;

    // Temporary storage for this entity's key-value pairs
    BSP_Entity Entity = {0};
    Entity.Common.Scale = 1.0f;   // Default scale
    Entity.Common.Alpha = 1.0f;   // Default opacity
    char Classname[64]   = {0};
    int  Classname_Len   = 0;
    float Color[3]       = {1, 1, 1};
    float Intensity      = 300;
    int   Gravity        = 800;

    // Parse key-value pairs
    while (Text < End and *Text != '}') {
      while (Text < End and (*Text == ' ' or *Text == '\t' or *Text == '\n' or *Text == '\r'))
        Text++;
      if (Text >= End or *Text == '}') break;

      // Read quoted key
      if (*Text != '"') { Text++; continue; }
      Text++;
      const char *Key = Text;
      while (Text < End and *Text != '"') Text++;
      int Key_Len = (int)(Text - Key);
      if (Text < End) Text++;

      // Skip whitespace
      while (Text < End and (*Text == ' ' or *Text == '\t')) Text++;

      // Read quoted value
      if (Text >= End or *Text != '"') continue;
      Text++;
      const char *Val = Text;
      while (Text < End and *Text != '"') Text++;
      int Val_Len = (int)(Text - Val);
      if (Text < End) Text++;

      // Helper: copy value into a fixed buffer
      #define COPY_VAL(DST, MAX) do { \
        int _n = Val_Len < (MAX)-1 ? Val_Len : (MAX)-1; \
        memcpy(DST, Val, _n); DST[_n] = 0; \
      } while(0)

      // Dispatch on key name - populate Entity_Common fields directly
      if (Key_Len == 9 and memcmp (Key, "classname", 9) == 0) {
        Classname_Len = Val_Len < 63 ? Val_Len : 63;
        memcpy (Classname, Val, Classname_Len);
        Classname[Classname_Len] = 0;
      }
      else if (Key_Len == 6 and memcmp (Key, "origin", 6) == 0) {
        char Tmp[64]; COPY_VAL(Tmp, 64);
        sscanf (Tmp, "%f %f %f", &Entity.Common.Origin.x, &Entity.Common.Origin.y, &Entity.Common.Origin.z);
      }
      else if (Key_Len == 5 and memcmp (Key, "angle", 5) == 0) {
        char Tmp[32]; COPY_VAL(Tmp, 32);
        sscanf (Tmp, "%f", &Entity.Common.Angles.y);  // Q3 "angle" = yaw
      }
      else if (Key_Len == 10 and memcmp (Key, "spawnflags", 10) == 0) {
        char Tmp[32]; COPY_VAL(Tmp, 32);
        sscanf (Tmp, "%d", &Entity.Common.Spawnflags);
      }
      else if (Key_Len == 10 and memcmp (Key, "targetname", 10) == 0) { COPY_VAL(Entity.Common.Name, 64); }
      else if (Key_Len == 6  and memcmp (Key, "target", 6)     == 0) { COPY_VAL(Entity.Common.Target, 64); }
      else if (Key_Len == 5  and memcmp (Key, "noise",  5)     == 0) { COPY_VAL(Entity.Common.Sound, 96); }
      else if (Key_Len == 5  and memcmp (Key, "model",  5)     == 0) { COPY_VAL(Entity.Common.Model, 96); }
      else if (Key_Len == 7  and memcmp (Key, "message",7)     == 0) { COPY_VAL(Entity.Common.Message, 128); }
      else if (Key_Len == 5  and memcmp (Key, "light",  5)     == 0) {
        char Tmp[32]; COPY_VAL(Tmp, 32); sscanf (Tmp, "%f", &Intensity);
      }
      else if (Key_Len == 7 and memcmp (Key, "gravity", 7) == 0) {
        char Tmp[32]; COPY_VAL(Tmp, 32); sscanf (Tmp, "%d", &Gravity);
      }
      else if (Key_Len == 6 and memcmp (Key, "_color", 6) == 0) {
        char Tmp[64]; COPY_VAL(Tmp, 64); sscanf (Tmp, "%f %f %f", &Color[0], &Color[1], &Color[2]);
      }

      #undef COPY_VAL
    }
    if (Text < End) Text++; // skip closing brace

    // Classify: populates Entity.Kind and item sub-kinds (Weapon_Kind, Ammo_Kind, etc.)
    Classify_Entity (Classname, Classname_Len, &Entity);

    // Skip unrecognized entity classnames
    if (Entity.Kind == NO_ENTITY) continue; // skip unknown entities

    // Populate kind-specific fields from parsed temporaries
    switch (Entity.Kind) {
      case ENTITY_TRIGGER_TELEPORT:

        // Target already in Entity.Common.Target from key parsing
        break;
      case ENTITY_TRIGGER_PUSH:

        // Target already in Entity.Common.Target from key parsing
        break;
      case ENTITY_SOUND_EMITTER:

        // Sound path already in Entity.Common.Sound from "noise" key
        Entity.sound.Looping = (Entity.Common.Spawnflags & 1) ? 1 : 0;
        break;
      case ENTITY_PROP_STATIC:

        // Model path already in Entity.Common.Model from "model" key
        Entity.Common.Scale = 1.0f;
        break;
      case ENTITY_LIGHT:
        Entity.light.Intensity = Intensity;
        Entity.Common.Color = (vec3){Color[0], Color[1], Color[2]};
        break;
      case ENTITY_WORLD:
        Entity.world.Gravity = (float)Gravity;

        // Message already in Entity.Common.Message from key parsing
        break;
      default: break;
    }

    // Store the fully parsed entity in the output array
    Out_Entities[Count++] = Entity;
  }

  // Print summary
  int N_Items = 0, N_Weapons = 0, N_Spawns = 0, N_Lights = 0;
  for (uint I = 0; I < Count; I++) {
    Entity_Kind K = Out_Entities[I].Kind;
    if (K == ENTITY_ITEM_WEAPON)  N_Weapons++;
    if (K >= ENTITY_ITEM_GENERIC and K <= ENTITY_ITEM_KEY) N_Items++;
    if (K == ENTITY_INFO_PLAYER_SPAWN or K == ENTITY_INFO_PLAYER_START) N_Spawns++;
    if (K == ENTITY_LIGHT) N_Lights++;
  }
  printf ("[bsp] entities: %u total, %d spawns, %d weapons, %d items, %d lights\n",
          Count, N_Spawns, N_Weapons, N_Items, N_Lights);

  // Return result
  return Count;

} // Parse_BSP_Entities

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §8. Scene - Texture Loading
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ═══════════════════════
//   Scene_Load_Textures
// ═══════════════════════

void Scene_Load_Textures (const Scene *Scene_Data) {
  Texture_Sampler  = Sampler_Create_Repeating ();

  // PBR texture layout in the bindless array:
  //   [0 .. N-1]       diffuse maps
  //   [N .. 2N-1]      normal maps    (_n.tga)
  //   [2N .. 3N-1]     roughness maps (_r.tga)
  //   [3N .. 4N-1]     metalness maps (_m.tga)
  //   [4N .. 5N-1]     emissive maps  (_e.tga)
  //   [5N .. 6N-1]     height maps    (_h.tga)
  //   [6N ..]          weapon textures (appended later)
  uint Material_Count = Scene_Data->Material_Count;
  uint PBR_Slots      = Material_Count * 6;  // 6 maps per material (diffuse + 5 PBR)
  PBR_Stride       = Material_Count;         // Distance between map blocks in the texture array
  Texture_Count    = PBR_Slots;
  Textures_Loaded  = 0;
  Texture_Images   = calloc (PBR_Slots, sizeof (VkImage));
  Texture_Memories = calloc (PBR_Slots, sizeof (VkDeviceMemory));
  Texture_Views    = calloc (PBR_Slots, sizeof (VkImageView));

  // PBR map suffixes: [0]=diffuse (no suffix), [1]=normal, [2]=roughness, [3]=metalness, [4]=emissive, [5]=height
  const char *PBR_Suffixes[] = {"", "_n", "_r", "_m", "_e", "_h"};

  // Default fallback pixels for each PBR map type
  uint8_t Fallback_Normal[4]    = {127, 127, 255, 255};  // flat normal pointing up
  uint8_t Fallback_Roughness[4] = {180, 180, 180, 255};  // moderate roughness ~0.7 (overridden per-material below)
  uint8_t Fallback_Metalness[4] = {0,   0,   0,   255};  // non-metallic (overridden per-material below)
  uint8_t Fallback_Emissive[4]  = {0,   0,   0,   255};  // no emission
  uint8_t Fallback_Height[4]    = {127, 127, 127, 255};  // mid-height
  uint8_t *PBR_Fallbacks[]      = {NULL, Fallback_Normal, Fallback_Roughness, Fallback_Metalness, Fallback_Emissive, Fallback_Height};

  // Per-material PBR classification: hand-tuned roughness and metalness for each BSP texture based on real-world material properties.
  // These override the uniform fallback when no explicit PBR map exists. Values are [roughness_byte, metalness_byte] (0-255 > 0.0-1.0).
  // Stored as a parallel array: Material_PBR[material_index] = {R, M}.
  uint8_t (*Material_PBR)[2] = calloc (Material_Count, sizeof (uint8_t[2]));
  for (uint I = 0; I < Material_Count; I++) {
    // Default: moderate stone (R=0.75, M=0.0)
    Material_PBR[I][0] = 191;  Material_PBR[I][1] = 0;
    if (not Scene_Data->Texture_Names) continue;
    const char *N = Scene_Data->Texture_Names[I];

    // Stone / brick / block: rough, non-metallic
    if (strstr (N, "gothic_block") or strstr (N, "gothic_wall/street"))
      { Material_PBR[I][0] = 204; Material_PBR[I][1] = 0; }     // R=0.80, M=0.00 - rough stone
    else if (strstr (N, "proto_brik"))
      { Material_PBR[I][0] = 209; Material_PBR[I][1] = 0; }     // R=0.82, M=0.00 - brick
    else if (strstr (N, "gothic_wall"))
      { Material_PBR[I][0] = 191; Material_PBR[I][1] = 0; }     // R=0.75, M=0.00 - generic wall

    // Metal trim / rust
    else if (strstr (N, "pitted_rust"))
      { Material_PBR[I][0] = 166; Material_PBR[I][1] = 153; }   // R=0.65, M=0.60 - corroded metal
    else if (strstr (N, "deeprust"))
      { Material_PBR[I][0] = 191; Material_PBR[I][1] = 128; }   // R=0.75, M=0.50 - heavy rust
    else if (strstr (N, "pewter"))
      { Material_PBR[I][0] = strstr(N,"dirty") ? 140 : 102;     // dirty=0.55, clean=0.40
        Material_PBR[I][1] = strstr(N,"dirty") ? 166 : 179; }   // dirty=0.65, clean=0.70
    else if (strstr (N, "border7") or strstr (N, "baseboard"))
      { Material_PBR[I][0] = 153; Material_PBR[I][1] = 89; }    // R=0.60, M=0.35 - mixed trim

    // Tech walls
    else if (strstr (N, "atech"))
      { Material_PBR[I][0] = 115; Material_PBR[I][1] = 128; }   // R=0.45, M=0.50 - brushed metal panels
    else if (strstr (N, "ceilingtech"))
      { Material_PBR[I][0] = 140; Material_PBR[I][1] = 77; }    // R=0.55, M=0.30 - ceiling panel

    // Wood
    else if (strstr (N, "wood"))
      { Material_PBR[I][0] = 204; Material_PBR[I][1] = 0; }     // R=0.80, M=0.00 - dry wood

    // Floor
    else if (strstr (N, "gothic_floor") or strstr (N, "floor"))
      { Material_PBR[I][0] = 166; Material_PBR[I][1] = 0; }     // R=0.65, M=0.00 - worn floor stone

    // Light panels (EMISSIVE - PBR values less important)
    else if (strstr (N, "light") or strstr (N, "xlight"))
      { Material_PBR[I][0] = 77;  Material_PBR[I][1] = 0; }     // R=0.30, M=0.00 - smooth glass cover

    // Skull / bone decorations
    else if (strstr (N, "skull"))
      { Material_PBR[I][0] = 191; Material_PBR[I][1] = 13; }    // R=0.75, M=0.05 - bone

    // Lava
    else if (strstr (N, "lava"))
      { Material_PBR[I][0] = 26;  Material_PBR[I][1] = 0; }     // R=0.10, M=0.00 - molten liquid

    // SFX (flames, beams, flares)
    else if (strstr (N, "sfx/flame") or strstr (N, "sfx/beam") or strstr (N, "flameflare"))
      { Material_PBR[I][0] = 26;  Material_PBR[I][1] = 0; }     // R=0.10, M=0.00 - emissive effect

    // Window / glass
    else if (strstr (N, "window"))
      { Material_PBR[I][0] = 51;  Material_PBR[I][1] = 26; }    // R=0.20, M=0.10 - smooth glass

    // Torch model
    else if (strstr (N, "torch"))
      { Material_PBR[I][0] = 153; Material_PBR[I][1] = 77; }    // R=0.60, M=0.30 - metal + wood

    // Player skin (cloth + leather + armor plates)
    else if (strstr (N, "players/"))
      { Material_PBR[I][0] = 179; Material_PBR[I][1] = 20; }    // R=0.70, M=0.08 - mostly cloth, hint of metal

    // Weapon metal
    else if (strstr (N, "weapons"))
      { Material_PBR[I][0] = 77;  Material_PBR[I][1] = 217; }   // R=0.30, M=0.85 - gun steel
  }

  // Initialize PBR loading counters
  uint PBR_Maps_Loaded = 0, PBR_Generated = 0;

  // Pass 1: Load diffuse textures, retaining pixel data for PBR derivation
  uint8_t **Diffuse_Pixels = calloc (Material_Count, sizeof (uint8_t *));
  uint    *Diffuse_W       = calloc (Material_Count, sizeof (uint));
  uint    *Diffuse_H       = calloc (Material_Count, sizeof (uint));

  for (uint Index = 0; Index < Material_Count; Index++) {
    uint Slot = Index;  // diffuse = Map_Type 0
    uint W = 0, H = 0;
    uint8_t *Pixels = NULL;
    if (Scene_Data->Texture_Names) {
      char Path[256];
      snprintf (Path, sizeof (Path), "assets/%s.tga", Scene_Data->Texture_Names[Index]);
      Pixels = TGA_Load (Path, &W, &H);
    }
    if (Pixels and W and H) {
      Texture_Upload_With_Format (/*Command_Buffer =>*/ Command_Buffer,
                                  /*Queue          =>*/ Queue,
                                  /*Pixels         =>*/ Pixels,
                                  /*Width          =>*/ W,
                                  /*Height         =>*/ H,
                                  /*Format         =>*/ VK_FORMAT_R8G8B8A8_SRGB,
                                  /*Out_Image      =>*/ &Texture_Images[Slot],
                                  /*Out_Memory     =>*/ &Texture_Memories[Slot],
                                  /*Out_View       =>*/ &Texture_Views[Slot]);
      Diffuse_Pixels[Index] = Pixels;  // retain for PBR derivation
      Diffuse_W[Index] = W;
      Diffuse_H[Index] = H;
      Textures_Loaded++;
    } else {
      free (Pixels);
      vec4 Color = Scene_Data->Materials[Index];
      uint8_t Fallback[4] = {(uint8_t)(Color.x * 255), (uint8_t)(Color.y * 255), (uint8_t)(Color.z * 255), 255};
      Texture_Upload_With_Format (/*Command_Buffer =>*/ Command_Buffer,
                                  /*Queue          =>*/ Queue,
                                  /*Pixels         =>*/ Fallback,
                                  /*Width          =>*/ 1,
                                  /*Height         =>*/ 1,
                                  /*Format         =>*/ VK_FORMAT_R8G8B8A8_SRGB,
                                  /*Out_Image      =>*/ &Texture_Images[Slot],
                                  /*Out_Memory     =>*/ &Texture_Memories[Slot],
                                  /*Out_View       =>*/ &Texture_Views[Slot]);
    }
  }

  // Pass 2: Load or derive PBR maps (normal, roughness, metalness, emissive, height)
  for (uint Map_Type = 1; Map_Type < 6; Map_Type++) {
    for (uint Index = 0; Index < Material_Count; Index++) {
      uint Slot = Map_Type * Material_Count + Index;
      uint W = 0, H = 0;
      uint8_t *Pixels = NULL;

      if (Scene_Data->Texture_Names) {
        char Path[256];
        snprintf (Path, sizeof (Path), "assets/%s%s.tga", Scene_Data->Texture_Names[Index], PBR_Suffixes[Map_Type]);
        Pixels = TGA_Load (Path, &W, &H);
      }

      VkFormat Fmt = VK_FORMAT_R8G8B8A8_UNORM;

      if (Pixels and W and H) {
        Texture_Upload_With_Format (/*Command_Buffer =>*/ Command_Buffer,
                                    /*Queue          =>*/ Queue,
                                    /*Pixels         =>*/ Pixels,
                                    /*Width          =>*/ W,
                                    /*Height         =>*/ H,
                                    /*Format         =>*/ Fmt,
                                    /*Out_Image      =>*/ &Texture_Images[Slot],
                                    /*Out_Memory     =>*/ &Texture_Memories[Slot],
                                    /*Out_View       =>*/ &Texture_Views[Slot]);
        free (Pixels);
        PBR_Maps_Loaded++;
      } else {
        free (Pixels);

        // Derive PBR map from diffuse texture
        uint8_t *Diff = Diffuse_Pixels[Index];
        uint DW = Diffuse_W[Index], DH = Diffuse_H[Index];
        uint8_t Base_R = Material_PBR[Index][0];
        uint8_t Base_M = Material_PBR[Index][1];

        if (Diff and DW > 1 and DH > 1) {
          // We have real diffuse pixels - generate a full-resolution PBR map
          uint Pixel_Count = DW * DH;
          uint8_t *Gen = malloc (Pixel_Count * 4);

          if (Map_Type == 1) {
            // Normal map: Sobel filter on diffuse luminance. Produces tangent-space normals: cracks and mortar joints in stone become
            // visible geometric detail under directional lighting.
            for (uint Y = 0; Y < DH; Y++) {
              for (uint X = 0; X < DW; X++) {
                // Wrap-safe sampling (textures tile)
                #define LUM(px, py) ({ \
                  uint sx = ((px) % DW), sy = ((py) % DH); \
                  uint8_t *P = &Diff[(sy * DW + sx) * 4]; \
                  (float)P[0] * 0.2126f + (float)P[1] * 0.7152f + (float)P[2] * 0.0722f; \
                })

                // Sobel 3×3
                float TL = LUM(X-1,Y-1), TC = LUM(X,Y-1), TR = LUM(X+1,Y-1);
                float ML = LUM(X-1,Y),                      MR = LUM(X+1,Y);
                float BL = LUM(X-1,Y+1), BC = LUM(X,Y+1), BR = LUM(X+1,Y+1);
                float Gx = (TR + 2*MR + BR) - (TL + 2*ML + BL);
                float Gy = (BL + 2*BC + BR) - (TL + 2*TC + TR);

                // Scale: 0.4 = subtle but visible bumps (not too aggressive)
                float Scale = 0.4f;
                float Nx = -Gx * Scale / 255.0f;
                float Ny = -Gy * Scale / 255.0f;
                float Nz = 1.0f;
                float Len = sqrtf (Nx*Nx + Ny*Ny + Nz*Nz);
                Nx /= Len; Ny /= Len; Nz /= Len;
                uint Idx = (Y * DW + X) * 4;
                Gen[Idx+0] = (uint8_t)((Nx * 0.5f + 0.5f) * 255);
                Gen[Idx+1] = (uint8_t)((Ny * 0.5f + 0.5f) * 255);
                Gen[Idx+2] = (uint8_t)((Nz * 0.5f + 0.5f) * 255);
                Gen[Idx+3] = 255;
                #undef LUM
              }
            }

          // Roughness map: base ~ luminance variation. Darker cracks in stone = rougher; bright polished areas = smoother. Metal: dark
          // oxidation = rougher; bright highlights = smoother.
          } else if (Map_Type == 2) {
            for (uint I = 0; I < Pixel_Count; I++) {
              uint8_t *P = &Diff[I * 4];
              float Lu = (float)P[0] * 0.2126f + (float)P[1] * 0.7152f + (float)P[2] * 0.0722f;
              float Norm_Lu = Lu / 255.0f;

              // Variation: ~0.08 around the base roughness
              float R_Val = (float)Base_R / 255.0f + (0.5f - Norm_Lu) * 0.16f;
              if (R_Val < 0.0f) R_Val = 0.0f;
              if (R_Val > 1.0f) R_Val = 1.0f;
              uint8_t V = (uint8_t)(R_Val * 255);
              Gen[I*4+0] = V; Gen[I*4+1] = V; Gen[I*4+2] = V; Gen[I*4+3] = 255;
            }

          // Metalness map: base ~ saturation/brightness detection. For metallic materials: desaturated bright pixels > more metallic.
          // For non-metallic: stay near zero with slight variation.
          } else if (Map_Type == 3) {
            for (uint I = 0; I < Pixel_Count; I++) {
              uint8_t *P = &Diff[I * 4];
              float R = P[0]/255.0f, G = P[1]/255.0f, B = P[2]/255.0f;
              float Hi = R > G ? (R > B ? R : B) : (G > B ? G : B);
              float Lo = R < G ? (R < B ? R : B) : (G < B ? G : B);
              float Sat = Hi > 0.001f ? (Hi - Lo) / Hi : 0.0f;
              float Lu  = R * 0.2126f + G * 0.7152f + B * 0.0722f;
              float M_Val = (float)Base_M / 255.0f;

              // For metallic materials: boost metalness in bright desaturated areas
              if (Base_M > 50) {
                float Metal_Boost = (1.0f - Sat) * Lu * 0.15f;
                M_Val += Metal_Boost - 0.05f;
              }
              if (M_Val < 0.0f) M_Val = 0.0f;
              if (M_Val > 1.0f) M_Val = 1.0f;
              uint8_t V = (uint8_t)(M_Val * 255);
              Gen[I*4+0] = V; Gen[I*4+1] = V; Gen[I*4+2] = V; Gen[I*4+3] = 255;
            }

          // Emissive map: for light/lava/sfx materials, threshold on brightness
          } else if (Map_Type == 4) {
            bool Is_Emissive = false;
            if (Scene_Data->Texture_Names) {
              const char *N = Scene_Data->Texture_Names[Index];
              Is_Emissive = strstr(N,"light") or strstr(N,"lava") or strstr(N,"flame")
                         or strstr(N,"flare") or strstr(N,"beam") or strstr(N,"sfx");
            }
            for (uint I = 0; I < Pixel_Count; I++) {
              uint8_t *P = &Diff[I * 4];
              if (Is_Emissive) {
                // Emissive: bright diffuse areas glow
                float Lu = (float)P[0] * 0.2126f + (float)P[1] * 0.7152f + (float)P[2] * 0.0722f;
                float Glow = (Lu / 255.0f - 0.3f) / 0.7f;
                if (Glow < 0.0f) Glow = 0.0f;
                Gen[I*4+0] = (uint8_t)(P[0] * Glow);
                Gen[I*4+1] = (uint8_t)(P[1] * Glow);
                Gen[I*4+2] = (uint8_t)(P[2] * Glow);
              } else {
                Gen[I*4+0] = 0; Gen[I*4+1] = 0; Gen[I*4+2] = 0;
              }
              Gen[I*4+3] = 255;
            }

          // Height map: luminance-based. Stone/brick: inverted luminance (dark cracks = deeper). Metal: direct luminance (bright
          // highlights = raised)
          } else {
            bool Invert = (Base_M < 80);  // non-metallic > invert
            for (uint I = 0; I < Pixel_Count; I++) {
              uint8_t *P = &Diff[I * 4];
              float Lu = (float)P[0] * 0.2126f + (float)P[1] * 0.7152f + (float)P[2] * 0.0722f;

              // Map to 0.3-0.7 range (avoid extremes that cause parallax artifacts)
              float H = Lu / 255.0f * 0.4f + 0.3f;
              if (Invert) H = 1.0f - H;
              uint8_t V = (uint8_t)(H * 255);
              Gen[I*4+0] = V; Gen[I*4+1] = V; Gen[I*4+2] = V; Gen[I*4+3] = 255;
            }
          }

          // Upload texture to GPU
          Texture_Upload_With_Format (/*Command_Buffer =>*/ Command_Buffer,
                                      /*Queue          =>*/ Queue,
                                      /*Pixels         =>*/ Gen,
                                      /*Width          =>*/ DW,
                                      /*Height         =>*/ DH,
                                      /*Format         =>*/ Fmt,
                                      /*Out_Image      =>*/ &Texture_Images[Slot],
                                      /*Out_Memory     =>*/ &Texture_Memories[Slot],
                                      /*Out_View       =>*/ &Texture_Views[Slot]);
          free (Gen);
          PBR_Generated++;

        // No diffuse data - use 1×1 classified fallback
        } else {
          if (Map_Type == 2) {
            uint8_t R_Pixel[4] = {Base_R, Base_R, Base_R, 255};
            Texture_Upload_With_Format (/*Command_Buffer =>*/ Command_Buffer,
                                        /*Queue          =>*/ Queue,
                                        /*Pixels         =>*/ R_Pixel,
                                        /*Width          =>*/ 1,
                                        /*Height         =>*/ 1,
                                        /*Format         =>*/ Fmt,
                                        /*Out_Image      =>*/ &Texture_Images[Slot],
                                        /*Out_Memory     =>*/ &Texture_Memories[Slot],
                                        /*Out_View       =>*/ &Texture_Views[Slot]);
          } else if (Map_Type == 3) {
            uint8_t M_Pixel[4] = {Base_M, Base_M, Base_M, 255};
            Texture_Upload_With_Format (/*Command_Buffer =>*/ Command_Buffer,
                                        /*Queue          =>*/ Queue,
                                        /*Pixels         =>*/ M_Pixel,
                                        /*Width          =>*/ 1,
                                        /*Height         =>*/ 1,
                                        /*Format         =>*/ Fmt,
                                        /*Out_Image      =>*/ &Texture_Images[Slot],
                                        /*Out_Memory     =>*/ &Texture_Memories[Slot],
                                        /*Out_View       =>*/ &Texture_Views[Slot]);
          } else {
            Texture_Upload_With_Format (/*Command_Buffer =>*/ Command_Buffer,
                                        /*Queue          =>*/ Queue,
                                        /*Pixels         =>*/ PBR_Fallbacks[Map_Type],
                                        /*Width          =>*/ 1,
                                        /*Height         =>*/ 1,
                                        /*Format         =>*/ Fmt,
                                        /*Out_Image      =>*/ &Texture_Images[Slot],
                                        /*Out_Memory     =>*/ &Texture_Memories[Slot],
                                        /*Out_View       =>*/ &Texture_Views[Slot]);
          }
        }
      }
    }
  }

  // Free retained diffuse pixel data
  for (uint I = 0; I < Material_Count; I++) free (Diffuse_Pixels[I]);
  free (Diffuse_Pixels);
  free (Diffuse_W);
  free (Diffuse_H);
  free (Material_PBR);

  // Upload per-triangle texture IDs as a storage buffer
  Texture_Id_Buffer = Buffer_Stage_Upload (/*Command_Buffer =>*/ Command_Buffer,
                                           /*Queue          =>*/ Queue,
                                           /*Data           =>*/ Scene_Data->Texture_Ids,
                                           /*Size           =>*/ sizeof (uint) * Scene_Data->Triangle_Count,
                                           /*Usage          =>*/ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

  // Log texture loading statistics and material names
  printf ("[textures] loaded %u/%u diffuse, %u PBR from disk, %u PBR generated, total %u slots\n",
          Textures_Loaded, Material_Count, PBR_Maps_Loaded, PBR_Generated, Texture_Count);
  for (uint I = 0; I < Material_Count and Scene_Data->Texture_Names; I++)
    printf ("[material %2u] %s\n", I, Scene_Data->Texture_Names[I]);

  // Upload the lightmap atlas (or a 1x1 white fallback if no lightmaps exist)
  Lightmap_Sampler = Sampler_Create_Clamping ();
  if (Scene_Data->Lightmap_Atlas and Scene_Data->Lightmap_Width and Scene_Data->Lightmap_Height) {
    Texture_Upload_With_Format (/*Command_Buffer =>*/ Command_Buffer,
                                /*Queue          =>*/ Queue,
                                /*Pixels         =>*/ Scene_Data->Lightmap_Atlas,
                                /*Width          =>*/ Scene_Data->Lightmap_Width,
                                /*Height         =>*/ Scene_Data->Lightmap_Height,
                                /*Format         =>*/ VK_FORMAT_R8G8B8A8_SRGB,
                                /*Out_Image      =>*/ &Lightmap_Image,
                                /*Out_Memory     =>*/ &Lightmap_Memory,
                                /*Out_View       =>*/ &Lightmap_View);
    printf ("[lightmap] uploaded %ux%u atlas (SRGB - auto-linearized on sample)\n", Scene_Data->Lightmap_Width, Scene_Data->Lightmap_Height);
  } else {
    uint8_t White[4] = {255, 255, 255, 255};
    Texture_Upload_With_Format (/*Command_Buffer =>*/ Command_Buffer,
                                /*Queue          =>*/ Queue,
                                /*Pixels         =>*/ White,
                                /*Width          =>*/ 1,
                                /*Height         =>*/ 1,
                                /*Format         =>*/ VK_FORMAT_R8G8B8A8_SRGB,
                                /*Out_Image      =>*/ &Lightmap_Image,
                                /*Out_Memory     =>*/ &Lightmap_Memory,
                                /*Out_View       =>*/ &Lightmap_View);
  }
} // BSP_Parse_Entities

// ════════════════════════
//   Weapon_Load_Textures
// ════════════════════════

void Weapon_Load_Textures (Weapon_Instance *Weapon) {

  // Record the starting index in the global texture array for this weapon's textures
  Weapon->Texture_Base_Index = Texture_Count;

  // 6 map types per weapon texture: diffuse, normal, roughness, metalness, emissive, height
  const char *Weapon_PBR_Suffixes[] = {"", "_n", "_r", "_m", "_e", "_h"};
  const uint8_t Weapon_PBR_Fallbacks[][4] = {
    {180, 180, 180, 255},   // diffuse: grey
    {128, 128, 255, 255},   // normal: flat (0,0,1) encoded as (128,128,255)
    { 30,  30,  30, 255},   // roughness: smooth metallic weapon (0.12)
    {230, 230, 230, 255},   // metalness: highly metallic weapon (0.90)
    {  0,   0,   0, 255},   // emissive: none
    {128, 128, 128, 255},   // height: mid-level
  };

  // Grow the global texture arrays to hold weapon PBR slots
  uint Weapon_PBR_Maps = WEAPON_TEXTURE_COUNT * 6;
  uint New_Total = Texture_Count + Weapon_PBR_Maps;
  Texture_Images   = realloc (Texture_Images,   sizeof (VkImage)        * New_Total);
  Texture_Memories = realloc (Texture_Memories,  sizeof (VkDeviceMemory) * New_Total);
  Texture_Views    = realloc (Texture_Views,     sizeof (VkImageView)    * New_Total);

  // Load weapon textures: 6 PBR map types × WEAPON_TEXTURE_COUNT textures
  uint Weapon_PBR_Loaded = 0;
  for (uint Map_Type = 0; Map_Type < 6; Map_Type++) {
    for (uint Index = 0; Index < WEAPON_TEXTURE_COUNT; Index++) {
      uint Slot = Texture_Count + Map_Type * WEAPON_TEXTURE_COUNT + Index;
      uint Img_W = 0, Img_H = 0;
      uint8_t *Pixels = NULL;

      // Build the PBR variant path
      char Path[256];
      snprintf (Path, sizeof Path, "%s", WEAPON_TEXTURE_PATHS[Index]);
      char *Ext = strstr (Path, ".tga");
      if (Ext) {
        snprintf (Ext, sizeof Path - (size_t)(Ext - Path), "%s.tga", Weapon_PBR_Suffixes[Map_Type]);
        Pixels = TGA_Load (Path, &Img_W, &Img_H);
      }

      // Diffuse maps use SRGB; PBR maps (normal, roughness, metalness, emissive, height) use UNORM
      VkFormat Fmt = (Map_Type == 0) ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

      if (Pixels and Img_W and Img_H) {
        Texture_Upload_With_Format (/*Command_Buffer =>*/ Command_Buffer,
                                    /*Queue          =>*/ Queue,
                                    /*Pixels         =>*/ Pixels,
                                    /*Width          =>*/ Img_W,
                                    /*Height         =>*/ Img_H,
                                    /*Format         =>*/ Fmt,
                                    /*Out_Image      =>*/ &Texture_Images[Slot],
                                    /*Out_Memory     =>*/ &Texture_Memories[Slot],
                                    /*Out_View       =>*/ &Texture_Views[Slot]);
        free (Pixels);
        if (Map_Type == 0)
          printf ("[weapon] loaded texture %s (%ux%u)\n", Path, Img_W, Img_H);
        else
          Weapon_PBR_Loaded++;
      } else {
        Texture_Upload_With_Format (/*Command_Buffer =>*/ Command_Buffer,
                                    /*Queue          =>*/ Queue,
                                    /*Pixels         =>*/ (uint8_t *)Weapon_PBR_Fallbacks[Map_Type],
                                    /*Width          =>*/ 1,
                                    /*Height         =>*/ 1,
                                    /*Format         =>*/ Fmt,
                                    /*Out_Image      =>*/ &Texture_Images[Slot],
                                    /*Out_Memory     =>*/ &Texture_Memories[Slot],
                                    /*Out_View       =>*/ &Texture_Views[Slot]);
        if (Map_Type == 0)
          printf ("[weapon] fallback texture for %s\n", WEAPON_TEXTURE_PATHS[Index]);
      }
    }
  }
  Texture_Count += Weapon_PBR_Maps;
  printf ("[weapon] textures: base=%u, count=%u (diffuse), PBR maps=%u\n",
          Weapon->Texture_Base_Index, WEAPON_TEXTURE_COUNT, Weapon_PBR_Loaded);
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §10. Acceleration Structures
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ════════════════════════════
//   Build_World_Bottom_Level
// ════════════════════════════

Acceleration_Structure Build_World_Bottom_Level (const Scene *Scene_Data) {

  // Upload scene vertex, index, and material data to device-local GPU buffers for BLAS construction and shader access
  Vertex_Buffer = Buffer_Stage_Upload (/*Command_Buffer =>*/ Command_Buffer,
                                       /*Queue          =>*/ Queue,
                                       /*Data           =>*/ Scene_Data->Vertices,
                                       /*Size           =>*/ sizeof (Vertex) * Scene_Data->Vertex_Count,
                                       /*Usage          =>*/ VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                                           | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                           | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);

  // Upload the index buffer to device-local memory for BLAS construction and shader access
  Index_Buffer = Buffer_Stage_Upload (/*Command_Buffer =>*/ Command_Buffer,
                                      /*Queue          =>*/ Queue,
                                      /*Data           =>*/ Scene_Data->Indices,
                                      /*Size           =>*/ sizeof (uint) * Scene_Data->Index_Count,
                                      /*Usage          =>*/ VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                                          | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                          | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);

  // Allocate a host-visible material buffer for per-surface RGBA tints that shaders can reference via buffer device address
  Material_Buffer = Buffer_Allocate (/*Size         =>*/ sizeof (vec4) * Scene_Data->Material_Count,
                                     /*Usage        =>*/ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                       | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                     /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
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

  // Query required acceleration structure and scratch buffer sizes from the driver for the given triangle geometry
  uint Primitive_Count = Scene_Data->Triangle_Count;
  VkAccelerationStructureBuildSizesInfoKHR Build_Sizes = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
  vkGetAccelerationStructureBuildSizes (/*device    =>*/ Device,
                                        /*buildType =>*/ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                        /*pBuildInfo =>*/ &Build_Info,
                                        /*pMaxPrimitiveCounts =>*/ &Primitive_Count,
                                        /*pSizeInfo =>*/ &Build_Sizes);

  // Allocate the acceleration structure buffer and create the BLAS object
  Acceleration_Structure Result = {0};
  Result.Buffer = Buffer_Allocate (/*Size         =>*/ Build_Sizes.accelerationStructureSize,
                                   /*Usage        =>*/ VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
                                                     | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                   /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Create the bottom-level acceleration structure object backed by the allocated buffer
  VK_CHECK (vkCreateAccelerationStructure (/*device      =>*/ Device,
                                           /*pCreateInfo =>*/ &(VkAccelerationStructureCreateInfoKHR){
                                             .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                                             .buffer = Result.Buffer.Buffer,
                                             .size   = Build_Sizes.accelerationStructureSize,
                                             .type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR},
                                           /*pAllocator  =>*/ NULL,
                                           /*pStructure  =>*/ &Result.Handle));

  // Allocate temporary scratch memory for the build operation (freed after the build completes)
  Gpu_Buffer Scratch = Buffer_Allocate (/*Size         =>*/ Build_Sizes.buildScratchSize,
                                        /*Usage        =>*/ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                          | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                        /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Finalize the build info with the destination structure and scratch address
  Build_Info.dstAccelerationStructure  = Result.Handle;
  Build_Info.scratchData.deviceAddress = Scratch.Address;

  // Set up the build range covering all triangles in one geometry
  VkAccelerationStructureBuildRangeInfoKHR Range = {.primitiveCount = Primitive_Count};
  const VkAccelerationStructureBuildRangeInfoKHR *Range_Pointer = &Range;

  // Record and submit a one-shot command buffer to build the BLAS on the GPU
  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (/*commandBuffer =>*/ Command_Buffer,
                                  /*pBeginInfo    =>*/ &(VkCommandBufferBeginInfo){
                                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));

  // Record the BLAS build command into the command buffer
  vkCmdBuildAccelerationStructures (/*commandBuffer =>*/ Command_Buffer,
                                    /*infoCount     =>*/ 1,
                                    /*pInfos        =>*/ &Build_Info,
                                    /*ppBuildRangeInfos =>*/ &Range_Pointer);
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));
  VK_CHECK (vkQueueSubmit (/*queue       =>*/ Queue,
                           /*submitCount =>*/ 1,
                           /*pSubmits    =>*/ &(VkSubmitInfo){
                             .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .commandBufferCount = 1,
                             .pCommandBuffers    = &Command_Buffer},
                           /*fence       =>*/ VK_NULL_HANDLE));

  // Wait for the build to complete before querying the device address
  VK_CHECK (vkQueueWaitIdle (Queue));

  // Query the device address of the built BLAS for referencing from the TLAS instance data
  Result.Address = vkGetAccelerationStructureDeviceAddress (/*device =>*/ Device,
                     /*pInfo  =>*/ &(VkAccelerationStructureDeviceAddressInfoKHR){
                       .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                       .accelerationStructure = Result.Handle});

  // Free the scratch buffer (no longer needed after the build)
  vkDestroyBuffer (Device, Scratch.Buffer, NULL);
  vkFreeMemory    (Device, Scratch.Memory, NULL);
  return Result;

} // Build_World_Bottom_Level

// ══════════════════════════════════
//   Weapon_Bottom_Level_Initialize
// ══════════════════════════════════

void Weapon_Bottom_Level_Initialize (Weapon_Instance *Weapon) {
  if (not Weapon->Model.Vertex_Count) return;

  // Allocate a host-visible copy of the weapon vertices for per-frame CPU transformation
  Weapon->Transformed_Vertices = malloc (sizeof (Vertex) * Weapon->Model.Vertex_Count);
  memcpy (Weapon->Transformed_Vertices, Weapon->Model.Vertices, sizeof (Vertex) * Weapon->Model.Vertex_Count);

  // Create host-visible vertex buffer for direct CPU writes each frame (host-visible so we can update without staging)
  Weapon->Vertex_Buffer = Buffer_Allocate (/*Size         =>*/ sizeof (Vertex) * Weapon->Model.Vertex_Count,
                                           /*Usage        =>*/ VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                                             | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                             | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                                                             | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                           /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                             | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Upload the initial vertex positions
  Buffer_Upload (Weapon->Vertex_Buffer, Weapon->Transformed_Vertices, sizeof (Vertex) * Weapon->Model.Vertex_Count);

  // Upload index and texture-id data (static, device-local — these never change after the initial upload)
  Weapon->Index_Buffer      = Buffer_Stage_Upload (/*Command_Buffer =>*/ Command_Buffer,
                                                   /*Queue          =>*/ Queue,
                                                   /*Data           =>*/ Weapon->Model.Indices,
                                                   /*Size           =>*/ sizeof (uint) * Weapon->Model.Index_Count,
                                                   /*Usage          =>*/ VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                                                       | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                                       | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
  Weapon->Texture_Id_Buffer = Buffer_Stage_Upload (/*Command_Buffer =>*/ Command_Buffer,
                                                   /*Queue          =>*/ Queue,
                                                   /*Data           =>*/ Weapon->Model.Texture_Ids,
                                                   /*Size           =>*/ sizeof (uint) * Weapon->Model.Triangle_Count,
                                                   /*Usage          =>*/ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

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

  // Query required sizes for the weapon BLAS and its scratch buffer from the driver
  uint Primitive_Count = Weapon->Model.Triangle_Count;
  VkAccelerationStructureBuildSizesInfoKHR Build_Sizes = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
  vkGetAccelerationStructureBuildSizes (/*device             =>*/ Device,
                                        /*buildType          =>*/ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                        /*pBuildInfo         =>*/ &Build_Info,
                                        /*pMaxPrimitiveCounts =>*/ &Primitive_Count,
                                        /*pSizeInfo          =>*/ &Build_Sizes);

  // Allocate the BLAS buffer and persistent scratch buffer (scratch is reused across frames for BLAS refits)
  Weapon->Bottom_Level.Buffer  = Buffer_Allocate (/*Size         =>*/ Build_Sizes.accelerationStructureSize,
                                                  /*Usage        =>*/ VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
                                                                    | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                  /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK (vkCreateAccelerationStructure (/*device      =>*/ Device,
                                           /*pCreateInfo =>*/ &(VkAccelerationStructureCreateInfoKHR){
                                             .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                                             .buffer = Weapon->Bottom_Level.Buffer.Buffer,
                                             .size   = Build_Sizes.accelerationStructureSize,
                                             .type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR},
                                           /*pAllocator  =>*/ NULL,
                                           /*pStructure  =>*/ &Weapon->Bottom_Level.Handle));
  Weapon->Bottom_Level_Scratch = Buffer_Allocate (/*Size         =>*/ Build_Sizes.buildScratchSize,
                                                  /*Usage        =>*/ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                                    | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                  /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Perform the initial BLAS build
  Build_Info.dstAccelerationStructure  = Weapon->Bottom_Level.Handle;
  Build_Info.scratchData.deviceAddress = Weapon->Bottom_Level_Scratch.Address;

  // Build range: all weapon triangles in a single geometry entry
  VkAccelerationStructureBuildRangeInfoKHR Range = {.primitiveCount = Primitive_Count};
  const VkAccelerationStructureBuildRangeInfoKHR *Range_Pointer = &Range;

  // Record and submit the initial weapon BLAS build
  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (/*commandBuffer =>*/ Command_Buffer,
                                  /*pBeginInfo    =>*/ &(VkCommandBufferBeginInfo){
                                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));
  vkCmdBuildAccelerationStructures (/*commandBuffer     =>*/ Command_Buffer,
                                    /*infoCount         =>*/ 1,
                                    /*pInfos            =>*/ &Build_Info,
                                    /*ppBuildRangeInfos =>*/ &Range_Pointer);
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));

  // Submit and wait for the build to complete before querying the device address
  VK_CHECK (vkQueueSubmit (/*queue       =>*/ Queue,
                           /*submitCount =>*/ 1,
                           /*pSubmits    =>*/ &(VkSubmitInfo){
                             .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .commandBufferCount = 1,
                             .pCommandBuffers    = &Command_Buffer},
                           /*fence       =>*/ VK_NULL_HANDLE));
  VK_CHECK (vkQueueWaitIdle (Queue));

  // Query the BLAS device address for TLAS instance referencing
  Weapon->Bottom_Level.Address = vkGetAccelerationStructureDeviceAddress (/*device =>*/ Device,
                                   /*pInfo  =>*/ &(VkAccelerationStructureDeviceAddressInfoKHR){
                                     .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                                     .accelerationStructure = Weapon->Bottom_Level.Handle});
  printf ("[weapon] BLAS built: %u triangles\n", Primitive_Count);

} // Weapon_Bottom_Level_Initialize

// ═══════════════════════════════
//   Weapon_Bottom_Level_Rebuild
// ═══════════════════════════════

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

  // BLAS refit (MODE_UPDATE) instead of full rebuild. The weapon mesh topology never changes - only vertex positions move.
  // MODE_UPDATE re-computes AABBs in-place without rebuilding the BVH tree, which is 5-10× faster than a full build on NVIDIA RT cores.
  // srcAccelerationStructure = dst for in-place update.
  VkAccelerationStructureBuildGeometryInfoKHR Build_Info = {
    .sType                     = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type                      = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .flags                     = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR
                               | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
    .mode                      = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR,
    .srcAccelerationStructure  = Weapon->Bottom_Level.Handle,
    .dstAccelerationStructure  = Weapon->Bottom_Level.Handle,
    .scratchData.deviceAddress = Weapon->Bottom_Level_Scratch.Address,
    .geometryCount             = 1,
    .pGeometries               = &Geometry};

  // Build range covering all weapon triangles
  VkAccelerationStructureBuildRangeInfoKHR Range = {.primitiveCount = Weapon->Model.Triangle_Count};
  const VkAccelerationStructureBuildRangeInfoKHR *Range_Pointer = &Range;

  // Record the BLAS refit command into a one-shot command buffer
  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (/*commandBuffer =>*/ Command_Buffer,
                                  /*pBeginInfo    =>*/ &(VkCommandBufferBeginInfo){
                                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));

  // Issue the BLAS refit command (MODE_UPDATE: re-compute AABBs in-place without rebuilding the BVH tree)
  vkCmdBuildAccelerationStructures (/*commandBuffer     =>*/ Command_Buffer,
                                    /*infoCount         =>*/ 1,
                                    /*pInfos            =>*/ &Build_Info,
                                    /*ppBuildRangeInfos =>*/ &Range_Pointer);

  // Submit and synchronize before the frame uses the updated BLAS
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));
  VK_CHECK (vkQueueSubmit (/*queue       =>*/ Queue,
                           /*submitCount =>*/ 1,
                           /*pSubmits    =>*/ &(VkSubmitInfo){
                             .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .commandBufferCount = 1,
                             .pCommandBuffers    = &Command_Buffer},
                           VK_NULL_HANDLE));
  VK_CHECK (vkQueueWaitIdle (Queue));
}

// ══════════════════════════════════
//   Entity_Bottom_Level_Initialize
// ══════════════════════════════════

void Entity_Bottom_Level_Initialize (Entity *Enemy) {
  if (not Enemy->Vertex_Count) return;

  // Host-visible vertex buffer for per-frame CPU uploads (host-visible so we can update each frame without staging)
  Enemy->Vertex_Buffer = Buffer_Allocate (/*Size         =>*/ sizeof (Vertex) * Enemy->Vertex_Count,
                                          /*Usage        =>*/ VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                                            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                                                            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                          /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  Buffer_Upload (Enemy->Vertex_Buffer, Enemy->Current_Vertices, sizeof (Vertex) * Enemy->Vertex_Count);

  // Static index and texture-id buffers (device-local — these never change after the initial upload)
  Enemy->Index_Buffer      = Buffer_Stage_Upload (/*Command_Buffer =>*/ Command_Buffer,
                                                  /*Queue          =>*/ Queue,
                                                  /*Data           =>*/ Enemy->Indices,
                                                  /*Size           =>*/ sizeof (uint) * Enemy->Index_Count,
                                                  /*Usage          =>*/ VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                                                      | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                                      | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
  Enemy->Texture_Id_Buffer = Buffer_Stage_Upload (/*Command_Buffer =>*/ Command_Buffer,
                                                  /*Queue          =>*/ Queue,
                                                  /*Data           =>*/ Enemy->Texture_Ids,
                                                  /*Size           =>*/ sizeof (uint) * Enemy->Triangle_Count,
                                                  /*Usage          =>*/ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

  // Build the initial BLAS with FAST_BUILD + ALLOW_UPDATE for per-frame refit
  VkAccelerationStructureGeometryKHR Geometry = {
    .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
    .flags        = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.triangles = {
      .sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
      .vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT,
      .vertexData.deviceAddress = Enemy->Vertex_Buffer.Address,
      .vertexStride             = sizeof (Vertex),
      .maxVertex                = Enemy->Vertex_Count - 1,
      .indexType                = VK_INDEX_TYPE_UINT32,
      .indexData.deviceAddress  = Enemy->Index_Buffer.Address}};

  // Configure the build for fast construction with per-frame update support
  VkAccelerationStructureBuildGeometryInfoKHR Build_Info = {
    .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR
                   | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
    .geometryCount = 1,
    .pGeometries   = &Geometry};

  // Query required BLAS and scratch buffer sizes from the driver for the given triangle geometry
  uint Primitive_Count = Enemy->Triangle_Count;
  VkAccelerationStructureBuildSizesInfoKHR Build_Sizes = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
  vkGetAccelerationStructureBuildSizes (/*device             =>*/ Device,
                                        /*buildType          =>*/ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                        /*pBuildInfo         =>*/ &Build_Info,
                                        /*pMaxPrimitiveCounts =>*/ &Primitive_Count,
                                        /*pSizeInfo          =>*/ &Build_Sizes);

  // Allocate BLAS storage, create the acceleration structure, and allocate persistent scratch memory for per-frame refits
  Enemy->Bottom_Level.Buffer  = Buffer_Allocate (/*Size         =>*/ Build_Sizes.accelerationStructureSize,
                                                 /*Usage        =>*/ VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
                                                                   | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                 /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK (vkCreateAccelerationStructure (/*device      =>*/ Device,
                                           /*pCreateInfo =>*/ &(VkAccelerationStructureCreateInfoKHR){
                                             .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                                             .buffer = Enemy->Bottom_Level.Buffer.Buffer,
                                             .size   = Build_Sizes.accelerationStructureSize,
                                             .type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR},
                                           /*pAllocator  =>*/ NULL,
                                           /*pStructure  =>*/ &Enemy->Bottom_Level.Handle));
  Enemy->Bottom_Level_Scratch = Buffer_Allocate (/*Size         =>*/ Build_Sizes.buildScratchSize,
                                                 /*Usage        =>*/ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                                   | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                 /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Finalize the build info with destination and scratch addresses
  Build_Info.dstAccelerationStructure  = Enemy->Bottom_Level.Handle;
  Build_Info.scratchData.deviceAddress = Enemy->Bottom_Level_Scratch.Address;

  // Record and submit a one-shot command buffer to build the enemy BLAS
  VkAccelerationStructureBuildRangeInfoKHR Range = {.primitiveCount = Primitive_Count};
  const VkAccelerationStructureBuildRangeInfoKHR *Range_Pointer = &Range;
  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (/*commandBuffer =>*/ Command_Buffer,
                                  /*pBeginInfo    =>*/ &(VkCommandBufferBeginInfo){
                                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));
  vkCmdBuildAccelerationStructures (/*commandBuffer     =>*/ Command_Buffer,
                                    /*infoCount         =>*/ 1,
                                    /*pInfos            =>*/ &Build_Info,
                                    /*ppBuildRangeInfos =>*/ &Range_Pointer);
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));
  VK_CHECK (vkQueueSubmit (/*queue       =>*/ Queue,
                           /*submitCount =>*/ 1,
                           /*pSubmits    =>*/ &(VkSubmitInfo){
                             .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .commandBufferCount = 1,
                             .pCommandBuffers    = &Command_Buffer},
                           /*fence       =>*/ VK_NULL_HANDLE));
  VK_CHECK (vkQueueWaitIdle (Queue));

  // Query the BLAS device address for TLAS instance referencing
  Enemy->Bottom_Level.Address = vkGetAccelerationStructureDeviceAddress (/*device =>*/ Device,
                                  /*pInfo  =>*/ &(VkAccelerationStructureDeviceAddressInfoKHR){
                                    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      .accelerationStructure = Enemy->Bottom_Level.Handle});
  printf ("[enemy] BLAS built: %u triangles\n", Primitive_Count);
}

// ═══════════════════════════════
//   Entity_Bottom_Level_Rebuild
// ═══════════════════════════════

void Entity_Bottom_Level_Rebuild (Entity *Enemy) {
  if (not Enemy->Vertex_Count) return;

  // Re-upload the transformed vertices to the GPU buffer
  Buffer_Upload (Enemy->Vertex_Buffer, Enemy->Current_Vertices, sizeof (Vertex) * Enemy->Vertex_Count);

  // Define the triangle geometry referencing the updated vertex data
  VkAccelerationStructureGeometryKHR Geometry = {
    .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
    .flags        = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.triangles = {
      .sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
      .vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT,
      .vertexData.deviceAddress = Enemy->Vertex_Buffer.Address,
      .vertexStride             = sizeof (Vertex),
      .maxVertex                = Enemy->Vertex_Count - 1,
      .indexType                = VK_INDEX_TYPE_UINT32,
      .indexData.deviceAddress  = Enemy->Index_Buffer.Address}};

  // Configure in-place BLAS refit using the existing structure
  VkAccelerationStructureBuildGeometryInfoKHR Build_Info = {
    .sType                     = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type                      = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .flags                     = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR
                               | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
    .mode                      = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR,
    .srcAccelerationStructure  = Enemy->Bottom_Level.Handle,
    .dstAccelerationStructure  = Enemy->Bottom_Level.Handle,
    .scratchData.deviceAddress = Enemy->Bottom_Level_Scratch.Address,
    .geometryCount             = 1,
    .pGeometries               = &Geometry};

  // Record and submit the BLAS refit command into a one-shot command buffer
  VkAccelerationStructureBuildRangeInfoKHR Range = {.primitiveCount = Enemy->Triangle_Count};
  const VkAccelerationStructureBuildRangeInfoKHR *Range_Pointer = &Range;
  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (/*commandBuffer =>*/ Command_Buffer,
                                  /*pBeginInfo    =>*/ &(VkCommandBufferBeginInfo){
                                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));
  vkCmdBuildAccelerationStructures (/*commandBuffer     =>*/ Command_Buffer,
                                    /*infoCount         =>*/ 1,
                                    /*pInfos            =>*/ &Build_Info,
                                    /*ppBuildRangeInfos =>*/ &Range_Pointer);
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));
  VK_CHECK (vkQueueSubmit (/*queue       =>*/ Queue,
                           /*submitCount =>*/ 1,
                           /*pSubmits    =>*/ &(VkSubmitInfo){
                             .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .commandBufferCount = 1,
                             .pCommandBuffers    = &Command_Buffer},
                           /*fence       =>*/ VK_NULL_HANDLE));
  VK_CHECK (vkQueueWaitIdle (Queue));
}

// ════════════════════════
//   Top_Level_Initialize
// ════════════════════════

void Top_Level_Initialize (uint Maximum_Instances) {

  // Allocate a host-visible instance buffer for writing TLAS instance descriptors each frame (host-visible for direct CPU writes)
  Top_Level_Instance_Buffer = Buffer_Allocate (/*Size         =>*/ sizeof (VkAccelerationStructureInstanceKHR) * Maximum_Instances,
                                               /*Usage        =>*/ VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                                                                 | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                               /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
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

  // Query TLAS build sizes from the driver. ALLOW_UPDATE_BIT: requesting update capability at creation time tells the driver to build
  // the BVH in a format amenable to in-place refitting. On NVIDIA, this selects a "refit-friendly" BVH2 layout; on AMD, it avoids the
  // compact PLOC builder. Per-frame, we then use MODE_UPDATE (refit) instead of full MODE_BUILD, which only touches BVH nodes whose
  // child AABBs changed - typically O(log N) instead of O(N log N) for a full SAH rebuild.
  VkAccelerationStructureBuildGeometryInfoKHR Build_Info = {
    .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR
                   | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
    .geometryCount = 1,
    .pGeometries   = &Geometry};
  VkAccelerationStructureBuildSizesInfoKHR Build_Sizes = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
  vkGetAccelerationStructureBuildSizes (/*device             =>*/ Device,
                                        /*buildType          =>*/ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                        /*pBuildInfo         =>*/ &Build_Info,
                                        /*pMaxPrimitiveCounts =>*/ &Maximum_Instances,
                                        /*pSizeInfo          =>*/ &Build_Sizes);

  // Allocate the TLAS storage buffer and create the top-level acceleration structure object
  Top_Level.Buffer = Buffer_Allocate (/*Size         =>*/ Build_Sizes.accelerationStructureSize,
                                      /*Usage        =>*/ VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
                                                        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                      /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Create the top-level acceleration structure object backed by the allocated buffer
  VK_CHECK (vkCreateAccelerationStructure (/*device      =>*/ Device,
                                           /*pCreateInfo =>*/ &(VkAccelerationStructureCreateInfoKHR){
                                             .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                                             .buffer = Top_Level.Buffer.Buffer,
                                             .size   = Build_Sizes.accelerationStructureSize,
                                             .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR},
                                           /*pAllocator  =>*/ NULL,
                                           /*pStructure  =>*/ &Top_Level.Handle));

  // Allocate persistent scratch memory for per-frame TLAS rebuilds (reused every frame)
  Top_Level_Scratch_Buffer = Buffer_Allocate (/*Size         =>*/ Build_Sizes.buildScratchSize,
                                              /*Usage        =>*/ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                                | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                              /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Query the TLAS device address for descriptor binding in the ray tracing pipeline
  Top_Level.Address = vkGetAccelerationStructureDeviceAddress (/*device =>*/ Device,
                        /*pInfo  =>*/ &(VkAccelerationStructureDeviceAddressInfoKHR){
                          .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                          .accelerationStructure = Top_Level.Handle});

} // Top_Level_Initialize

// ═════════════════════
//   Top_Level_Rebuild
// ═════════════════════

void Top_Level_Rebuild (Acceleration_Structure *World, Acceleration_Structure *Weapon, Acceleration_Structure *Enemy,
                        const float *Player_Body_Transform) {

  // Zero-initialize the instance descriptors
  VkAccelerationStructureInstanceKHR Instances[4];
  memset (Instances, 0, sizeof (Instances));

  // Instance 0: the world geometry with identity transform, visible to all rays
  Instances[0].transform.matrix[0][0]         = 1.f;
  Instances[0].transform.matrix[1][1]         = 1.f;
  Instances[0].transform.matrix[2][2]         = 1.f;
  Instances[0].mask                           = 0xFF;
  Instances[0].instanceCustomIndex            = 0;
  Instances[0].flags                          = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
  Instances[0].accelerationStructureReference = World->Address;

  // Track the number of active TLAS instances
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

  // Instance 2 (optional): entity (animated character), visible to all rays (casts shadows)
  if (Enemy and Enemy->Handle) {
    Instances[2].transform.matrix[0][0]         = 1.f;
    Instances[2].transform.matrix[1][1]         = 1.f;
    Instances[2].transform.matrix[2][2]         = 1.f;
    Instances[2].mask                           = 0xFF;
    Instances[2].instanceCustomIndex            = 2;
    Instances[2].flags                          = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    Instances[2].accelerationStructureReference = Enemy->Address;
    Instance_Count = 3;

    // Instance 3 (optional): player body - same BLAS as entity, repositioned at the player's location
    if (Player_Body_Transform) {
      memcpy (&Instances[3].transform, Player_Body_Transform, sizeof (float) * 12);
      Instances[3].mask                           = 0x02;
      Instances[3].instanceCustomIndex            = 2;  // Same as entity - shares entity buffers
      Instances[3].flags                          = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
      Instances[3].accelerationStructureReference = Enemy->Address;  // Reuse the enemy BLAS
      Instance_Count = 4;
    }
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

  // TLAS refit instead of full rebuild
  int First_Build = 1;
  VkAccelerationStructureBuildGeometryInfoKHR Build_Info = {
    .sType                     = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type                      = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    .flags                     = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR
                               | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
    .mode                      = First_Build ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR
                                             : VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR,
    .srcAccelerationStructure  = First_Build ? VK_NULL_HANDLE : Top_Level.Handle,
    .dstAccelerationStructure  = Top_Level.Handle,
    .scratchData.deviceAddress = Top_Level_Scratch_Buffer.Address,
    .geometryCount             = 1,
    .pGeometries               = &Geometry};
  First_Build = 0;

  // Record and submit the TLAS rebuild command into a one-shot command buffer
  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (/*commandBuffer =>*/ Command_Buffer,
                                  /*pBeginInfo    =>*/ &(VkCommandBufferBeginInfo){
                                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));
  vkCmdBuildAccelerationStructures (/*commandBuffer     =>*/ Command_Buffer,
                                    /*infoCount         =>*/ 1,
                                    /*pInfos            =>*/ &Build_Info,
                                    /*ppBuildRangeInfos =>*/ &Range_Pointer);
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));

  // Submit and wait for the TLAS rebuild to complete before the frame uses it
  VK_CHECK (vkQueueSubmit (/*queue       =>*/ Queue,
                           /*submitCount =>*/ 1,
                           /*pSubmits    =>*/ &(VkSubmitInfo){
                             .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .commandBufferCount = 1,
                             .pCommandBuffers    = &Command_Buffer},
                           /*fence       =>*/ VK_NULL_HANDLE));
  VK_CHECK (vkQueueWaitIdle (Queue));

} // Top_Level_Rebuild

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §12. Pipeline
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════
//   Postprocess_Pipeline_Create
// ═══════════════════════════════

void Postprocess_Pipeline_Create () {

  // Storage image bindings: color (raw RT), depth, history (TAA), display output
  VkDescriptorSetLayoutBinding Bindings[] = {
    {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // Color (raw RT, read-only)
    {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // Depth
    {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // History (previous frame TAA)
    {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // Display output (tonemapped)
  };

  VK_CHECK (vkCreateDescriptorSetLayout (/*device      =>*/ Device,
                                         /*pCreateInfo =>*/ &(VkDescriptorSetLayoutCreateInfo){
                                           .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                           .bindingCount = 4,
                                           .pBindings    = Bindings},
                                         /*pAllocator  =>*/ NULL,
                                         /*pSetLayout  =>*/ &Postprocess_Descriptor_Layout));

  // Create the pipeline layout with push constants for postprocess parameters
  VkPushConstantRange Push_Range = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof (Gpu_Postprocess_Push)};
  VK_CHECK (vkCreatePipelineLayout (/*device          =>*/ Device,
                                    /*pCreateInfo     =>*/ &(VkPipelineLayoutCreateInfo){
                                      .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                      .setLayoutCount         = 1,
                                      .pSetLayouts            = &Postprocess_Descriptor_Layout,
                                      .pushConstantRangeCount = 1,
                                      .pPushConstantRanges    = &Push_Range},
                                    /*pAllocator      =>*/ NULL,
                                    /*pPipelineLayout =>*/ &Postprocess_Pipeline_Layout));

  // Load the postprocess shader and create the compute pipeline
  VkShaderModule Module = Shader_Module_Load (Shader_Path(Post_Process));
  VK_CHECK (vkCreateComputePipelines (/*device          =>*/ Device,
                                      /*pipelineCache   =>*/ Pipeline_Cache,
                                      /*createInfoCount =>*/ 1,
                                      /*pCreateInfos    =>*/ &(VkComputePipelineCreateInfo){
                                        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                        .stage  = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0,
                                                   VK_SHADER_STAGE_COMPUTE_BIT, Module, "main", NULL},
                                        .layout = Postprocess_Pipeline_Layout},
                                      /*pAllocator      =>*/ NULL,
                                      /*pPipelines      =>*/ &Postprocess_Pipeline));
  vkDestroyShaderModule (Device, Module, NULL);

  // Descriptor pool and set
  VkDescriptorPoolSize Pool_Size = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4};
  VK_CHECK (vkCreateDescriptorPool (/*device          =>*/ Device,
                                    /*pCreateInfo     =>*/ &(VkDescriptorPoolCreateInfo){
                                      .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                      .maxSets       = 1,
                                      .poolSizeCount = 1,
                                      .pPoolSizes    = &Pool_Size},
                                    /*pAllocator      =>*/ NULL,
                                    /*pDescriptorPool =>*/ &Postprocess_Descriptor_Pool));

  VK_CHECK (vkAllocateDescriptorSets (/*device          =>*/ Device,
                                      /*pAllocateInfo   =>*/ &(VkDescriptorSetAllocateInfo){
                                        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                        .descriptorPool     = Postprocess_Descriptor_Pool,
                                        .descriptorSetCount = 1,
                                        .pSetLayouts        = &Postprocess_Descriptor_Layout},
                                      /*pDescriptorSets =>*/ &Postprocess_Descriptor_Set));

  // Prepare image descriptor infos for color, depth, history, and display output
  VkDescriptorImageInfo Color_Info   = {.imageView = Raytracing_Storage_Image.View,  .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorImageInfo Depth_Info   = {.imageView = Depth_Image.View,               .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorImageInfo History_Info = {.imageView = History_Image.View,              .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorImageInfo Display_Info = {.imageView = Postprocess_Output_Image.View,  .imageLayout = VK_IMAGE_LAYOUT_GENERAL};

  VkWriteDescriptorSet Writes[] = {
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Postprocess_Descriptor_Set, 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &Color_Info,   NULL, NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Postprocess_Descriptor_Set, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &Depth_Info,   NULL, NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Postprocess_Descriptor_Set, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &History_Info, NULL, NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Postprocess_Descriptor_Set, 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &Display_Info, NULL, NULL},
  };
  vkUpdateDescriptorSets (Device, 4, Writes, 0, NULL);

} // Postprocess_Pipeline_Create

// ═══════════════════════════
//   Denoise_Pipeline_Create
// ═══════════════════════════

void Denoise_Pipeline_Create () {

  // Three bindings: Input (read), Output (write), Depth (read)
  VkDescriptorSetLayoutBinding Bindings[] = {
    {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},  // Input color
    {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},  // Output color
    {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},  // Depth
  };

  VK_CHECK (vkCreateDescriptorSetLayout (/*device      =>*/ Device,
                                         /*pCreateInfo =>*/ &(VkDescriptorSetLayoutCreateInfo){
                                           .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                           .bindingCount = 3,
                                           .pBindings    = Bindings},
                                         /*pAllocator  =>*/ NULL,
                                         /*pSetLayout  =>*/ &Denoise_Descriptor_Layout));

  // Create the pipeline layout with push constants for step size and budget
  VkPushConstantRange Push_Range = {VK_SHADER_STAGE_COMPUTE_BIT, 0, 2 * sizeof (int)};
  VK_CHECK (vkCreatePipelineLayout (/*device          =>*/ Device,
                                    /*pCreateInfo     =>*/ &(VkPipelineLayoutCreateInfo){
                                      .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                      .setLayoutCount         = 1,
                                      .pSetLayouts            = &Denoise_Descriptor_Layout,
                                      .pushConstantRangeCount = 1,
                                      .pPushConstantRanges    = &Push_Range},
                                    /*pAllocator      =>*/ NULL,
                                    /*pPipelineLayout =>*/ &Denoise_Pipeline_Layout));

  // Load the denoise shader and create the compute pipeline
  VkShaderModule Module = Shader_Module_Load (Shader_Path(Denoise));
  VK_CHECK (vkCreateComputePipelines (/*device          =>*/ Device,
                                      /*pipelineCache   =>*/ Pipeline_Cache,
                                      /*createInfoCount =>*/ 1,
                                      /*pCreateInfos    =>*/ &(VkComputePipelineCreateInfo){
                                        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                        .stage  = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0,
                                                   VK_SHADER_STAGE_COMPUTE_BIT, Module, "main", NULL},
                                        .layout = Denoise_Pipeline_Layout},
                                      /*pAllocator      =>*/ NULL,
                                      /*pPipelines      =>*/ &Denoise_Pipeline));
  vkDestroyShaderModule (Device, Module, NULL);

  // Descriptor pool: 2 sets × 3 images each = 6 image descriptors
  VkDescriptorPoolSize Pool_Size = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 6};
  VK_CHECK (vkCreateDescriptorPool (/*device          =>*/ Device,
                                    /*pCreateInfo     =>*/ &(VkDescriptorPoolCreateInfo){
                                      .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                      .maxSets       = 2,
                                      .poolSizeCount = 1,
                                      .pPoolSizes    = &Pool_Size},
                                    /*pAllocator      =>*/ NULL,
                                    /*pDescriptorPool =>*/ &Denoise_Descriptor_Pool));

  // Allocate both ping-pong descriptor sets
  VkDescriptorSetLayout Layouts[2] = {Denoise_Descriptor_Layout, Denoise_Descriptor_Layout};
  VK_CHECK (vkAllocateDescriptorSets (/*device          =>*/ Device,
                                      /*pAllocateInfo   =>*/ &(VkDescriptorSetAllocateInfo){
                                        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                        .descriptorPool     = Denoise_Descriptor_Pool,
                                        .descriptorSetCount = 2,
                                        .pSetLayouts        = Layouts},
                                      /*pDescriptorSets =>*/ Denoise_Descriptor_Sets));

  // Set[0]: reads Storage_Image > writes Denoise_Ping_Image
  VkDescriptorImageInfo Storage_Info = {.imageView = Raytracing_Storage_Image.View, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorImageInfo Ping_Info    = {.imageView = Denoise_Ping_Image.View,       .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorImageInfo Depth_Info   = {.imageView = Depth_Image.View,              .imageLayout = VK_IMAGE_LAYOUT_GENERAL};

  // Write descriptor set 0: Storage > Ping
  VkWriteDescriptorSet Writes_0[] = {
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Denoise_Descriptor_Sets[0], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &Storage_Info, NULL, NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Denoise_Descriptor_Sets[0], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &Ping_Info,    NULL, NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Denoise_Descriptor_Sets[0], 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &Depth_Info,   NULL, NULL},
  };
  vkUpdateDescriptorSets (Device, 3, Writes_0, 0, NULL);

  // Set[1]: reads Denoise_Ping_Image > writes Storage_Image
  VkWriteDescriptorSet Writes_1[] = {
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Denoise_Descriptor_Sets[1], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &Ping_Info,    NULL, NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Denoise_Descriptor_Sets[1], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &Storage_Info, NULL, NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Denoise_Descriptor_Sets[1], 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &Depth_Info,   NULL, NULL},
  };
  vkUpdateDescriptorSets (Device, 3, Writes_1, 0, NULL);

} // Denoise_Pipeline_Create

// ══════════════════════════════
//   Raytracing_Pipeline_Create
// ══════════════════════════════

void Raytracing_Pipeline_Create () {

  // Define the 16 descriptor bindings for the ray tracing pipeline
  // Bindings 12-14 are entity geometry; binding 15 is the texture array (must be last for variable count)
  VkDescriptorSetLayoutBinding Bindings[] = {
    {0,  VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR
                                                         | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {1,  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,      NULL},
    {2,  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR
                                                         | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
                                                         | VK_SHADER_STAGE_MISS_BIT_KHR,        NULL},
    {3,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {4,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {5,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {6,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {7,  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {8,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {9,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {11, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,      NULL}, // Depth output
    {12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL}, // Entity vertices
    {13, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL}, // Entity indices
    {14, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL}, // Entity texture IDs
    {15, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DESCRIPTOR_TEXTURE_SLOTS, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    // Textures: must be last binding for variable descriptor count
  };

  // The texture array binding uses partially-bound and variable-count flags
  VkDescriptorBindingFlags Binding_Flags[] =
    {0, 0, 0, 0, 0,
     0, 0, 0, 0, 0,
     0, 0, 0, 0, 0,
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};

  // Chain the binding flags extension into the descriptor set layout creation
  VkDescriptorSetLayoutBindingFlagsCreateInfo Binding_Flags_Info = {
    .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
    .bindingCount  = 16,
    .pBindingFlags = Binding_Flags};

  // Create the descriptor set layout with all 16 bindings
  VK_CHECK (vkCreateDescriptorSetLayout (/*device      =>*/ Device,
                                         /*pCreateInfo =>*/ &(VkDescriptorSetLayoutCreateInfo){
                                           .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                           .pNext        = &Binding_Flags_Info,
                                           .bindingCount = 16,
                                           .pBindings    = Bindings},
                                         /*pAllocator  =>*/ NULL,
                                         /*pSetLayout  =>*/ &Descriptor_Set_Layout));

  // Create the pipeline layout referencing the single descriptor set
  VK_CHECK (vkCreatePipelineLayout (/*device          =>*/ Device,
                                    /*pCreateInfo     =>*/ &(VkPipelineLayoutCreateInfo){
                                      .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                      .setLayoutCount = 1,
                                      .pSetLayouts    = &Descriptor_Set_Layout},
                                    /*pAllocator      =>*/ NULL,
                                    /*pPipelineLayout =>*/ &Pipeline_Layout));

  // Load the four SPIR-V shader modules from pre-compiled files
  VkShaderModule Ray_Generation_Module = Shader_Module_Load (Shader_Path (Ray_Generation));
  VkShaderModule Closest_Hit_Module    = Shader_Module_Load (Shader_Path (Closest_Hit));
  VkShaderModule Primary_Miss_Module   = Shader_Module_Load (Shader_Path (Ray_Miss));
  VkShaderModule Shadow_Miss_Module    = Shader_Module_Load (Shader_Path (Shadow_Miss));

  // Define the pipeline shader stages
  VkPipelineShaderStageCreateInfo Stages[] = {
    {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_RAYGEN_BIT_KHR,      Ray_Generation_Module, "main", NULL},
    {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_MISS_BIT_KHR,        Primary_Miss_Module,   "main", NULL},
    {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_MISS_BIT_KHR,        Shadow_Miss_Module,    "main", NULL},
    {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, Closest_Hit_Module,    "main", NULL}};

  // Define the shader groups
  VkRayTracingShaderGroupCreateInfoKHR Groups[] = {
    {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, NULL, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, NULL},
    {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, NULL, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, NULL},
    {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, NULL, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 2, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, NULL},
    {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, NULL, VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 3, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, NULL}};

  // A shared pipeline cache lets the driver reuse compiled shader ISA across pipeline objects and across runs
  VK_CHECK (vkCreatePipelineCache (/*device      =>*/ Device,
                                   /*pCreateInfo =>*/ &(VkPipelineCacheCreateInfo){
                                     .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO},
                                   /*pAllocator  =>*/ NULL,
                                   /*pPipelineCache =>*/ &Pipeline_Cache));

  // Create the ray tracing pipeline with recursion depth
  VK_CHECK (vkCreateRayTracingPipelines (/*device            =>*/ Device,
                                         /*deferredOperation =>*/ VK_NULL_HANDLE,
                                         /*pipelineCache     =>*/ Pipeline_Cache,
                                         /*createInfoCount   =>*/ 1,
                                         /*pCreateInfos      =>*/ &(VkRayTracingPipelineCreateInfoKHR){
                                           .sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
                                           .stageCount                   = 4,
                                           .pStages                      = Stages,
                                           .groupCount                   = 4,
                                           .pGroups                      = Groups,
                                           .maxPipelineRayRecursionDepth = 2,
                                           .layout                       = Pipeline_Layout},
                                         /*pAllocator        =>*/ NULL,
                                         /*pPipelines        =>*/ &Pipeline));

  // Destroy the shader modules now that the pipeline owns the compiled code
  vkDestroyShaderModule (Device, Ray_Generation_Module, NULL);
  vkDestroyShaderModule (Device, Closest_Hit_Module,    NULL);
  vkDestroyShaderModule (Device, Primary_Miss_Module,   NULL);
  vkDestroyShaderModule (Device, Shadow_Miss_Module,    NULL);
}

// ═══════════════════════════════
//   Shader_Binding_Table_Create
// ═══════════════════════════════

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
  Shader_Binding_Table_Buffer = Buffer_Allocate (/*Size         =>*/ Table_Size,
                                                 /*Usage        =>*/ VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR
                                                                   | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                 /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
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
  Shader_Binding_Callable       = (VkStridedDeviceAddressRegionKHR){0, 0, 0};

} // Shader_Binding_Table_Create

// ═════════════════════════
//   Descriptor_Set_Create
// ═════════════════════════

void Descriptor_Set_Create (Weapon_Instance *Weapon, Entity *Enemy) {

  // Allocate a descriptor pool large enough for all binding types
  VkDescriptorPoolSize Pool_Sizes[] = {{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
                                       {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              2},  // Color + Depth
                                       {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1},
                                       {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             10}, // 7 world/weapon + 3 entity
                                       {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     DESCRIPTOR_TEXTURE_SLOTS + 1}}; // +1 for lightmap
  VK_CHECK (vkCreateDescriptorPool (/*device          =>*/ Device,
                                    /*pCreateInfo     =>*/ &(VkDescriptorPoolCreateInfo){
                                      .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                      .maxSets       = 1,
                                      .poolSizeCount = 5,
                                      .pPoolSizes    = Pool_Sizes},
                                    /*pAllocator      =>*/ NULL,
                                    /*pDescriptorPool =>*/ &Descriptor_Pool));

  // Allocate the descriptor set with a variable descriptor count for the texture array
  uint Variable_Count = Texture_Count;
  VkDescriptorSetVariableDescriptorCountAllocateInfo Variable_Allocate = {
    .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
    .descriptorSetCount = 1,
    .pDescriptorCounts  = &Variable_Count};
  VK_CHECK (vkAllocateDescriptorSets (/*device          =>*/ Device,
                                      /*pAllocateInfo   =>*/ &(VkDescriptorSetAllocateInfo){
                                        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                        .pNext              = &Variable_Allocate,
                                        .descriptorPool     = Descriptor_Pool,
                                        .descriptorSetCount = 1,
                                        .pSetLayouts        = &Descriptor_Set_Layout},
                                      /*pDescriptorSets =>*/ &Descriptor_Set));

  // Prepare descriptor info structures for each binding
  VkWriteDescriptorSetAccelerationStructureKHR Acceleration_Write = {
    .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
    .accelerationStructureCount = 1,
    .pAccelerationStructures    = &Top_Level.Handle};
  VkDescriptorImageInfo  Image_Info             = {.imageView = Raytracing_Storage_Image.View, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorBufferInfo Camera_Info            = {Camera_Uniform_Buffer.Buffer,     0, Camera_Uniform_Buffer.Size};
  VkDescriptorBufferInfo Vertex_Info            = {Vertex_Buffer.Buffer,             0, Vertex_Buffer.Size};
  VkDescriptorBufferInfo Index_Info             = {Index_Buffer.Buffer,              0, Index_Buffer.Size};
  VkDescriptorBufferInfo Material_Info          = {Material_Buffer.Buffer,           0, Material_Buffer.Size};
  VkDescriptorBufferInfo Texture_Id_Info        = {Texture_Id_Buffer.Buffer,         0, Texture_Id_Buffer.Size};
  VkDescriptorBufferInfo Weapon_Vertex_Info     = {Weapon->Vertex_Buffer.Buffer,     0, Weapon->Vertex_Buffer.Size};
  VkDescriptorBufferInfo Weapon_Index_Info      = {Weapon->Index_Buffer.Buffer,      0, Weapon->Index_Buffer.Size};
  VkDescriptorBufferInfo Weapon_Texture_Id_Info = {Weapon->Texture_Id_Buffer.Buffer, 0, Weapon->Texture_Id_Buffer.Size};
  VkDescriptorBufferInfo Entity_Vertex_Info     = {Enemy->Vertex_Buffer.Buffer,      0, Enemy->Vertex_Buffer.Size};
  VkDescriptorBufferInfo Entity_Index_Info      = {Enemy->Index_Buffer.Buffer,       0, Enemy->Index_Buffer.Size};
  VkDescriptorBufferInfo Entity_Texture_Id_Info = {Enemy->Texture_Id_Buffer.Buffer,  0, Enemy->Texture_Id_Buffer.Size};

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

  // Depth output image descriptor
  VkDescriptorImageInfo Depth_Info = {.imageView = Depth_Image.View, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};

  // Write all 16 descriptor bindings in one batch
  VkWriteDescriptorSet Writes[] = {
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, &Acceleration_Write, Descriptor_Set, 0,  0, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, NULL,           NULL,                    NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 1,  0, 1,                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              &Image_Info,    NULL,                    NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 2,  0, 1,                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             NULL,           &Camera_Info,            NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 3,  0, 1,                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Vertex_Info,            NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 4,  0, 1,                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Index_Info,             NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 5,  0, 1,                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Material_Info,          NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 6,  0, 1,                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Texture_Id_Info,        NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 7,  0, 1,                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     &Lightmap_Info, NULL,                    NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 8,  0, 1,                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Weapon_Vertex_Info,     NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 9,  0, 1,                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Weapon_Index_Info,      NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 10, 0, 1,                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Weapon_Texture_Id_Info, NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 11, 0, 1,                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              &Depth_Info,    NULL,                    NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 12, 0, 1,                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Entity_Vertex_Info,     NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 13, 0, 1,                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Entity_Index_Info,      NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 14, 0, 1,                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Entity_Texture_Id_Info, NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 15, 0, Texture_Count,    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     Texture_Infos,  NULL,                    NULL},
  };

  // Update Vulkan state
  vkUpdateDescriptorSets (Device, 16, Writes, 0, NULL);
  free (Texture_Infos);

} // Descriptor_Set_Create

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §14. Render
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ═════════════════
//   Camera_Upload
// ═════════════════

void Camera_Upload (Camera *State, float Field_Of_View, uint Weapon_Texture_Base, uint PBR_Stride_Value, uint Active_SPP) {

  // Build the view and projection matrices from the camera state
  mat4 View_Matrix = View (State->Position, State->Yaw, State->Pitch);
  mat4 Proj_Matrix = Perspective (Field_Of_View, (float)Width / Height, 0.1f, 10000.f);

  // Uniform layout matching the GPU Camera_Uniform block
  struct __attribute__((aligned(16))) {
    mat4  Inverse_View, Inverse_Projection;
    uint  Frame;
    uint  Weapon_Texture_Base;
    uint  PBR_Stride;
    uint  Active_SPP;

    // Environment parameters (std140 aligned)
    float Sun_Dir      [4]; // xyz = direction, w = angular_radius
    float Sun_Color    [4]; // xyz = color, w = intensity
    float Sky_Zenith   [4]; // xyz = zenith color, w = sky_intensity
    float Sky_Horizon  [4]; // xyz = horizon color, w = sun_disc_size
    float Ambient_Up   [4]; // xyz = ambient up, w = sun_disc_intensity
    float Ambient_Down [4]; // xyz = ambient down, w = fog_density
    float Fog_Color    [4]; // xyz = fog color, w = unused
  } Uniform;

  // Compute the inverse matrices for reconstructing world-space rays from screen coordinates
  Uniform.Inverse_View        = Inverse_Orthogonal (View_Matrix);
  Uniform.Inverse_Projection  = Inverse_Projection (Proj_Matrix);
  Uniform.Frame               = State->Frame;
  Uniform.Weapon_Texture_Base = Weapon_Texture_Base;
  Uniform.PBR_Stride          = PBR_Stride_Value;
  Uniform.Active_SPP          = Active_SPP;

  // Pack environment parameters into the uniform (vec4-aligned for std140)
  Scene_Environment *E = &Active_Environment;
  vec3 Nd = Normalize (E->Sun_Direction);
  Uniform.Sun_Dir     [3] = E->Sun_Angular_Radius;
  Uniform.Sun_Dir     [0] = Nd.x; Uniform.Sun_Dir[1] = Nd.y; Uniform.Sun_Dir[2] = Nd.z;
  Uniform.Sun_Color   [0] = E->Sun_Color.x;   Uniform.Sun_Color   [1] = E->Sun_Color.y;
  Uniform.Sun_Color   [2] = E->Sun_Color.z;   Uniform.Sun_Color   [3] = E->Sun_Intensity;
  Uniform.Sky_Zenith  [0] = E->Sky_Zenith.x;  Uniform.Sky_Zenith  [1] = E->Sky_Zenith.y;
  Uniform.Sky_Zenith  [2] = E->Sky_Zenith.z;  Uniform.Sky_Zenith  [3] = E->Sky_Intensity;
  Uniform.Sky_Horizon [0] = E->Sky_Horizon.x; Uniform.Sky_Horizon [1] = E->Sky_Horizon.y;

  // Pre-compute cos(Sun_Disc_Size) on CPU - eliminates cos() per miss invocation
  Uniform.Sky_Horizon  [2] = E->Sky_Horizon.z;  Uniform.Sky_Horizon  [3] = cosf (E->Sun_Disc_Size);
  Uniform.Ambient_Up   [0] = E->Ambient_Up.x;   Uniform.Ambient_Up   [1] = E->Ambient_Up.y;
  Uniform.Ambient_Up   [2] = E->Ambient_Up.z;   Uniform.Ambient_Up   [3] = E->Sun_Disc_Intensity;
  Uniform.Ambient_Down [0] = E->Ambient_Down.x; Uniform.Ambient_Down [1] = E->Ambient_Down.y;
  Uniform.Ambient_Down [2] = E->Ambient_Down.z; Uniform.Ambient_Down [3] = E->Fog_Density;
  Uniform.Fog_Color    [0] = E->Fog_Color.x;    Uniform.Fog_Color    [1] = E->Fog_Color.y;
  Uniform.Fog_Color    [2] = E->Fog_Color.z;    Uniform.Fog_Color    [3] = 0;

  // Upload the uniform data to the camera buffer
  Buffer_Upload (Camera_Uniform_Buffer, &Uniform, sizeof (Uniform));
  
} // Camera_Upload

// ═════════════════
//   Weapon_Update
// ═════════════════

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

  // Build the camera's forward, right, and up vectors
  vec3 Forward = Make      (Sine_Yaw * Cosine_Pitch, -Sine_Pitch, -Cosine_Yaw * Cosine_Pitch);
  vec3 Right   = Normalize (Cross (Forward, Make (0, 1, 0)));
  vec3 Up      = Cross     (Right, Forward);

  // Compute the viewmodel offset with idle bob and recoil animations
  float Bob_Vertical   = sinf (Weapon->Bob_Time * 3.5f) * 0.08f;
  float Bob_Horizontal = cosf (Weapon->Bob_Time * 1.7f) * 0.04f;
  float Recoil         = Weapon->Is_Firing ? -0.5f * expf (-Weapon->Fire_Time * 5.f) : 0.f;

  // Final weapon position: camera origin + forward/right/up offsets with bob and recoil.
  // Place weapon grip near camera (almost behind it) so only the barrel extends forward.
  // Low forward offset keeps grip at the camera; barrel naturally extends into the scene.
  vec3 Offset = Add (Camera_Data->Position,
                     Add (Scale (Forward, 2.f + Recoil),
                          Add (Scale (Right, 3.5f + Bob_Horizontal),
                               Scale (Up,   -3.f + Bob_Vertical))));

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

  // Scale the viewmodel down - no depth hack, so we shrink the model in world space
  float Model_Scale = 0.45f;

  // Transform each vertex from model space (Q3 Z-up) to world space (Y-up).
  // Swizzle Q3 coords (X,Y,Z) > Y-up (X,Z,-Y) so barrel>Forward, up>Up, right>Right.
  for (uint Index = 0; Index < Weapon->Model.Vertex_Count; Index++) {
    float Source_X =  Weapon->Model.Vertices[Index].Position[0] * Model_Scale; // Q3 X (forward/barrel)
    float Source_Y =  Weapon->Model.Vertices[Index].Position[2] * Model_Scale; // Q3 Z (up)
    float Source_Z = -Weapon->Model.Vertices[Index].Position[1] * Model_Scale; // Q3 -Y (right)

    // Apply the combined rotation and translate by the camera offset
    Weapon->Transformed_Vertices[Index].Position[0] = Rotation[0] * Source_X + Rotation[1] * Source_Y + Rotation[2] * Source_Z + Offset.x;
    Weapon->Transformed_Vertices[Index].Position[1] = Rotation[3] * Source_X + Rotation[4] * Source_Y + Rotation[5] * Source_Z + Offset.y;
    Weapon->Transformed_Vertices[Index].Position[2] = Rotation[6] * Source_X + Rotation[7] * Source_Y + Rotation[8] * Source_Z + Offset.z;

    // Rotate the vertex normal by the same swizzle + rotation (no translation)
    float Normal_X =  Weapon->Model.Vertices[Index].Normal[0];
    float Normal_Y =  Weapon->Model.Vertices[Index].Normal[2];
    float Normal_Z = -Weapon->Model.Vertices[Index].Normal[1];
    Weapon->Transformed_Vertices[Index].Normal[0] = Rotation[0] * Normal_X + Rotation[1] * Normal_Y + Rotation[2] * Normal_Z;
    Weapon->Transformed_Vertices[Index].Normal[1] = Rotation[3] * Normal_X + Rotation[4] * Normal_Y + Rotation[5] * Normal_Z;
    Weapon->Transformed_Vertices[Index].Normal[2] = Rotation[6] * Normal_X + Rotation[7] * Normal_Y + Rotation[8] * Normal_Z;

    // Pass texture coordinates through unchanged
    Weapon->Transformed_Vertices[Index].Texture_Uv[0] = Weapon->Model.Vertices[Index].Texture_Uv[0];
    Weapon->Transformed_Vertices[Index].Texture_Uv[1] = Weapon->Model.Vertices[Index].Texture_Uv[1];
  }
} // Weapon_Update

// ════════════════════
//   Raytracing_Frame
// ════════════════════

void Raytracing_Frame (Gpu_Postprocess_Push Postprocess) {

  // Wait for the previous frame's GPU work to complete
  VK_CHECK (vkWaitForFences (Device, 1, &Fence, VK_TRUE, UINT64_MAX));

  // Acquire the next swapchain image (handle OUT_OF_DATE from window resize)
  uint Image_Index;
  { VkResult R = vkAcquireNextImageKHR (/*device      =>*/ Device,
                                        /*swapchain   =>*/ Swapchain,
                                        /*timeout     =>*/ UINT64_MAX,
                                        /*semaphore   =>*/ Semaphore_Image_Available,
                                        /*fence       =>*/ VK_NULL_HANDLE,
                                        /*pImageIndex =>*/ &Image_Index);
    if (R == VK_ERROR_OUT_OF_DATE_KHR) { Swapchain_Dirty = 1; return; }
    if (R != VK_SUCCESS and R != VK_SUBOPTIMAL_KHR) {
      fprintf (stderr, "[vulkan] acquire error %d at %s:%d\n", R, __FILE__, __LINE__); exit (1);
    }
  }

  // Reset the fence and begin recording the frame's command buffer
  VK_CHECK (vkResetFences (Device, 1, &Fence));
  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (/*commandBuffer =>*/ Command_Buffer,
                                  /*pBeginInfo    =>*/ &(VkCommandBufferBeginInfo){
                                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}));

  // Bind the ray tracing pipeline and descriptor set
  vkCmdBindPipeline       (Command_Buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, Pipeline);
  vkCmdBindDescriptorSets (/*commandBuffer      =>*/ Command_Buffer,
                           /*pipelineBindPoint   =>*/ VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                           /*layout              =>*/ Pipeline_Layout,
                           /*firstSet            =>*/ 0,
                           /*descriptorSetCount  =>*/ 1,
                           /*pDescriptorSets     =>*/ &Descriptor_Set,
                           /*dynamicOffsetCount  =>*/ 0,
                           /*pDynamicOffsets     =>*/ NULL);

  // Checkerboard: dispatch at half width, each thread remaps to a
  // checkerboard pixel. Untouched pixels keep their previous value; the postprocess
  // reconstructs them from traced neighbors before TAA. On lavapipe (CPU) this halves actual thread count, saving ~35% frame time.
  int RT_Dispatch_W = Active_Checkerboard ? (Render_Width + 1) / 2 : Render_Width;
  vkCmdTraceRays (/*commandBuffer                  =>*/ Command_Buffer,
                  /*pRaygenShaderBindingTable      =>*/ &Shader_Binding_Ray_Generation,
                  /*pMissShaderBindingTable        =>*/ &Shader_Binding_Miss,
                  /*pHitShaderBindingTable         =>*/ &Shader_Binding_Hit,
                  /*pCallableShaderBindingTable    =>*/ &Shader_Binding_Callable,
                  /*width                          =>*/ RT_Dispatch_W,
                  /*height                         =>*/ Render_Height,
                  /*depth                          =>*/ 1);

  // Dispatch postprocess compute shader (unless bypassed for raw PBR output)
  if (not Skip_Postprocess) {
    // Barrier: RT writes > compute reads
    VkImageMemoryBarrier RT_To_Compute_Barriers[] = {
      {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
       .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
       .oldLayout = VK_IMAGE_LAYOUT_GENERAL, .newLayout = VK_IMAGE_LAYOUT_GENERAL,
       .image = Raytracing_Storage_Image.Image,
       .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}},
      {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
       .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
       .oldLayout = VK_IMAGE_LAYOUT_GENERAL, .newLayout = VK_IMAGE_LAYOUT_GENERAL,
       .image = Depth_Image.Image,
       .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}}};
    vkCmdPipelineBarrier (/*commandBuffer            =>*/ Command_Buffer,
                          /*srcStageMask             =>*/ VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                          /*dstStageMask             =>*/ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          /*dependencyFlags          =>*/ 0,
                          /*memoryBarrierCount       =>*/ 0,
                          /*pMemoryBarriers          =>*/ NULL,
                          /*bufferMemoryBarrierCount =>*/ 0,
                          /*pBufferMemoryBarriers    =>*/ NULL,
                          /*imageMemoryBarrierCount  =>*/ 2,
                          /*pImageMemoryBarriers     =>*/ RT_To_Compute_Barriers);

    // Iteration count controlled by quality preset (Potato=0, Low=1, Medium+=2).
    // Passes Budget to the shader - at high budget (cheap path), denoiser is a passthrough.
    int Steps[] = {1, 2};
    int Denoise_Passes = Active_Denoise_Passes;
    if (Denoise_Passes > 0) {
      vkCmdBindPipeline (Command_Buffer, VK_PIPELINE_BIND_POINT_COMPUTE, Denoise_Pipeline);
      for (int I = 0; I < Denoise_Passes; I++) {
        vkCmdBindDescriptorSets (/*commandBuffer      =>*/ Command_Buffer,
                                 /*pipelineBindPoint   =>*/ VK_PIPELINE_BIND_POINT_COMPUTE,
                                 /*layout              =>*/ Denoise_Pipeline_Layout,
                                 /*firstSet            =>*/ 0,
                                 /*descriptorSetCount  =>*/ 1,
                                 /*pDescriptorSets     =>*/ &Denoise_Descriptor_Sets[I],
                                 /*dynamicOffsetCount  =>*/ 0,
                                 /*pDynamicOffsets     =>*/ NULL);
        int Push[2] = {Steps[I], Current_Budget_Byte};
        vkCmdPushConstants (/*commandBuffer =>*/ Command_Buffer,
                            /*layout        =>*/ Denoise_Pipeline_Layout,
                            /*stageFlags    =>*/ VK_SHADER_STAGE_COMPUTE_BIT,
                            /*offset        =>*/ 0,
                            /*size          =>*/ sizeof (Push),
                            /*pValues       =>*/ Push);
        vkCmdDispatch (Command_Buffer, (Render_Width + 7) / 8, (Render_Height + 7) / 8, 1);

        // Barrier between iterations
        VkMemoryBarrier Iter_Barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, NULL,
          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT};
        vkCmdPipelineBarrier (/*commandBuffer            =>*/ Command_Buffer,
                              /*srcStageMask             =>*/ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              /*dstStageMask             =>*/ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              /*dependencyFlags          =>*/ 0,
                              /*memoryBarrierCount       =>*/ 1,
                              /*pMemoryBarriers          =>*/ &Iter_Barrier,
                              /*bufferMemoryBarrierCount =>*/ 0,
                              /*pBufferMemoryBarriers    =>*/ NULL,
                              /*imageMemoryBarrierCount  =>*/ 0,
                              /*pImageMemoryBarriers     =>*/ NULL);
      }
    }

    // Result is now back in Storage_Image.

    // Postprocess: TAA + bloom + tonemapping
    vkCmdBindPipeline       (Command_Buffer, VK_PIPELINE_BIND_POINT_COMPUTE, Postprocess_Pipeline);
    vkCmdBindDescriptorSets (/*commandBuffer      =>*/ Command_Buffer,
                             /*pipelineBindPoint   =>*/ VK_PIPELINE_BIND_POINT_COMPUTE,
                             /*layout              =>*/ Postprocess_Pipeline_Layout,
                             /*firstSet            =>*/ 0,
                             /*descriptorSetCount  =>*/ 1,
                             /*pDescriptorSets     =>*/ &Postprocess_Descriptor_Set,
                             /*dynamicOffsetCount  =>*/ 0,
                             /*pDynamicOffsets     =>*/ NULL);
    vkCmdPushConstants      (/*commandBuffer =>*/ Command_Buffer,
                             /*layout        =>*/ Postprocess_Pipeline_Layout,
                             /*stageFlags    =>*/ VK_SHADER_STAGE_COMPUTE_BIT,
                             /*offset        =>*/ 0,
                             /*size          =>*/ sizeof Postprocess,
                             /*pValues       =>*/ &Postprocess);
    vkCmdDispatch (Command_Buffer, (Render_Width + 7) / 8, (Render_Height + 7) / 8, 1);
  }

  // Select blit source: postprocess writes to Display_Image, raw RT to Storage_Image
  VkImage Blit_Source = Skip_Postprocess ? Raytracing_Storage_Image.Image : Postprocess_Output_Image.Image;
  VkPipelineStageFlags Pre_Blit = Skip_Postprocess
    ? VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

  // Barrier: writes complete before blit reads
  vkCmdPipelineBarrier (/*commandBuffer            =>*/ Command_Buffer,
                        /*srcStageMask             =>*/ Pre_Blit,
                        /*dstStageMask             =>*/ VK_PIPELINE_STAGE_TRANSFER_BIT,
                        /*dependencyFlags          =>*/ 0,
                        /*memoryBarrierCount       =>*/ 1,
                        /*pMemoryBarriers          =>*/ &(VkMemoryBarrier){
                          VK_STRUCTURE_TYPE_MEMORY_BARRIER, NULL,
                          VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT},
                        /*bufferMemoryBarrierCount =>*/ 0,
                        /*pBufferMemoryBarriers    =>*/ NULL,
                        /*imageMemoryBarrierCount  =>*/ 0,
                        /*pImageMemoryBarriers     =>*/ NULL);

  // Transition blit source from general to transfer-source
  Image_Layout_Barrier (/*Command_Buffer     =>*/ Command_Buffer,
                        /*Image              =>*/ Blit_Source,
                        /*Old_Layout         =>*/ VK_IMAGE_LAYOUT_GENERAL,
                        /*New_Layout         =>*/ VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        /*Source_Access      =>*/ VK_ACCESS_SHADER_WRITE_BIT,
                        /*Destination_Access =>*/ VK_ACCESS_TRANSFER_READ_BIT,
                        /*Source_Stage       =>*/ Pre_Blit,
                        /*Destination_Stage  =>*/ VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Transition the swapchain image from undefined to transfer-destination
  Image_Layout_Barrier (/*Command_Buffer     =>*/ Command_Buffer,
                        /*Image              =>*/ Swapchain_Images[Image_Index],
                        /*Old_Layout         =>*/ VK_IMAGE_LAYOUT_UNDEFINED,
                        /*New_Layout         =>*/ VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        /*Source_Access      =>*/ 0,
                        /*Destination_Access =>*/ VK_ACCESS_TRANSFER_WRITE_BIT,
                        /*Source_Stage       =>*/ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        /*Destination_Stage  =>*/ VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Blit result to swapchain (bilinear upscale from internal to window resolution)
  vkCmdBlitImage (/*commandBuffer  =>*/ Command_Buffer,
                  /*srcImage       =>*/ Blit_Source,
                  /*srcImageLayout =>*/ VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  /*dstImage       =>*/ Swapchain_Images[Image_Index],
                  /*dstImageLayout =>*/ VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  /*regionCount    =>*/ 1,
                  /*pRegions       =>*/ &(VkImageBlit){
                    .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                    .srcOffsets[1]  = {Render_Width, Render_Height, 1},
                    .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                    .dstOffsets[1]  = {(int)Swapchain_Extent.width, (int)Swapchain_Extent.height, 1}},
                  /*filter         =>*/ VK_FILTER_LINEAR);

  // Transition blit source back to general for next frame
  Image_Layout_Barrier (/*Command_Buffer     =>*/ Command_Buffer,
                        /*Image              =>*/ Blit_Source,
                        /*Old_Layout         =>*/ VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        /*New_Layout         =>*/ VK_IMAGE_LAYOUT_GENERAL,
                        /*Source_Access      =>*/ VK_ACCESS_TRANSFER_READ_BIT,
                        /*Destination_Access =>*/ VK_ACCESS_SHADER_WRITE_BIT,
                        /*Source_Stage       =>*/ VK_PIPELINE_STAGE_TRANSFER_BIT,
                        /*Destination_Stage  =>*/ VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
                                                | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  // Transition the swapchain image to present-source for display
  Image_Layout_Barrier (/*Command_Buffer     =>*/ Command_Buffer,
                        /*Image              =>*/ Swapchain_Images[Image_Index],
                        /*Old_Layout         =>*/ VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        /*New_Layout         =>*/ VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        /*Source_Access      =>*/ VK_ACCESS_TRANSFER_WRITE_BIT,
                        /*Destination_Access =>*/ 0,
                        /*Source_Stage       =>*/ VK_PIPELINE_STAGE_TRANSFER_BIT,
                        /*Destination_Stage  =>*/ VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

  // Finalize the command buffer recording
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));

  // Submit the command buffer, waiting on image-available and signaling render-finished
  VkPipelineStageFlags Wait_Stage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
  VK_CHECK (vkQueueSubmit (/*queue       =>*/ Queue,
                           /*submitCount =>*/ 1,
                           /*pSubmits    =>*/ &(VkSubmitInfo){
                             .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .waitSemaphoreCount   = 1,
                             .pWaitSemaphores      = &Semaphore_Image_Available,
                             .pWaitDstStageMask    = &Wait_Stage,
                             .commandBufferCount   = 1,
                             .pCommandBuffers      = &Command_Buffer,
                             .signalSemaphoreCount = 1,
                             .pSignalSemaphores    = &Semaphore_Render_Finished},
                           /*fence       =>*/ Fence));

  // Present the rendered image to the display (handle OUT_OF_DATE from resize)
  { VkResult R = vkQueuePresentKHR (/*queue        =>*/ Queue,
                                    /*pPresentInfo =>*/ &(VkPresentInfoKHR){
                                      .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                      .waitSemaphoreCount = 1,
                                      .pWaitSemaphores    = &Semaphore_Render_Finished,
                                      .swapchainCount     = 1,
                                      .pSwapchains        = &Swapchain,
                                      .pImageIndices      = &Image_Index});
    if (R == VK_ERROR_OUT_OF_DATE_KHR or R == VK_SUBOPTIMAL_KHR) Swapchain_Dirty = 1;
    else if (R != VK_SUCCESS) {
      fprintf (stderr, "[vulkan] present error %d at %s:%d\n", R, __FILE__, __LINE__); exit (1);
    }
  }

} // Raytracing_Frame

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §10. Physics
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ══════════════════
//   Quickhull_Dist
// ══════════════════

float Quickhull_Dist (vec3 P, vec3 A, vec3 B, vec3 C) {

  // Compute the signed distance from point P to the plane defined by triangle (A, B, C)
  vec3 Normal = Cross (Subtract (B, A), Subtract (C, A));
  float Length = sqrtf (Dot (Normal, Normal));
  return Length > 1e-8f ? Dot (Subtract (P, A), Scale (Normal, 1.f / Length)) : 0;
}

// ═════════════
//   Quickhull
// ═════════════

Convex_Hull Quickhull (const vec3 *Points, uint Count) {

  // Initialize the output hull
  Convex_Hull Result = {0};

  // Degenerate case: fewer than 4 points cannot form a tetrahedron
  if (Count < 4) {
    for (uint Index = 0; Index < Count and Index < HULL_MAX_VERTS; Index++)
      Result.Vertices[Result.Vertex_Count++] = Points[Index];
    return Result;
  }

  // Find 6 extremal points (min/max along each axis)
  int Extremals[6] = {0, 0, 0, 0, 0, 0};
  for (uint Index = 1; Index < Count; Index++) {
    if (Points[Index].x < Points[Extremals[0]].x) Extremals[0] = Index;
    if (Points[Index].x > Points[Extremals[1]].x) Extremals[1] = Index;
    if (Points[Index].y < Points[Extremals[2]].y) Extremals[2] = Index;
    if (Points[Index].y > Points[Extremals[3]].y) Extremals[3] = Index;
    if (Points[Index].z < Points[Extremals[4]].z) Extremals[4] = Index;
    if (Points[Index].z > Points[Extremals[5]].z) Extremals[5] = Index;
  }

  // Select the most distant pair as the initial edge
  int Point_0 = Extremals[0], Point_1 = Extremals[1];
  float Best_Distance = 0;
  for (int I = 0; I < 6; I++)
    for (int J = I + 1; J < 6; J++) {
      float Distance = Dot (Subtract (Points[Extremals[I]], Points[Extremals[J]]),
                            Subtract (Points[Extremals[I]], Points[Extremals[J]]));
      if (Distance > Best_Distance) { Best_Distance = Distance; Point_0 = Extremals[I]; Point_1 = Extremals[J]; }
    }

  // Find the third point: most distant from the initial edge
  vec3  Edge        = Subtract (Points[Point_1], Points[Point_0]);
  float Edge_Length2 = Dot (Edge, Edge);
  int   Point_2     = -1;
  Best_Distance     = 0;

  // Search all points for the most distant from the edge
  for (uint Index = 0; Index < Count; Index++) {
    if ((int)Index == Point_0 or (int)Index == Point_1) continue;
    vec3  Vector    = Subtract (Points[Index], Points[Point_0]);
    float Parameter = Dot (Vector, Edge) / Edge_Length2;
    float Distance  = Dot (Subtract (Vector, Scale (Edge, Parameter)),
                           Subtract (Vector, Scale (Edge, Parameter)));
    if (Distance > Best_Distance) { Best_Distance = Distance; Point_2 = Index; }
  }
  if (Point_2 < 0) Point_2 = (Point_0 + 1) % Count;

  // Find the fourth point: most distant from the initial triangle
  int Point_3    = -1;
  Best_Distance  = 0;
  for (uint Index = 0; Index < Count; Index++) {
    if ((int)Index == Point_0 or (int)Index == Point_1 or (int)Index == Point_2) continue;
    float Distance = fabsf (Quickhull_Dist (Points[Index], Points[Point_0], Points[Point_1], Points[Point_2]));
    if (Distance > Best_Distance) { Best_Distance = Distance; Point_3 = Index; }
  }
  if (Point_3 < 0) Point_3 = (Point_2 + 1) % Count;

  // Ensure the tetrahedron has consistent winding (fourth point on the negative side)
  if (Quickhull_Dist (Points[Point_3], Points[Point_0], Points[Point_1], Points[Point_2]) > 0) {
    int Temp = Point_0; Point_0 = Point_1; Point_1 = Temp;
  }

  // Initialize the face array with the 4 tetrahedron faces
  Quickhull_Face Faces[HULL_MAX_FACES];
  int            Face_Count = 0;

  // Add the four faces of the seed tetrahedron
  Faces[Face_Count++] = (Quickhull_Face){Point_0, Point_1, Point_2, 0};
  Faces[Face_Count++] = (Quickhull_Face){Point_0, Point_2, Point_3, 0};
  Faces[Face_Count++] = (Quickhull_Face){Point_0, Point_3, Point_1, 0};
  Faces[Face_Count++] = (Quickhull_Face){Point_1, Point_3, Point_2, 0};

  // Assign each remaining point to the face it lies farthest above
  int *Assignments = calloc (Count, sizeof (int));
  for (uint Index = 0; Index < Count; Index++) Assignments[Index] = -1;

  // Find the best face assignment for each non-seed point
  for (uint Index = 0; Index < Count; Index++) {
    if ((int)Index == Point_0 or (int)Index == Point_1 or (int)Index == Point_2 or (int)Index == Point_3) continue;
    float Best = 0;
    for (int Face = 0; Face < Face_Count; Face++) {
      if (Faces[Face].Dead) continue;
      float Distance = Quickhull_Dist (Points[Index], Points[Faces[Face].A], Points[Faces[Face].B], Points[Faces[Face].C]);
      if (Distance > Best) {Best = Distance; Assignments[Index] = Face;}
    }
  }

  // Iteratively expand the hull
  for (int Iteration = 0; Iteration < (int)Count and Face_Count < HULL_MAX_FACES - 20; Iteration++) {

    // Find the point with the greatest distance above its assigned face
    int   Best_Point = -1;
    Best_Distance = 0;
    for (uint Index = 0; Index < Count; Index++) {
      if (Assignments[Index] < 0 or Faces[Assignments[Index]].Dead) continue;
      float Distance = Quickhull_Dist (Points[Index],
                                       Points[Faces[Assignments[Index]].A],
                                       Points[Faces[Assignments[Index]].B],
                                       Points[Faces[Assignments[Index]].C]);
      if (Distance > Best_Distance) {Best_Distance = Distance; Best_Point = Index;}
    }
    if (Best_Point < 0) break;

    // Find all faces visible from the selected point
    int Visible[HULL_MAX_FACES];
    int Visible_Count = 0;
    for (int Face = 0; Face < Face_Count; Face++) {
      if (Faces[Face].Dead) continue;
      if (Quickhull_Dist (Points[Best_Point], Points[Faces[Face].A], Points[Faces[Face].B], Points[Faces[Face].C]) > 1e-6f)
        Visible[Visible_Count++] = Face;
    }

    // Extract the horizon edges (edges shared between one visible and one non-visible face)
    Quickhull_Edge Horizon[HULL_MAX_FACES * 3];
    int            Horizon_Count = 0;

    // Iterate visible faces to collect unshared (horizon) edges
    for (int Vi = 0; Vi < Visible_Count; Vi++) {
      int Face_Index = Visible[Vi];
      int Triangle[3][2] = {{Faces[Face_Index].A, Faces[Face_Index].B},
                            {Faces[Face_Index].B, Faces[Face_Index].C},
                            {Faces[Face_Index].C, Faces[Face_Index].A}};

      // Check each edge for shared visibility with other faces
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
        if (not Shared) Horizon[Horizon_Count++] = (Quickhull_Edge){Triangle[Edge][0], Triangle[Edge][1], Face_Index};
      }
    }

    // Mark all visible faces as dead
    for (int Vi = 0; Vi < Visible_Count; Vi++) Faces[Visible[Vi]].Dead = 1;

    // Create new faces connecting each horizon edge to the new point
    int New_Start = Face_Count;
    for (int Hi = 0; Hi < Horizon_Count and Face_Count < HULL_MAX_FACES; Hi++)
      Faces[Face_Count++] = (Quickhull_Face){Horizon[Hi].V1, Horizon[Hi].V0, Best_Point, 0};

    // Reassign orphaned points to the new faces
    Assignments[Best_Point] = -1;
    for (uint Index = 0; Index < Count; Index++) {
      if (Assignments[Index] < 0) continue;
      if (not Faces[Assignments[Index]].Dead) continue;
      Assignments[Index] = -1;
      float Best = 0;
      for (int Face = New_Start; Face < Face_Count; Face++) {
        float Distance = Quickhull_Dist (Points[Index], Points[Faces[Face].A], Points[Faces[Face].B], Points[Faces[Face].C]);
        if (Distance > Best) { Best = Distance; Assignments[Index] = Face; }
      }
    }
  }
  free (Assignments);

  // Extract unique vertices from surviving faces
  int Remap[HULL_MAX_FACES * 3];
  memset (Remap, -1, sizeof Remap);

  // Collect unique vertices from live faces into the result
  for (int Face = 0; Face < Face_Count; Face++) {
    if (Faces[Face].Dead) continue;
    int Triangle[3] = {Faces[Face].A, Faces[Face].B, Faces[Face].C};
    for (int K = 0; K < 3; K++)
      if (Triangle[K] >= 0 and Triangle[K] < (int)Count and Remap[Triangle[K]] < 0 and Result.Vertex_Count < HULL_MAX_VERTS) {
        Remap[Triangle[K]] = (int)Result.Vertex_Count;
        Result.Vertices[Result.Vertex_Count++] = Points[Triangle[K]];
      }
  }

  // Build per-vertex adjacency table
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

  // Compute centroid and bounding radius
  Result.Centroid = Make (0, 0, 0);
  for (uint Index = 0; Index < Result.Vertex_Count; Index++)
    Result.Centroid = Add (Result.Centroid, Result.Vertices[Index]);
  if (Result.Vertex_Count)
    Result.Centroid = Scale (Result.Centroid, 1.f / Result.Vertex_Count);

  // Compute the bounding sphere radius from the centroid
  Result.Bounding_Radius = 0;
  for (uint Index = 0; Index < Result.Vertex_Count; Index++) {
    float Distance_Sq = Dot (Subtract (Result.Vertices[Index], Result.Centroid),
                             Subtract (Result.Vertices[Index], Result.Centroid));
    if (Distance_Sq > Result.Bounding_Radius * Result.Bounding_Radius)
      Result.Bounding_Radius = sqrtf (Distance_Sq);
  }

  // Log diagnostic output
  printf ("[hull] %u vertices, radius %.1f\n", Result.Vertex_Count, Result.Bounding_Radius);
  return Result;

} // Quick_Hull

// ══════════════════════
//   Hull_From_Vertices
// ══════════════════════

Convex_Hull Hull_From_Vertices (const Vertex *Vertices, uint Count) {

  // Extract vec3 positions from the vertex array and delegate to Quickhull
  vec3 *Positions = malloc (sizeof (vec3) * Count);
  for (uint Index = 0; Index < Count; Index++)
    Positions[Index] = Make (Vertices[Index].Position[0], Vertices[Index].Position[1], Vertices[Index].Position[2]);
  Convex_Hull Hull = Quickhull (Positions, Count);
  free (Positions);
  return Hull;
}

// ═══════════════
//   Hull_Upload
// ═══════════════

void Hull_Upload (const Convex_Hull *Hull) {

  // Pack the CPU-side hull into the GPU-compatible layout
  Gpu_Hull Packed = {0};
  Packed.Count  = (int)Hull->Vertex_Count;
  Packed.Radius = Hull->Bounding_Radius;
  Packed.Centroid[0] = Hull->Centroid.x;
  Packed.Centroid[1] = Hull->Centroid.y;
  Packed.Centroid[2] = Hull->Centroid.z;

  // Copy vertices and adjacency data into the packed layout
  for (uint Index = 0; Index < Hull->Vertex_Count; Index++) {
    Packed.Vertices[Index][0] = Hull->Vertices[Index].x;
    Packed.Vertices[Index][1] = Hull->Vertices[Index].y;
    Packed.Vertices[Index][2] = Hull->Vertices[Index].z;
    Packed.Vertices[Index][3] = 0;
    memcpy (Packed.Adjacency[Index], Hull->Adjacency[Index], sizeof (int) * HULL_MAX_ADJ);
  }

  // Allocate the hull storage buffer on first use, then upload the packed data
  if (not Hull_Storage_Buffer.Buffer)
    Hull_Storage_Buffer = Buffer_Allocate (/*Size         =>*/ sizeof (Gpu_Hull),
                                           /*Usage        =>*/ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                             | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                           /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                             | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  Buffer_Upload (Hull_Storage_Buffer, &Packed, sizeof Packed);
}

// ═══════════════════════════
//   Physics_Pipeline_Create
// ═══════════════════════════

void Physics_Pipeline_Create () {

  // Define the 6 descriptor bindings for the physics compute pipeline
  VkDescriptorSetLayoutBinding Bindings[] = {
    {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // TLAS
    {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // Vertex buffer
    {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // Index buffer
    {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // Player state
    {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // Hull data
    {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}, // Projectiles
  };

  // Create the descriptor set layout with all 6 physics bindings
  VK_CHECK (vkCreateDescriptorSetLayout (/*device      =>*/ Device,
                                         /*pCreateInfo =>*/ &(VkDescriptorSetLayoutCreateInfo){
                                           .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                           .bindingCount = 6,
                                           .pBindings    = Bindings},
                                         /*pAllocator  =>*/ NULL,
                                         /*pSetLayout  =>*/ &Physics_Descriptor_Layout));

  // Create the pipeline layout with push constants for per-frame Gpu_Input delivery
  VkPushConstantRange Push_Range = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof (Gpu_Input)};
  VK_CHECK (vkCreatePipelineLayout (/*device          =>*/ Device,
                                    /*pCreateInfo     =>*/ &(VkPipelineLayoutCreateInfo){
                                      .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                      .setLayoutCount         = 1,
                                      .pSetLayouts            = &Physics_Descriptor_Layout,
                                      .pushConstantRangeCount = 1,
                                      .pPushConstantRanges    = &Push_Range},
                                    /*pAllocator      =>*/ NULL,
                                    /*pPipelineLayout =>*/ &Physics_Pipeline_Layout));

  // Load the pre-compiled physics compute shader and create the compute pipeline
  VkShaderModule Physics_Module = Shader_Module_Load (Shader_Path(Physics));
  VK_CHECK (vkCreateComputePipelines (/*device          =>*/ Device,
                                      /*pipelineCache   =>*/ Pipeline_Cache,
                                      /*createInfoCount =>*/ 1,
                                      /*pCreateInfos    =>*/ &(VkComputePipelineCreateInfo){
                                        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                        .stage  = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0,
                                                   VK_SHADER_STAGE_COMPUTE_BIT, Physics_Module, "main", NULL},
                                        .layout = Physics_Pipeline_Layout},
                                      /*pAllocator      =>*/ NULL,
                                      /*pPipelines      =>*/ &Physics_Pipeline));
  vkDestroyShaderModule (Device, Physics_Module, NULL);
}

// ════════════════════════════
//   Physics_Resources_Create
// ════════════════════════════

void Physics_Resources_Create (const Player *Initial_State) {

  // Allocate the host-visible player state buffer for GPU read-write access each frame
  Player_State_Buffer = Buffer_Allocate (/*Size         =>*/ sizeof (Gpu_Player),
                                         /*Usage        =>*/ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                           | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                         /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                           | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

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
    Hull_Storage_Buffer = Buffer_Allocate (/*Size         =>*/ sizeof (Gpu_Hull),
                                           /*Usage        =>*/ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                             | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                           /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                             | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    Gpu_Hull Empty = {0};
    Empty.Count = 1;
    Buffer_Upload (Hull_Storage_Buffer, &Empty, sizeof Empty);
  }

  // Allocate the projectile pool buffer (binding 5)
  Projectile_Buffer = Buffer_Allocate (/*Size         =>*/ sizeof (Gpu_Projectile_Pool),
                                       /*Usage        =>*/ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                         | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                       /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  Gpu_Projectile_Pool Empty_Pool = {0};
  Buffer_Upload (Projectile_Buffer, &Empty_Pool, sizeof Empty_Pool);

  // Allocate the physics descriptor pool and set
  VkDescriptorPoolSize Pool_Sizes[] = {
    {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             5}}; // 5 storage buffers: vertex, index, player, hull, projectiles
  VK_CHECK (vkCreateDescriptorPool (/*device          =>*/ Device,
                                    /*pCreateInfo     =>*/ &(VkDescriptorPoolCreateInfo){
                                      .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                      .maxSets       = 1,
                                      .poolSizeCount = 2,
                                      .pPoolSizes    = Pool_Sizes},
                                    /*pAllocator      =>*/ NULL,
                                    /*pDescriptorPool =>*/ &Physics_Descriptor_Pool));

  VK_CHECK (vkAllocateDescriptorSets (/*device          =>*/ Device,
                                      /*pAllocateInfo   =>*/ &(VkDescriptorSetAllocateInfo){
                                        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                        .descriptorPool     = Physics_Descriptor_Pool,
                                        .descriptorSetCount = 1,
                                        .pSetLayouts        = &Physics_Descriptor_Layout},
                                      /*pDescriptorSets =>*/ &Physics_Descriptor_Set));

  // Write all 5 descriptor bindings: TLAS, vertex buffer, index buffer, player state, hull data
  VkWriteDescriptorSetAccelerationStructureKHR Acceleration_Write = {
    .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
    .accelerationStructureCount = 1,
    .pAccelerationStructures    = &Top_Level.Handle};
  VkDescriptorBufferInfo Vertex_Info  = {Vertex_Buffer.Buffer,       0, Vertex_Buffer.Size};
  VkDescriptorBufferInfo Index_Info   = {Index_Buffer.Buffer,        0, Index_Buffer.Size};
  VkDescriptorBufferInfo Player_Info  = {Player_State_Buffer.Buffer, 0, Player_State_Buffer.Size};
  VkDescriptorBufferInfo Hull_Info    = {Hull_Storage_Buffer.Buffer, 0, Hull_Storage_Buffer.Size};
  VkDescriptorBufferInfo Proj_Info    = {Projectile_Buffer.Buffer,   0, Projectile_Buffer.Size};

  // Write all 6 physics descriptor bindings
  VkWriteDescriptorSet Writes[] = {
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, &Acceleration_Write, Physics_Descriptor_Set, 0, 0, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, NULL, NULL,         NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,                Physics_Descriptor_Set, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL, &Vertex_Info, NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,                Physics_Descriptor_Set, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL, &Index_Info,  NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,                Physics_Descriptor_Set, 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL, &Player_Info, NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,                Physics_Descriptor_Set, 4, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL, &Hull_Info,   NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,                Physics_Descriptor_Set, 5, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL, &Proj_Info,   NULL},
  };
  vkUpdateDescriptorSets (Device, 6, Writes, 0, NULL);

} // Physics_Resources_Create

// ════════════════════
//   Physics_Dispatch
// ════════════════════

Player Physics_Dispatch (Input In, float Dt) {

  // Pack the CPU input into the GPU push constant structure
  Gpu_Input GPU_Input = {
    In.Forward, In.Back, In.Left, In.Right,
    In.Jump, In.Fire, In.Crouch, 0,
    In.Delta_X, In.Delta_Y, Dt, 0};

  // Record a one-shot command buffer for the physics compute dispatch
  VK_CHECK (vkResetCommandBuffer (Command_Buffer, 0));
  VK_CHECK (vkBeginCommandBuffer (/*commandBuffer =>*/ Command_Buffer,
                                  /*pBeginInfo    =>*/ &(VkCommandBufferBeginInfo){
                                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));

  // Bind the physics pipeline and descriptors, push the input, dispatch a single workgroup
  vkCmdBindPipeline       (Command_Buffer, VK_PIPELINE_BIND_POINT_COMPUTE, Physics_Pipeline);
  vkCmdBindDescriptorSets (/*commandBuffer      =>*/ Command_Buffer,
                           /*pipelineBindPoint   =>*/ VK_PIPELINE_BIND_POINT_COMPUTE,
                           /*layout              =>*/ Physics_Pipeline_Layout,
                           /*firstSet            =>*/ 0,
                           /*descriptorSetCount  =>*/ 1,
                           /*pDescriptorSets     =>*/ &Physics_Descriptor_Set,
                           /*dynamicOffsetCount  =>*/ 0,
                           /*pDynamicOffsets     =>*/ NULL);
  vkCmdPushConstants      (/*commandBuffer =>*/ Command_Buffer,
                           /*layout        =>*/ Physics_Pipeline_Layout,
                           /*stageFlags    =>*/ VK_SHADER_STAGE_COMPUTE_BIT,
                           /*offset        =>*/ 0,
                           /*size          =>*/ sizeof GPU_Input,
                           /*pValues       =>*/ &GPU_Input);
  vkCmdDispatch           (Command_Buffer, 1, 1, 1);

  // Memory barrier: ensure compute shader writes are visible to the host before readback
  vkCmdPipelineBarrier (/*commandBuffer            =>*/ Command_Buffer,
                        /*srcStageMask             =>*/ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        /*dstStageMask             =>*/ VK_PIPELINE_STAGE_HOST_BIT,
                        /*dependencyFlags          =>*/ 0,
                        /*memoryBarrierCount       =>*/ 1,
                        /*pMemoryBarriers          =>*/ &(VkMemoryBarrier){
                          .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                          .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                          .dstAccessMask = VK_ACCESS_HOST_READ_BIT},
                        /*bufferMemoryBarrierCount =>*/ 0,
                        /*pBufferMemoryBarriers    =>*/ NULL,
                        /*imageMemoryBarrierCount  =>*/ 0,
                        /*pImageMemoryBarriers     =>*/ NULL);

  // Submit the command buffer and wait for the GPU to finish
  VK_CHECK (vkEndCommandBuffer (Command_Buffer));
  VK_CHECK (vkQueueSubmit (/*queue       =>*/ Queue,
                           /*submitCount =>*/ 1,
                           /*pSubmits    =>*/ &(VkSubmitInfo){
                             .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .commandBufferCount = 1,
                             .pCommandBuffers    = &Command_Buffer},
                           /*fence       =>*/ VK_NULL_HANDLE));
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

} // Physics_Dispatch


// ════════════════════
//   Projectile_Spawn
// ════════════════════

void Projectile_Spawn (vec3 Origin, vec3 Direction) {
  if (Projectiles.Count >= MAX_PROJECTILES) return;
  if (Projectiles.Fire_Cooldown > 0) return;

  // Initialize projectile state
  vec3 Normalized = Normalize (Direction);
  Projectile *P   = &Projectiles.Slots[Projectiles.Count++];
  P->Position     = Add (Origin, Scale (Normalized, 20.f)); // Spawn 20 units ahead
  P->Velocity     = Scale (Normalized, ROCKET_SPEED);
  P->Lifetime     = ROCKET_LIFETIME;
  P->Active       = 1;
  P->Radius       = 3.f;
  P->Damage       = ROCKET_DAMAGE;
  P->Material_Hit = MATERIAL_DEFAULT;
  P->Hit_U        = 0.f;
  P->Hit_V        = 0.f;
  P->Instance_Hit = -1;

  // Start fire cooldown and play sound
  Projectiles.Fire_Cooldown = FIRE_COOLDOWN;
  Audio_Play (Audio.Sound_Shoot, 0.8f);
}

// ══════════════════════════
//   Projectile_Pool_Upload
// ══════════════════════════

void Projectile_Pool_Upload () {
  Gpu_Projectile_Pool GPU_Pool = {0};
  GPU_Pool.Count         = Projectiles.Count;
  GPU_Pool.Fire_Cooldown = Projectiles.Fire_Cooldown;
  for (int I = 0; I < Projectiles.Count; I++) {
    Projectile *P = &Projectiles.Slots[I];
    GPU_Pool.Slots[I] = (Gpu_Projectile){
      {P->Position.x, P->Position.y, P->Position.z}, 0,
      {P->Velocity.x, P->Velocity.y, P->Velocity.z}, P->Lifetime,
      P->Active, P->Material_Hit, P->Radius, P->Damage,
      P->Hit_U, P->Hit_V, P->Instance_Hit, 0};
  }
  Buffer_Upload (Projectile_Buffer, &GPU_Pool, sizeof GPU_Pool);
}

// ════════════════════════════
//   Projectile_Pool_Readback
// ════════════════════════════

void Projectile_Pool_Readback () {
  Gpu_Projectile_Pool *Mapped;
  vkMapMemory (Device, Projectile_Buffer.Memory, 0, sizeof (Gpu_Projectile_Pool), 0, (void **)&Mapped);
  Projectiles.Count         = Mapped->Count;
  Projectiles.Fire_Cooldown = Mapped->Fire_Cooldown;
  for (int I = 0; I < Projectiles.Count; I++) {
    Gpu_Projectile *G = &Mapped->Slots[I];
    Projectiles.Slots[I] = (Projectile){
      .Position     = {G->Position[0], G->Position[1], G->Position[2]},
      .Velocity     = {G->Velocity[0], G->Velocity[1], G->Velocity[2]},
      .Lifetime     = G->Lifetime,
      .Active       = G->Active,
      .Material_Hit = G->Material_Hit,
      .Radius       = G->Radius,
      .Damage       = G->Damage,
      .Hit_U        = G->Hit_U,
      .Hit_V        = G->Hit_V,
      .Instance_Hit = G->Instance_Hit};
  }
  vkUnmapMemory (Device, Projectile_Buffer.Memory);

  // Compact: remove dead projectiles, play explosion sound on impact
  int Write = 0;
  for (int I = 0; I < Projectiles.Count; I++) {
    if (Projectiles.Slots[I].Active) {
      if (Write != I) Projectiles.Slots[Write] = Projectiles.Slots[I];
      Write++;
    } else {
      Audio_Play (Audio.Sound_Explode, 0.7f);
    }
  }
  Projectiles.Count = Write;
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §13. Audio
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ═════════════
//   Mode_Init
// ═════════════

void Mode_Init (Mode_Resonator *M, float Freq_Hz, float T60, float Gain) {
  float R = powf (0.001f, 1.f / (T60 * MODAL_SAMPLE_RATE)); // r from T60: amplitude decays by 60 dB over T60 seconds
  float W = 2.f * 3.14159265f * Freq_Hz / MODAL_SAMPLE_RATE;
  M->a1 = 2.f * R * cosf (W);
  M->a2 = -(R * R);
  M->b0 = Gain;
  M->y1 = M->y2 = 0.f;
}

// ═════════════
//   Mode_Tick
// ═════════════

float Mode_Tick (Mode_Resonator *M, float X) {
  float Y = M->a1 * M->y1 + M->a2 * M->y2 + M->b0 * X;
  M->y2 = M->y1;
  M->y1 = Y;
  return Y;
}

// ═══════════════════════════════
//   Audio_Generate_Modal_Impact
// ═══════════════════════════════

ALuint Audio_Generate_Modal_Impact (int Material, float Impulse_Strength, float Duration, float Volume) {
  int Samples = (int)(MODAL_SAMPLE_RATE * Duration);
  short *Data = malloc (Samples * sizeof (short));

  // Select material modes
  if (Material < 0 or Material > 5) Material = 0;
  const Mode_Spec *Specs = Material_Modes[Material];

  // Initialize resonator bank
  Mode_Resonator Bank[MODAL_MAX_MODES];
  for (int I = 0; I < MODAL_MAX_MODES; I++)
    Mode_Init (&Bank[I], Specs[I].Freq, Specs[I].T60, Specs[I].Gain * Impulse_Strength);

  // Generate excitation: short impulse pulse (1-5ms) shaped by contact force profile
  int Impulse_Len = (int)(MODAL_SAMPLE_RATE * 0.003f); // 3ms impulse
  if (Impulse_Len < 1) Impulse_Len = 1;

  // LCG for acceleration noise
  unsigned Seed = 42 + Material * 7;

  // Synthesize audio samples from the resonator bank
  for (int I = 0; I < Samples; I++) {
    // Excitation: raised-cosine impulse for the first few ms
    float Excitation = 0;
    if (I < Impulse_Len) {
      float Phase = 3.14159f * (float)I / Impulse_Len;
      Excitation = 0.5f * (1.f - cosf (Phase)) * Impulse_Strength;
    }

    // Acceleration noise: band-limited noise that decays, fills in high-frequency content the modal bank misses (per Cornell SIGGRAPH 2012)
    float T = (float)I / MODAL_SAMPLE_RATE;
    float Noise_Env = expf (-T * 20.f) * 0.15f * Impulse_Strength;
    Seed = Seed * 1103515245 + 12345;
    float Noise = ((float)(Seed & 0x7FFF) / 16384.f - 1.f) * Noise_Env;

    // Sum all resonator outputs driven by the excitation
    float Y = 0;
    for (int M = 0; M < MODAL_MAX_MODES; M++)
      Y += Mode_Tick (&Bank[M], Excitation);
    Y += Noise;

    // Soft clip to prevent overflow
    if (Y >  1.f) Y =  1.f;
    if (Y < -1.f) Y = -1.f;
    Data[I] = (short)(Y * Volume * 32767.f);
  }

  // Upload PCM data to an OpenAL buffer
  ALuint Buffer;
  alGenBuffers (1, &Buffer);
  alBufferData (Buffer, AL_FORMAT_MONO16, Data, Samples * sizeof (short), MODAL_SAMPLE_RATE);
  free (Data);
  return Buffer;
}

// ══════════════════════════════════
//   Audio_Generate_Explosion_Modal
// ══════════════════════════════════

ALuint Audio_Generate_Explosion_Modal (float Duration, float Volume) {
  int Samples = (int)(MODAL_SAMPLE_RATE * Duration);
  short *Data = calloc (Samples, sizeof (short));

  // Initial broadband shock: excite stone + metal modes simultaneously
  Mode_Resonator Shock_Bank[MODAL_MAX_MODES];
  for (int I = 0; I < MODAL_MAX_MODES; I++) {
    // Mix stone and metal modes for broadband character
    float Freq = (Modes_Stone[I].Freq + Modes_Metal[I].Freq) * 0.5f * 0.6f; // Lower for explosion
    float T60  = (Modes_Stone[I].T60  + Modes_Metal[I].T60) * 0.5f * 1.5f;  // Longer decay
    float Gain = (Modes_Stone[I].Gain + Modes_Metal[I].Gain) * 0.5f;
    Mode_Init (&Shock_Bank[I], Freq, T60, Gain);
  }

  // Set up RNG seed and timing parameters
  unsigned Seed = 99;
  int Shock_Len = (int)(MODAL_SAMPLE_RATE * 0.01f); // 10ms shock
  int Debris_Count = 8;

  // Generate shock phase
  for (int I = 0; I < Samples; I++) {
    float Exc = 0;
    if (I < Shock_Len) Exc = 1.0f * (1.f - (float)I / Shock_Len);

    // Rumble noise
    float T = (float)I / MODAL_SAMPLE_RATE;
    Seed = Seed * 1103515245 + 12345;
    float Noise = ((float)(Seed & 0x7FFF) / 16384.f - 1.f);
    float Noise_Env = expf (-T * 4.f) * 0.6f;

    // Mix noise with resonator output
    float Y = Noise * Noise_Env;
    for (int M = 0; M < MODAL_MAX_MODES; M++)
      Y += Mode_Tick (&Shock_Bank[M], Exc);

    // Soft clip and write sample
    if (Y >  1.f) Y =  1.f;
    if (Y < -1.f) Y = -1.f;
    Data[I] = (short)(Y * Volume * 32767.f);
  }

  // Overlay debris sub-impacts at staggered times (perceptual scheduling)
  for (int D = 0; D < Debris_Count; D++) {
    Seed = Seed * 1103515245 + 12345;
    int Start = (Seed % (Samples / 2)) + Samples / 8; // Stagger after initial shock
    int Mat = (Seed >> 8) % 4; // Random material
    float Strength = 0.3f + 0.4f * ((Seed >> 16) & 0xFF) / 255.f;

    // Initialize resonator bank for this debris piece
    Mode_Resonator Debris_Bank[MODAL_MAX_MODES];
    const Mode_Spec *Specs = Material_Modes[Mat];
    for (int M = 0; M < MODAL_MAX_MODES; M++)
      Mode_Init (&Debris_Bank[M], Specs[M].Freq, Specs[M].T60, Specs[M].Gain * Strength);

    // Synthesize and mix the debris sub-impact into the output
    int Imp_Len = (int)(MODAL_SAMPLE_RATE * 0.002f);
    for (int I = 0; I < Samples - Start; I++) {
      float Exc = (I < Imp_Len) ? (1.f - (float)I / Imp_Len) * Strength : 0;
      float Y = 0;
      for (int M = 0; M < MODAL_MAX_MODES; M++)
        Y += Mode_Tick (&Debris_Bank[M], Exc);
      int Idx = Start + I;
      float Existing = Data[Idx] / 32767.f;
      float Mixed = Existing + Y * Volume * 0.3f;
      if (Mixed >  1.f) Mixed =  1.f;
      if (Mixed < -1.f) Mixed = -1.f;
      Data[Idx] = (short)(Mixed * 32767.f);
    }
  }

  // Upload PCM data to an OpenAL buffer
  ALuint Buffer;
  alGenBuffers (1, &Buffer);
  alBufferData (Buffer, AL_FORMAT_MONO16, Data, Samples * sizeof (short), MODAL_SAMPLE_RATE);
  free (Data);
  return Buffer;

} // Audio_Generate_Explosion_Modal

// ══════════════════════════════
//   Audio_Generate_Weapon_Fire
// ══════════════════════════════

ALuint Audio_Generate_Weapon_Fire (float Volume) {
  float Duration = 0.2f;
  int Samples = (int)(MODAL_SAMPLE_RATE * Duration);
  short *Data = malloc (Samples * sizeof (short));

  // Metal mechanism: high-frequency, short-decay metal modes
  Mode_Resonator Mech_Bank[MODAL_MAX_MODES];
  for (int I = 0; I < MODAL_MAX_MODES; I++)
    Mode_Init (&Mech_Bank[I], Modes_Metal[I].Freq * 1.5f, Modes_Metal[I].T60 * 0.3f,
               Modes_Metal[I].Gain * 0.8f);

  // Set up RNG seed and click duration
  unsigned Seed = 777;
  int Click_Len = (int)(MODAL_SAMPLE_RATE * 0.001f); // 1ms click

  // Synthesize audio samples combining mechanism click and gas noise
  for (int I = 0; I < Samples; I++) {
    float T = (float)I / MODAL_SAMPLE_RATE;

    // Mechanism click excitation
    float Exc = (I < Click_Len) ? 1.f : 0;

    // Gas expansion: broadband noise with fast decay
    Seed = Seed * 1103515245 + 12345;
    float Noise = ((float)(Seed & 0x7FFF) / 16384.f - 1.f);
    float Gas_Env = expf (-T * 30.f) * 0.7f;

    // Mix gas noise with resonator output
    float Y = Noise * Gas_Env;
    for (int M = 0; M < MODAL_MAX_MODES; M++)
      Y += Mode_Tick (&Mech_Bank[M], Exc);

    // Soft clip and write sample
    if (Y >  1.f) Y =  1.f;
    if (Y < -1.f) Y = -1.f;
    Data[I] = (short)(Y * Volume * 32767.f);
  }

  // Upload PCM data to an OpenAL buffer
  ALuint Buffer;
  alGenBuffers (1, &Buffer);
  alBufferData (Buffer, AL_FORMAT_MONO16, Data, Samples * sizeof (short), MODAL_SAMPLE_RATE);
  free (Data);
  return Buffer;
}

// ══════════════════
//   Audio_Load_WAV
// ══════════════════

ALuint Audio_Load_WAV (const char *Path) {
  FILE *F = fopen (Path, "rb");
  if (not F) return 0;

  // Read WAV header (44 bytes minimum)
  unsigned char Header[44];
  if (fread (Header, 1, 44, F) < 44) { fclose (F); return 0; }

  // Verify RIFF/WAVE signature
  if (memcmp (Header, "RIFF", 4) != 0 or memcmp (Header + 8, "WAVE", 4) != 0) { fclose (F); return 0; }

  // Parse format chunk
  int Channels    = Header[22] | (Header[23] << 8);
  int Sample_Rate = Header[24] | (Header[25] << 8) | (Header[26] << 16) | (Header[27] << 24);
  int Bits        = Header[34] | (Header[35] << 8);
  int Data_Size   = Header[40] | (Header[41] << 8) | (Header[42] << 16) | (Header[43] << 24);

  // Read PCM data
  void *Data = malloc (Data_Size);
  int Read = (int)fread (Data, 1, Data_Size, F);
  fclose (F);
  if (Read < Data_Size) Data_Size = Read;

  // Determine OpenAL format
  ALenum Format;
  if (Channels == 1 and Bits == 8)       Format = AL_FORMAT_MONO8;
  else if (Channels == 1 and Bits == 16) Format = AL_FORMAT_MONO16;
  else if (Channels == 2 and Bits == 8)  Format = AL_FORMAT_STEREO8;
  else if (Channels == 2 and Bits == 16) Format = AL_FORMAT_STEREO16;
  else { free (Data); return 0; }

  // Create OpenAL buffer and upload PCM data
  ALuint Buffer;
  alGenBuffers (1, &Buffer);
  alBufferData (Buffer, Format, Data, Data_Size, Sample_Rate);
  free (Data);
  return Buffer;
}

// ════════════════════════════════
//   Audio_Load_WAV_Or_Synthesize
// ════════════════════════════════

ALuint Audio_Load_WAV_Or_Modal (const char *Path, int Material, float Impulse, float Duration, float Volume) {
  ALuint Buf = Audio_Load_WAV (Path);
  if (Buf) { printf ("[audio] loaded %s\n", Path); return Buf; }
  printf ("[audio] synthesizing fallback for %s\n", Path);
  return Audio_Generate_Modal_Impact (Material, Impulse, Duration, Volume);
}

// ══════════════
//   Audio_Init
// ══════════════

void Audio_Init () {
  memset (&Audio, 0, sizeof Audio);

  // Open audio device
  Audio.Device = alcOpenDevice (NULL);
  if (not Audio.Device) { fprintf (stderr, "[audio] failed to open device\n"); return; }

  // Create and activate audio context
  Audio.Context = alcCreateContext (Audio.Device, NULL);
  alcMakeContextCurrent (Audio.Context);

  // Generate sound sources
  alGenSources (MAX_AUDIO_SOURCES, Audio.Sources);
  Audio.Source_Count = MAX_AUDIO_SOURCES;

  // Load sounds
  Audio.Sound_Shoot = Audio.Buffer_Count;
  Audio.Buffers[Audio.Buffer_Count] = Audio_Load_WAV (ASSET_ROOT "sound/weapons/machinegun/machgf1b.wav");
  if (not Audio.Buffers[Audio.Buffer_Count]) {
    printf ("[audio] synthesizing weapon fire (modal)\n");
    Audio.Buffers[Audio.Buffer_Count] = Audio_Generate_Weapon_Fire (0.8f);
  }
  Audio.Buffer_Count++;
  Audio.Sound_Explode = Audio.Buffer_Count;
  Audio.Buffers[Audio.Buffer_Count] = Audio_Load_WAV (ASSET_ROOT "sound/weapons/rocket/rocklx1a.wav");
  if (not Audio.Buffers[Audio.Buffer_Count]) {
    printf ("[audio] synthesizing explosion (modal)\n");
    Audio.Buffers[Audio.Buffer_Count] = Audio_Generate_Explosion_Modal (0.6f, 0.9f);
  }
  Audio.Buffer_Count++;
  Audio.Sound_Step_Stone = Audio.Buffer_Count;
  Audio.Buffers[Audio.Buffer_Count++] = Audio_Load_WAV_Or_Modal (
    ASSET_ROOT "sound/player/footsteps/step1.wav", MATERIAL_STONE, 0.4f, 0.15f, 0.5f);
  Audio.Sound_Step_Metal = Audio.Buffer_Count;
  Audio.Buffers[Audio.Buffer_Count++] = Audio_Load_WAV_Or_Modal (
    ASSET_ROOT "sound/player/footsteps/clank1.wav", MATERIAL_METAL, 0.5f, 0.12f, 0.5f);
  Audio.Sound_Jump = Audio.Buffer_Count;
  Audio.Buffers[Audio.Buffer_Count++] = Audio_Load_WAV_Or_Modal (
    ASSET_ROOT "sound/player/footsteps/boot1.wav", MATERIAL_STONE, 0.6f, 0.1f, 0.4f);
  Audio.Sound_Land = Audio.Buffer_Count;
  Audio.Buffers[Audio.Buffer_Count++] = Audio_Load_WAV_Or_Modal (
    ASSET_ROOT "sound/player/land1.wav", MATERIAL_STONE, 1.0f, 0.2f, 0.6f);

  // OpenAL EFX effects
  if (alcIsExtensionPresent (Audio.Device, "ALC_EXT_EFX")) {
    LPALGENEFFECTS alGenEffects = (LPALGENEFFECTS) alGetProcAddress ("alGenEffects");
    LPALEFFECTI    alEffecti    = (LPALEFFECTI)    alGetProcAddress ("alEffecti");
    LPALEFFECTF    alEffectf    = (LPALEFFECTF)    alGetProcAddress ("alEffectf");
    LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots =
      (LPALGENAUXILIARYEFFECTSLOTS) alGetProcAddress ("alGenAuxiliaryEffectSlots");
    LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti =
      (LPALAUXILIARYEFFECTSLOTI) alGetProcAddress ("alAuxiliaryEffectSloti");

    // Set up EFX reverb if supported
    if (alGenEffects and alGenAuxiliaryEffectSlots) {
      ALuint Effect, Slot;
      alGenEffects (1, &Effect);
      alEffecti (Effect, AL_EFFECT_TYPE, AL_EFFECT_REVERB);

      // Subtle room reverb: moderate decay, not too wet - physically based indoor arena
      alEffectf (Effect, AL_REVERB_DECAY_TIME, 1.2f);    // 1.2s decay for indoor arena
      alEffectf (Effect, AL_REVERB_GAIN, 0.4f);           // Not too loud
      alEffectf (Effect, AL_REVERB_GAINHF, 0.6f);         // Keep some high-frequency
      alEffectf (Effect, AL_REVERB_REFLECTIONS_GAIN, 0.3f);
      alEffectf (Effect, AL_REVERB_LATE_REVERB_GAIN, 0.25f);

      // Create the effect slot and attach the reverb
      alGenAuxiliaryEffectSlots (1, &Slot);
      alAuxiliaryEffectSloti (Slot, AL_EFFECTSLOT_EFFECT, Effect);

      // Route all sources through the reverb slot
      for (int I = 0; I < Audio.Source_Count; I++)
        alSource3i (Audio.Sources[I], AL_AUXILIARY_SEND_FILTER, Slot, 0, AL_FILTER_NULL);

      // Log diagnostic output
      printf ("[audio] EFX reverb enabled (1.2s arena decay)\n");
    }
  }

  // Initialize ground state for footstep tracking
  Audio.Was_On_Ground = 1;
  printf ("[audio] initialized: %d buffers, %d sources\n", Audio.Buffer_Count, Audio.Source_Count);

} // Audio_Init

// ══════════════
//   Audio_Play
// ══════════════

void Audio_Play (int Sound_Index, float Volume) {
  if (not Audio.Device or Sound_Index < 0 or Sound_Index >= Audio.Buffer_Count) return;

  // Find a free source
  for (int I = 0; I < Audio.Source_Count; I++) {
    ALint State;
    alGetSourcei (Audio.Sources[I], AL_SOURCE_STATE, &State);
    if (State != AL_PLAYING) {
      alSourcei  (Audio.Sources[I], AL_BUFFER, Audio.Buffers[Sound_Index]);
      alSourcef  (Audio.Sources[I], AL_GAIN, Volume);
      alSourcePlay (Audio.Sources[I]);
      return;
    }
  }
}

// ══════════════════════════
//   Audio_Update_Footsteps
// ══════════════════════════

void Audio_Update_Footsteps (Player *P, float Dt) {
  if (not Audio.Device) return;

  // Landing detection
  if (P->On_Ground and not Audio.Was_On_Ground) {
    Audio_Play (Audio.Sound_Land, 0.6f);
  }
  Audio.Was_On_Ground = P->On_Ground;

  // Footstep accumulation (based on horizontal speed)
  if (P->On_Ground) {
    float Speed = sqrtf (P->Velocity.x * P->Velocity.x + P->Velocity.z * P->Velocity.z);
    if (Speed > 50.f) {
      Audio.Step_Accumulator += Speed * Dt;
      float Step_Distance = 200.f; // Units per footstep
      if (Audio.Step_Accumulator >= Step_Distance) {
        Audio.Step_Accumulator -= Step_Distance;
        Audio_Play (Audio.Sound_Step_Stone, 0.3f);
      }
    } else {
      Audio.Step_Accumulator = 0;
    }
  }
}

// ══════════════════
//   Audio_Shutdown
// ══════════════════

void Audio_Shutdown () {
  if (not Audio.Device) return;
  alDeleteSources (Audio.Source_Count, Audio.Sources);
  alDeleteBuffers (Audio.Buffer_Count, Audio.Buffers);
  alcDestroyContext (Audio.Context);
  alcCloseDevice (Audio.Device);
  memset (&Audio, 0, sizeof Audio);
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §13. Shaders
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ══════════════════════
//   Shader_Module_Load
// ══════════════════════

VkShaderModule Shader_Module_Load (const char *Path) {

  // Open the SPIR-V binary file
  FILE *File = fopen (Path, "rb");
  if (not File) {fprintf (stderr, "Cannot open shader %s\n", Path); exit (1); }

  // Read the file size and allocate a buffer for the SPIR-V bytecode
  fseek (File, 0, SEEK_END);
  long Size = ftell (File);
  rewind (File);
  uint *Code = malloc (Size);
  size_t Code_Read_ = fread (Code, 1, Size, File); (void)Code_Read_;
  fclose (File);

  // Wrap the raw SPIR-V code in a Vulkan shader module
  VkShaderModule Module;
  VK_CHECK (vkCreateShaderModule (/*device      =>*/ Device,
                                  /*pCreateInfo =>*/ &(VkShaderModuleCreateInfo){
                                    .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                    .codeSize = (size_t)Size,
                                    .pCode    = Code},
                                  /*pAllocator  =>*/ NULL,
                                  /*pShaderModule =>*/ &Module));

  // Release allocated memory
  free (Code);
  return Module;
}

// ══════════════════
//   Ray_Generation
// ══════════════════

glsl rgen Ray_Generation {
  #version 460
  #extension GL_EXT_ray_tracing : require
  
  layout(binding = 0) uniform accelerationStructureEXT  Top_Level;
  layout(binding = 1, rgba16f) uniform image2D            Storage_Image;
  layout(binding = 2) uniform Camera_Uniform {
    mat4  Inverse_View; mat4 Inverse_Projection;
    uint  Frame; uint Weapon_Texture_Base; uint PBR_Stride; uint Active_SPP;
    vec4  Env_Sun_Dir;      // xyz = direction, w = angular_radius
    vec4  Env_Sun_Color;    // xyz = color, w = intensity
    vec4  Env_Sky_Zenith;   // xyz = zenith color, w = sky_intensity
    vec4  Env_Sky_Horizon;  // xyz = horizon color, w = cos(sun_disc_size)
    vec4  Env_Ambient_Up;   // xyz = ambient up, w = sun_disc_intensity
    vec4  Env_Ambient_Down; // xyz = ambient down, w = fog_density
    vec4  Env_Fog_Color;    // xyz = fog color
  };
  layout(binding = 11, r32f) uniform image2D             Depth_Output;
  
  layout(location = 0) rayPayloadEXT vec4 Payload;  // rgb = color, a = hit distance
  
  // Stratified sub-pixel jitter for multi-sample anti-aliasing
  uint PCG (uint V) {
    uint S = V * 747796405u + 2891336453u;
    uint W = ((S >> ((S >> 28u) + 4u)) ^ S) * 277803737u;
    return (W >> 22u) ^ W;
  }
  
  // Ray_Generation main
  void main () {
    int SPP = int (Active_SPP & 0xFFu);

    // Checkerboard half-width dispatch: when gl_LaunchSizeEXT.x < image width
    ivec2 Img_Size = imageSize (Storage_Image);
    ivec2 Px;
    if (int(gl_LaunchSizeEXT.x) < Img_Size.x) {
      Px.x = int(gl_LaunchIDEXT.x) * 2 + ((int(gl_LaunchIDEXT.y) + int(Frame)) & 1);
      Px.y = int(gl_LaunchIDEXT.y);
      if (Px.x >= Img_Size.x) return;
    } else {
      Px = ivec2 (gl_LaunchIDEXT.xy);
    }

    vec3  Origin     = (Inverse_View * vec4 (0, 0, 0, 1)).xyz;
    vec3  Color_Sum  = vec3 (0.0);
    float Depth_Sum  = 0.0;

    // Ray direction from pixel position (use full image size for correct UV mapping)
    vec2  Pixel  = vec2 (Px) + 0.5;
    vec2  Uv     = Pixel / vec2 (Img_Size);
    vec2  Ndc    = fma (Uv, vec2 (2.0), vec2 (-1.0));
    vec4  Target    = Inverse_Projection * vec4 (Ndc.x, Ndc.y, 0.0, 1.0);
    vec4  Direction = Inverse_View * vec4 (normalize (Target.xyz / Target.w), 0.0);
  
    // Accumulate color and depth across all samples
    for (int S = 0; S < SPP; S++) {
      Payload = vec4 (0.0, 0.0, 0.0, 10000.0);

      // Mask 0xFD: primary rays see everything EXCEPT player body (bit 1 = 0x02).
      traceRayEXT (Top_Level, gl_RayFlagsOpaqueEXT, 0xFD, 0, 0, 0,
                   Origin, 0.001, Direction.xyz, 10000.0, 0);
  
      // Accumulate sample result
      Color_Sum += Payload.rgb;
      Depth_Sum += Payload.a;
    }
  
    // Average samples - on NVIDIA, the compiler fuses this into a single FMA chain
    vec3  Final_Color = Color_Sum * (1.0 / float (SPP));
    float Final_Depth = Depth_Sum * (1.0 / float (SPP));
  
    // Write final color and depth to storage images (Px is remapped for checkerboard)
    imageStore (Storage_Image, Px, vec4 (Final_Color, 1.0));
    imageStore (Depth_Output,  Px, vec4 (Final_Depth, 0.0, 0.0, 0.0));
  }
}

// ═══════════════
//   Closest_Hit 
// ═══════════════

glsl rchit Closest_Hit {
  #version 460
  #extension GL_EXT_ray_tracing : require
  #extension GL_EXT_ray_query : require
  #extension GL_EXT_nonuniform_qualifier : require
  
  layout(binding = 0) uniform accelerationStructureEXT Top_Level;
  layout(binding = 2) uniform Camera_Uniform {
    mat4  Inverse_View; mat4 Inverse_Projection;
    uint  Frame; uint Weapon_Texture_Base; uint PBR_Stride; uint Active_SPP;
    vec4  Env_Sun_Dir;      // xyz = direction, w = angular_radius
    vec4  Env_Sun_Color;    // xyz = color, w = intensity
    vec4  Env_Sky_Zenith;   // xyz = zenith color, w = sky_intensity
    vec4  Env_Sky_Horizon;  // xyz = horizon color, w = cos(sun_disc_size)
    vec4  Env_Ambient_Up;   // xyz = ambient up, w = sun_disc_intensity
    vec4  Env_Ambient_Down; // xyz = ambient down, w = fog_density
    vec4  Env_Fog_Color;    // xyz = fog color
  };
  
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
  
  // Entity geometry
  layout(binding = 12, std430) readonly buffer Entity_Vertex_Data { vec4 Data[]; } Entity_Vertices;
  layout(binding = 13, std430) readonly buffer Entity_Index_Data  { uint Data[]; } Entity_Indices;
  layout(binding = 14, std430) readonly buffer Entity_Tex_Id_Data { uint Data[]; } Entity_Tex_Ids;
  
  // Bindless texture array (binding 15: must be highest for variable descriptor count)
  layout(binding = 15) uniform sampler2D Textures[];
  
  layout(location = 0) rayPayloadInEXT vec4 Payload;  // rgb = color, a = hit distance

  // Shadow rays now use inline rayQueryEXT - no payload needed (saves continuation stack)
  hitAttributeEXT vec2 Barycentrics;
  
  // PCG hash for stochastic effects (soft shadows, importance sampling)
  uint PCG (uint V) {
    uint S = V * 747796405u + 2891336453u;
    uint W = ((S >> ((S >> 28u) + 4u)) ^ S) * 277803737u;
    return (W >> 22u) ^ W;
  }
  
  // Soft shadow ray direction (extracted to avoid code duplication)
  vec3 Soft_Shadow_Dir (vec3 Ld, uint Prim, uint Inst, uint Frame, float Disk_Radius) {
    uint Seed  = PCG (Prim * 1973u + Inst * 9277u + Frame * 26699u);
    float Ang  = float (Seed) * 2.3283064e-10 * 6.2831853;
    float Rad  = sqrt (float (PCG (Seed)) * 2.3283064e-10) * Disk_Radius;
    vec3 Light_Tangent   = (abs (Ld.y) < 0.99) ? normalize (cross (Ld, vec3 (0, 1, 0)))
                                               : normalize (cross (Ld, vec3 (1, 0, 0)));
    vec3 Light_Bitangent = cross (Ld, Light_Tangent);
    return normalize (Ld + Light_Tangent * (cos (Ang) * Rad) + Light_Bitangent * (sin (Ang) * Rad));
  }
  
  // Extracted shadow trace (deduplicated from entity and world paths)
  float Trace_Shadow (vec3 Origin, vec3 Normal, vec3 Ld, uint Prim, uint Inst, uint Frame, float Disk_Radius) {
    vec3 Shadow_Dir = Soft_Shadow_Dir (Ld, Prim, Inst, Frame, Disk_Radius);
    rayQueryEXT Shadow_Query;
    rayQueryInitializeEXT (Shadow_Query, Top_Level,
      gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT,
      0xFE, Origin + Normal * 0.1, 0.001, Shadow_Dir, 600.0);
    rayQueryProceedEXT (Shadow_Query);
    return (rayQueryGetIntersectionTypeEXT (Shadow_Query, true)
      == gl_RayQueryCommittedIntersectionNoneEXT) ? 1.0 : 0.05;
  }
  
  // Read a vertex attribute (48 bytes = 12 floats per vertex) from the appropriate buffer
  vec4 Read_Raw (uint I, uint Slot, uint Instance) {
    if (Instance == 2u) return Entity_Vertices.Data[I * 3 + Slot];
    if (Instance == 1u) return Weapon_Vertices.Data[I * 3 + Slot];
    return Vertices.Data[I * 3 + Slot];
  }
  vec3 Read_Position   (uint I, uint Inst) { return Read_Raw (I, 0, Inst).xyz; }
  vec2 Read_Tex_Uv     (uint I, uint Inst) { return Read_Raw (I, 1, Inst).xy;  }
  vec2 Read_Lightmap_Uv(uint I, uint Inst) { return Read_Raw (I, 1, Inst).zw;  }
  vec3 Read_Normal     (uint I, uint Inst) { return Read_Raw (I, 2, Inst).xyz; }
  
  void main () {
    // Determine which instance we hit: 0 = world, 1 = weapon, 2 = entity
    uint  Instance     = gl_InstanceCustomIndexEXT;
    bool  Is_Weapon    = (Instance == 1u);
    bool  Is_Entity     = (Instance == 2u);
    uint  Primitive    = gl_PrimitiveID;
  
    // Adaptive quality budget: 0.0 = full quality (60fps+), 1.0 = minimal work (< 5fps)
    float Budget = float ((Active_SPP >> 8u) & 0xFFu) / 255.0;
  
    // Fetch triangle vertex indices from the appropriate buffer
    uint I0, I1, I2;
    if (Is_Entity) {
      I0 = Entity_Indices.Data[Primitive * 3 + 0];
      I1 = Entity_Indices.Data[Primitive * 3 + 1];
      I2 = Entity_Indices.Data[Primitive * 3 + 2];
    } else if (Is_Weapon) {
      I0 = Weapon_Indices.Data[Primitive * 3 + 0];
      I1 = Weapon_Indices.Data[Primitive * 3 + 1];
      I2 = Weapon_Indices.Data[Primitive * 3 + 2];
    } else {
      I0 = Indices.Data[Primitive * 3 + 0];
      I1 = Indices.Data[Primitive * 3 + 1];
      I2 = Indices.Data[Primitive * 3 + 2];
    }
  
    // Batched vertex attribute reads
    vec3 Bary   = vec3 (1.0 - Barycentrics.x - Barycentrics.y, Barycentrics.x, Barycentrics.y);
    vec4 V0_S0  = Read_Raw (I0, 0, Instance), V0_S1 = Read_Raw (I0, 1, Instance), V0_S2 = Read_Raw (I0, 2, Instance);
    vec4 V1_S0  = Read_Raw (I1, 0, Instance), V1_S1 = Read_Raw (I1, 1, Instance), V1_S2 = Read_Raw (I1, 2, Instance);
    vec4 V2_S0  = Read_Raw (I2, 0, Instance), V2_S1 = Read_Raw (I2, 1, Instance), V2_S2 = Read_Raw (I2, 2, Instance);
    vec3 Position  = V0_S0.xyz * Bary.x + V1_S0.xyz * Bary.y + V2_S0.xyz * Bary.z;
    vec2 Tex_Coord = V0_S1.xy  * Bary.x + V1_S1.xy  * Bary.y + V2_S1.xy  * Bary.z;
    vec2 Lightmap_Coordinate  = V0_S1.zw  * Bary.x + V1_S1.zw  * Bary.y + V2_S1.zw  * Bary.z;
    vec3 Normal    = normalize (V0_S2.xyz * Bary.x + V1_S2.xyz * Bary.y + V2_S2.xyz * Bary.z);
  
    // Fetch the texture ID for this triangle and sample the albedo
    uint Tex_Id;
    if (Is_Entity) Tex_Id = Entity_Tex_Ids.Data[Primitive];
    else if (Is_Weapon) Tex_Id = Weapon_Tex_Ids.Data[Primitive] + Weapon_Texture_Base;
    else Tex_Id = Texture_Ids.Data[Primitive];
  
    // Build tangent frame (Frisvad method) for normal mapping and parallax
    vec3 Geo_Normal = Normal; // Preserve geometric normal for parallax
    vec3 T_Axis, B_Axis;
    if (Geo_Normal.z < -0.999) {
      T_Axis = vec3 (0.0, -1.0, 0.0);
      B_Axis = vec3 (-1.0, 0.0, 0.0);
    } else {
      float Inv = 1.0 / (1.0 + Geo_Normal.z);
      T_Axis = vec3 (1.0 - Geo_Normal.x * Geo_Normal.x * Inv, -Geo_Normal.x * Geo_Normal.y * Inv, -Geo_Normal.x);
      B_Axis = vec3 (-Geo_Normal.x * Geo_Normal.y * Inv, 1.0 - Geo_Normal.y * Geo_Normal.y * Inv, -Geo_Normal.y);
    }
  
    // Parallax occlusion mapping from height map
    vec3  V  = -gl_WorldRayDirectionEXT;
    float Hit_Dist = gl_HitTEXT;

    // Adaptive parallax: 300u at full quality, 0u at Budget=1 (pure lightmap fallback).
    float Parallax_Dist = 300.0 * (1.0 - Budget);
    if (not Is_Weapon and not Is_Entity and Tex_Id < PBR_Stride and Hit_Dist < Parallax_Dist) {

      // Transform view to tangent space for parallax ray marching
      vec3 V_Tangent = vec3 (dot (V, T_Axis), dot (V, B_Axis), dot (V, Geo_Normal));

      // Scale parallax depth: 0.03 units - subtle but visible on close surfaces
      vec2 Parallax_Dir = V_Tangent.xy / max (V_Tangent.z, 0.1) * 0.03;
  
      // Hybrid linear-binary parallax 
      uint Height_Tex = nonuniformEXT(Tex_Id + PBR_Stride * 5u);
      float Layer_Depth = 1.0 / 4.0;  // 4 coarse steps
      float Current_Depth = 0.0;
      vec2  Current_Uv = Tex_Coord;
      vec2  Uv_Step = -Parallax_Dir * Layer_Depth;
      float H_Sample = textureLod (Textures[Height_Tex], Current_Uv, 0.0).r;
  
      // Phase 1: 4 coarse linear steps to find the crossing interval
      for (int Step = 0; Step < 4; Step++) {
        Current_Uv += Uv_Step;
        H_Sample = textureLod (Textures[Height_Tex], Current_Uv, 0.0).r;
        Current_Depth += Layer_Depth;
      }
  
      // Phase 2: 3 binary search steps to refine within the crossing interval
      vec2 Lo_Uv = Current_Uv - Uv_Step;  float Lo_D = Current_Depth - Layer_Depth;
      vec2 Hi_Uv = Current_Uv;             float Hi_D = Current_Depth;
      for (int B = 0; B < 3; B++) {
        vec2  Mid_Uv = (Lo_Uv + Hi_Uv) * 0.5;
        float Mid_D  = (Lo_D + Hi_D) * 0.5;
        float Mid_H  = textureLod (Textures[Height_Tex], Mid_Uv, 0.0).r;
        if (Mid_D < Mid_H) { Lo_Uv = Mid_Uv; Lo_D = Mid_D; }
        else                { Hi_Uv = Mid_Uv; Hi_D = Mid_D; }
      }
      Tex_Coord = Hi_Uv;
    }
  
    // Sample PBR texture maps
    vec3  Albedo     = textureLod (Textures[nonuniformEXT(Tex_Id)], Tex_Coord, 0.0).rgb;
    vec3  Normal_Map = vec3 (0.0, 0.0, 1.0);
    float R = 0.5;
    float M = 0.0;
    vec3  Emissive  = vec3 (0.0);
  
    // Sample PBR maps for world geometry, entities, and weapon
    float PBR_Dist = mix (1000.0, 200.0, Budget);
    if (not Is_Weapon and Tex_Id < PBR_Stride) {

      // World / Entity PBR: maps laid out at [diffuse_0..N, normal_0..N, roughness_0..N, ...]
      if (Hit_Dist < PBR_Dist) {
        Normal_Map = textureLod (Textures[nonuniformEXT(Tex_Id + PBR_Stride)],      Tex_Coord, 0.0).rgb * 2.0 - 1.0;
        R          = textureLod (Textures[nonuniformEXT(Tex_Id + PBR_Stride * 2u)], Tex_Coord, 0.0).r;
        M          = textureLod (Textures[nonuniformEXT(Tex_Id + PBR_Stride * 3u)], Tex_Coord, 0.0).r;
      }
      Emissive   = textureLod (Textures[nonuniformEXT(Tex_Id + PBR_Stride * 4u)], Tex_Coord, 0.0).rgb;

    // Weapon PBR: 6 map types × 2 surfaces, stride = 2
    // Eliminates 2 local variables and 4 subtractions per invocation ???
    } else if (Is_Weapon) {
      Normal_Map = textureLod (Textures[nonuniformEXT(Tex_Id + 2u)],  Tex_Coord, 0.0).rgb * 2.0 - 1.0;
      R          = textureLod (Textures[nonuniformEXT(Tex_Id + 4u)],  Tex_Coord, 0.0).r;
      M          = textureLod (Textures[nonuniformEXT(Tex_Id + 6u)],  Tex_Coord, 0.0).r;
      Emissive   = textureLod (Textures[nonuniformEXT(Tex_Id + 8u)],  Tex_Coord, 0.0).rgb;

    // Fallback for textures outside the PBR material range: derive from albedo statistics
    } else {
      float Lu = dot (Albedo, vec3 (0.2126, 0.7152, 0.0722));
      float Hi = max (Albedo.r, max (Albedo.g, Albedo.b));
      float Sa = (Hi - min (Albedo.r, min (Albedo.g, Albedo.b))) / max (Hi, 1e-3);
      R = mix (0.60, 0.90, 1.0 - Lu);   // Rougher default - less "wet" look on stone/brick
      M = smoothstep (0.35, 0.15, Sa) * smoothstep (0.45, 0.2, Lu) * 0.4;
    }
  
    // Normal mapping: always apply TBN transform
    Normal = normalize (T_Axis * Normal_Map.x + B_Axis * Normal_Map.y + Geo_Normal * Normal_Map.z);
  
    // PBR material parameters
    vec3  F0 = mix (vec3 (0.04), Albedo, M);
    float NV = max (dot (Normal, V), 1e-3);
  
    // Direct lighting: Cook-Torrance microfacet BRDF
    vec3  Ld = normalize (Env_Sun_Dir.xyz);         // Per-scene sun direction
    vec3  Lr = Env_Sun_Color.xyz * Env_Sun_Color.w; // Per-scene sun radiance (color × intensity)
    float NL  = max (dot (Normal, Ld), 0.0);
  
    // Skip full BRDF when surface faces away from sun
    vec3  Specular = vec3 (0.0);
    vec3  Diffuse  = vec3 (0.0);
    if (NL > 0.0) {
      vec3  H   = normalize (V + Ld);
      float NH  = max (dot (Normal, H),  0.0);
      float VH  = max (dot (V, H),       0.0);
      float a   = R * R,  a2 = max (a * a, 1e-4); // Clamp to avoid NaN at R=0
      float k   = (R + 1.0) * (R + 1.0) * 0.125;
  
      // GGX/Trowbridge-Reitz normal distribution
      float Denom = NH * NH * (a2 - 1.0) + 1.0;
      float D     = a2 / (3.14159 * Denom * Denom);
  
      // Smith geometry (height-correlated visibility)
      float G1_L = NL / (NL * (1.0 - k) + k);
      float G1_V = NV / (NV * (1.0 - k) + k);
      float Vis  = G1_L * G1_V / max (4.0 * NL * NV, 1e-4);
  
      // Schlick Fresnel
      float FT  = 1.0 - VH;  float T5 = FT * FT; T5 *= T5 * FT;
      vec3  F   = F0 + (1.0 - F0) * T5;
  
      // Final specular and diffuse terms
      Specular = D * Vis * F;
      Diffuse  = (1.0 - F) * (1.0 - M) * Albedo * 0.31831;  // 1/π
    }
  
    // Hemisphere ambient diffuse 
    vec3  Sky_Color    = Env_Ambient_Up.xyz;   // Per-scene ambient from above (sky contribution)
    vec3  Ground_Color = Env_Ambient_Down.xyz; // Per-scene ambient from below (ground bounce)
    float Hemisphere   = Normal.y * 0.5 + 0.5; // Zero is down, one up
    vec3  Ambient_Irradiance = mix (Ground_Color, Sky_Color, Hemisphere);
  
    // Ambient specular: pre-integrated split-sum approximation (Karis 2013)
    // Approximates ∫(BRDF * Li) with env BRDF LUT replaced by analytic fit
    float Env_FT   = 1.0 - NV;  float Env_T5 = Env_FT * Env_FT; Env_T5 *= Env_T5 * Env_FT;
    vec3  Env_F    = F0 + (max (vec3 (1.0 - R), F0) - F0) * Env_T5;     // Roughness-aware Fresnel
    vec2  Env_BRDF = vec2 (1.0 - R * 0.5, R * 0.08);  // Analytic fit to DFG LUT
    vec3  Ambient_Specular = Env_F * Env_BRDF.x + Env_BRDF.y;
  
    // Reflect the view vector off the surface to tint specular with sky direction
    vec3  Reflect_Dir = reflect (-V, Normal);
    float Refl_Up     = Reflect_Dir.y * 0.5 + 0.5;
    vec3  Env_Color   = mix (Ground_Color, Sky_Color * 1.2, Refl_Up); // Restrained - real reflections do the heavy lifting
    vec3  Indirect_Specular = Ambient_Specular * Env_Color;
  
    // Indirect diffuse: hemisphere ambient weighted by (1 - metallic) since metals have no diffuse
    vec3 Indirect_Diffuse = Ambient_Irradiance * Albedo * (1.0 - M) * (1.0 - Env_F);
    
    // Reflection-bounce hits detect this and skip tracing another reflection to limit recursion to exactly one bounce
    bool Is_Reflection_Bounce = (gl_RayTminEXT > 0.005);

    // Reflection culling
    bool  Refl_Active = not Is_Reflection_Bounce and (R < 0.55);
    float Refl_Dist = mix (800.0, 300.0, Budget);  // Adaptive reflection distance
    float Reflection_Weight = (not Refl_Active or Hit_Dist > Refl_Dist)
                                ? 0.0
                                : max (max (Env_F.r, Env_F.g), Env_F.b) * (1.0 - R * R);
    vec3  Reflection_Color  = vec3 (0.0);
  
    // Trace reflection ray for smooth/metallic surfaces
    if (Reflection_Weight > 0.10) {
      vec3 Refl_Dir = reflect (-V, Normal);
      Payload = vec4 (0.0, 0.0, 0.0, -1.0);
      traceRayEXT (Top_Level, gl_RayFlagsOpaqueEXT,
                   0xFF,
                   0, 1, 0,
                   Position + Normal * 0.2,
                   0.01,  // tmin=0.01 marks this as a reflection bounce
                   Refl_Dir,
                   mix (500.0, 200.0, Budget),  // Adaptive tmax - shorter trace at low fps
                   0);
      Reflection_Color = Payload.rgb;
    }
  
    // Blend reflection into indirect specular: replace the hemisphere approximation
    // with actual traced reflection, weighted by the Fresnel term.
    vec3 Traced_Specular = Env_F * mix (Indirect_Specular / max (Env_F, vec3(0.01)),
                                         Reflection_Color,
                                         vec3 (Reflection_Weight));
  
    // Compute final shading based on instance type
    vec3 Color;
    float Shadow_Dist = mix (600.0, 200.0, Budget); // Adaptive shadow ray cutoff distance
  
    // Apply per-instance lighting model
    if (Is_Weapon) {
      vec3 Direct    = (Diffuse + Specular) * Lr * NL;
      vec3 Weapon_Full  = Direct * 0.9 + (Indirect_Diffuse + Traced_Specular) * 1.5;
      vec3 Weapon_Cheap = Ambient_Irradiance * Albedo * 2.5 + Albedo * max(NL, 0.3);
      Color = mix (Weapon_Full, Weapon_Cheap, Budget);

    // Entity: direct sun + shadows + hemisphere ambient, no lightmap (MD3 models have no LM UVs)
    } else if (Is_Entity) {
      float Shadow_Factor = (NL > 0.0 and not Is_Reflection_Bounce and Hit_Dist < Shadow_Dist)
                              ? Trace_Shadow (Position, Normal, Ld, Primitive, Instance, Frame, Env_Sun_Dir.w)
                              : 1.0;
      vec3 Direct = (Diffuse + Specular) * Lr * NL * Shadow_Factor;

      // Entity color: balanced ambient - not too bright (avoids glow), not too dark (avoids silhouette)
      vec3 Entity_Full  = Direct + Indirect_Diffuse * 0.8 + Traced_Specular;
      vec3 Entity_Cheap = Ambient_Irradiance * Albedo * 0.9 + Albedo * max(NL, 0.2) * 0.4;
      Color = mix (Entity_Full, Entity_Cheap, Budget);

      // Subtle saturation boost for entities - counteracts ambient washout on character models
      float Entity_Luminance = dot (Color, vec3 (0.2126, 0.7152, 0.0722));
      Color = mix (vec3 (Entity_Luminance), Color, 1.15);  // 15% saturation increase
    } else {
      vec3 Lightmap_Color = textureLod (Lightmap, Lightmap_Coordinate, 0.0).rgb * 4.0;  // Lightmap auto-linearized via SRGB format
  
      // Inline ray query for shadows
      float Shadow_Factor = (NL > 0.0 and not Is_Reflection_Bounce and Hit_Dist < Shadow_Dist)
        ? Trace_Shadow (Position, Normal, Ld, Primitive, Instance, Frame, Env_Sun_Dir.w) : 1.0;
  
      // Dual-path rendering with Budget blend
      vec3 Direct = (Diffuse + Specular) * Lr * NL * Shadow_Factor;
      vec3 Baked_GI = Albedo * Lightmap_Color * (1.0 - M);
  
      // Cheap path: just lightmap + emissive (minimal ambient to keep shadows dark)
      vec3 Cheap = Baked_GI + Ambient_Irradiance * Albedo * 0.15 + Emissive * 5.0;
  
      // Full path: complete PBR lighting (direct at full strength for strong light/shadow contrast)
      vec3 Full = Baked_GI + Direct + Indirect_Diffuse + Traced_Specular + Emissive * 5.0;
  
      // Blend: Budget is the knob. 0 = full raytraced, 1 = pure lightmap.
      Color = mix (Full, Cheap, Budget);
  
      // Dynamic environment fog
      if (not Is_Reflection_Bounce) {
        float Fog_Distance  = gl_HitTEXT;

        // Per-scene exponential fog - adds depth to corridors
        float Fog_Amount    = 1.0 - exp (-Fog_Distance * Env_Ambient_Down.w);
        vec3  Fog_Color     = Env_Fog_Color.xyz;  // Per-scene fog color
        Color = mix (Color, Fog_Color, Fog_Amount);
      }
    }
  
    // Output shaded color and hit distance
    Payload = vec4 (Color, gl_HitTEXT);
  }
} // Closest_Hit
  
// ════════════
//   Ray_Miss 
// ════════════

glsl rmiss Ray_Miss {
  #version 460
  #extension GL_EXT_ray_tracing : require
  
  layout(location = 0) rayPayloadInEXT vec4 Payload;
  layout(binding = 2) uniform Camera_Uniform {
    mat4  Inverse_View; mat4 Inverse_Projection;
    uint  Frame; uint Weapon_Texture_Base; uint PBR_Stride; uint Active_SPP;
    vec4  Env_Sun_Dir;      // xyz = direction, w = angular_radius
    vec4  Env_Sun_Color;    // xyz = color, w = intensity
    vec4  Env_Sky_Zenith;   // xyz = zenith color, w = sky_intensity
    vec4  Env_Sky_Horizon;  // xyz = horizon color, w = cos(sun_disc_size)
    vec4  Env_Ambient_Up;   // xyz = ambient up, w = sun_disc_intensity
    vec4  Env_Ambient_Down; // xyz = ambient down, w = fog_density
    vec4  Env_Fog_Color;    // xyz = fog color
  };
  
  // Ray_Miss shader main
  void main () {

    // Per-scene procedural sky with sun disc
    vec3  Dir      = gl_WorldRayDirectionEXT;
    float Vertical = max (Dir.y, 0.0);
    vec3  Sky      = mix (Env_Sky_Horizon.xyz, Env_Sky_Zenith.xyz, sqrt (Vertical));
    Sky *= Env_Sky_Zenith.w;  // Sky intensity multiplier
  
    // Sun disc - bright spot in the sky at sun direction
    float Sun_Cos   = dot (Dir, Env_Sun_Dir.xyz);

    // Cos(disc_size) pre-computed on CPU - eliminates per-pixel transcendental
    float Disc_Edge = Env_Sky_Horizon.w;  // Already cos(sun_disc_size)
    float Sun_Edge  = clamp ((Sun_Cos - Disc_Edge) / (1.0 - Disc_Edge), 0.0, 1.0);
    float Sun_Glow  = Sun_Edge * Sun_Edge; Sun_Glow *= Sun_Glow;  // x^4
    Sky += Env_Sun_Color.xyz * Sun_Glow * Env_Ambient_Up.w;  // sun_disc_intensity
  
    // Atmospheric haze near horizon (Mie-like forward scattering glow)
    float Horizon_Glow = exp (-Vertical * 8.0);  // Concentrated near horizon
    vec2  Dir_XZ = vec2 (Dir.x, Dir.z);
    vec2  Sun_XZ = vec2 (Env_Sun_Dir.x, Env_Sun_Dir.z);
    float Sun_Horizon = max (dot (Dir_XZ, Sun_XZ) * inversesqrt (
      max (dot (Dir_XZ, Dir_XZ) * dot (Sun_XZ, Sun_XZ), 1e-12)), 0.0);

    // Pow(x,8) > ((x²)²)² (3 muls vs transcendental)
    float SH2 = Sun_Horizon * Sun_Horizon; float SH4 = SH2 * SH2; float SH8 = SH4 * SH4;
    Sky += Env_Sun_Color.xyz * Horizon_Glow * SH8 * 0.3;
  
    // Output sky color at maximum distance
    Payload = vec4 (Sky, 10000.0);  // Sky = max distance
  }
} // Ray_Miss
  
// ═══════════════
//   Shadow_Miss 
// ═══════════════

glsl rmiss Shadow_Miss {
  #version 460
  #extension GL_EXT_ray_tracing : require
  
  layout(location = 1) rayPayloadInEXT float Shadow_Factor;
  
  // Shadow_Miss main
  void main () {

    // Shadow ray reached the light source without hitting anything - fully lit
    Shadow_Factor = 1.0;
  }
}

// ═══════════
//   Physics
// ═══════════

glsl comp Physics {
  #version 460
  #extension GL_EXT_ray_tracing : require
  #extension GL_EXT_ray_query : require
  
  // Descriptor bindings
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
  
  struct Gpu_Projectile {
    vec3  Position;      float Pad_A;
    vec3  Velocity;      float Lifetime;
    int   Active;        int   Material_Hit;
    float Radius;        float Damage;
    float Hit_U, Hit_V;  // UV at impact point (for CPU-side damage map lookup)
    int   Instance_Hit;  // TLAS instance index of the object hit (-1 = none)
    int   Pad_B;
  };
  
  layout(binding = 5, std430) buffer Projectile_Buffer {
    Gpu_Projectile Projectiles[64];
    int   Projectile_Count;
    float Fire_Cooldown;
    float Proj_Pad[2];
  };
  
  layout(push_constant) uniform Push {
    int   Forward, Back, Left, Right;
    int   Jump, Fire, Crouch, Pad;
    float Delta_X, Delta_Y, Dt, Pad2;
  } Input;
  
  layout(local_size_x = 1) in;
  
  // Physics constants
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
  
  // Collider shape constants
  const int SHAPE_SPHERE    = 0;
  const int SHAPE_CAPSULE   = 1;
  const int SHAPE_AABB      = 2;
  const int SHAPE_CYLINDER  = 3;
  const int SHAPE_ELLIPSOID = 4;
  const int SHAPE_HULL      = 5;
  
  // Convex hull support functions
  
  // Brute-force: O(n) linear scan over all hull vertices. Best for small hulls (< 64 verts)
  vec3 hull_support_brute (vec3 direction) {
    float best_dot = -1e30;
    int   best_idx = 0;
    for (int i = 0; i < Hull_Count; i++) {
      float d = dot (Hull_Vertices[i].xyz, direction);
      if (d > best_dot) { best_dot = d; best_idx = i; }
    }
    return Hull_Vertices[best_idx].xyz;
  }
  
  // Hill-climbing: O(sqrt(n)) amortized using per-vertex adjacency table
  vec3 hull_support_hill (vec3 direction) {
    int current = 0;
    float current_dot = dot (Hull_Vertices[0].xyz, direction);
  
    // Walk neighbors until no improvement is found
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
  
  // Ray trace helper
  struct Trace_Result {
    float Fraction;
    vec3  Normal;
    bool  Hit;
  };
  
  // Cast a swept shape from Origin along Direction for up to Distance units
  Trace_Result trace_shape (vec3 Origin, vec3 Direction, float Distance) {
    Trace_Result result;
    result.Fraction = 1.0;
    result.Normal   = vec3 (0.0, 1.0, 0.0);
    result.Hit      = false;

    // Skip zero-length traces
    if (Distance < 1e-6) return result;
    vec3 dir_norm = normalize (Direction);
  
    // 7 probe directions: 6 cardinal axes + movement direction (optimized from 28 for performance)
    vec3 probes[7] = vec3[7](
      vec3( 1, 0, 0), vec3(-1, 0, 0), vec3(0, 1, 0), vec3(0,-1, 0), vec3(0, 0, 1), vec3(0, 0,-1),
      dir_norm
    );

    // Test each probe direction for collisions
    for (int i = 0; i < 7; i++) {
      vec3 offset = shape_offset (normalize(probes[i]));
      vec3 ray_origin = Origin + offset;
  
      // Initialize and execute the ray query
      rayQueryEXT rq;
      rayQueryInitializeEXT (rq, Top_Level, gl_RayFlagsOpaqueEXT, 0xFF,
                             ray_origin, 0.0, dir_norm, Distance);
  
      // Process all traversal steps
      while (rayQueryProceedEXT (rq)) {}
  
      // Record hit if closer than previous best
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
  
          // Update result with this closer hit
          result.Fraction = t / Distance;
          result.Normal   = n;
          result.Hit      = true;
        }
      }
    }
  
    // Return result
    return result;

  } // trace_shape
  
  // Cast a short ray downward to detect ground contact
  void ground_trace () {
    vec3 down_offset = shape_offset (vec3 (0, -1, 0));
    vec3 origin      = Player.Position + down_offset;
    float dist       = 0.5;

    // Cast a short downward ray query
    rayQueryEXT rq;
    rayQueryInitializeEXT (rq, Top_Level, gl_RayFlagsOpaqueEXT, 0xFF,
                           origin, 0.0, vec3 (0, -1, 0), dist);
    while (rayQueryProceedEXT (rq)) {}

    // Classify surface as walkable ground or steep slope
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

      // Set ground state based on surface steepness
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
  
  // Clip velocity
  vec3 clip_velocity (vec3 vel, vec3 normal) {
    float backoff = dot (vel, normal) * OVERBOUNCE;
    return vel - normal * backoff;
  }
  
  // Slide move
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
    for (int bump = 0; bump < 4 and time_left > 0.001; bump++) {
      vec3  move_dir  = vel * time_left;
      float move_dist = length (move_dir);
      if (move_dist < 0.001) break;

      // Trace movement against world geometry
      Trace_Result trace = trace_shape (Player.Position, move_dir, move_dist);

      // Advance position by the free distance before collision
      if (trace.Fraction > 0.0)
        Player.Position += normalize(move_dir) * move_dist * trace.Fraction;

      // Exit if no collision occurred
      if (not trace.Hit) break;

      // Reduce remaining time by the fraction traveled
      time_left *= (1.0 - trace.Fraction);

      // Avoid duplicating a plane we've already clipped against
      bool duplicate = false;
      for (int p = 0; p < plane_count; p++)
        if (dot (trace.Normal, planes[p]) > 0.99) { duplicate = true; break; }
      if (duplicate) continue;

      // Add new plane to the clip set
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
          if (q == p or dot (vel, planes[q]) >= 0.0) continue;
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

    // Store the final clipped velocity
    Player.Velocity = vel;
  }

  // Step move
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
    if (down.Hit and down.Normal.y >= MINIMUM_WALK_NORMAL) {
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
  
  // Stuck recovery
  void recover () {
    // Cast rays in 6 cardinal directions and nudge the player away from walls
    vec3 dirs[6] = vec3[6](
      vec3(1,0,0), vec3(-1,0,0), vec3(0,1,0), vec3(0,-1,0), vec3(0,0,1), vec3(0,0,-1));

    // Probe each direction and push out of intersecting geometry
    for (int i = 0; i < 6; i++) {
      vec3 offset  = shape_offset (dirs[i]);
      float expect = length (offset);

      // Trace ray in this direction to detect overlap
      rayQueryEXT rq;
      rayQueryInitializeEXT (rq, Top_Level, gl_RayFlagsOpaqueEXT, 0xFF,
                             Player.Position, 0.0, dirs[i], expect);
      while (rayQueryProceedEXT (rq)) {}

      // Nudge player away if inside geometry
      if (rayQueryGetIntersectionTypeEXT (rq, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {
        float t = rayQueryGetIntersectionTEXT (rq, true);
        if (t < expect) {
          float penetration = expect - t;
          Player.Position -= dirs[i] * (penetration + 0.125);
        }
      }
    }
  }
  
  // Physics shader main
  void main () {
  
    // Mouse look
    Player.Yaw   -= Input.Delta_X * MOUSE_SENSITIVITY;
    Player.Pitch -= Input.Delta_Y * MOUSE_SENSITIVITY;
    Player.Pitch  = clamp (Player.Pitch, -1.5, 1.5);
  
    // Build a movement basis from yaw (must match camera: Forward = (sy, 0, -cy))
    float cy = cos (Player.Yaw), sy = sin (Player.Yaw);
    vec3 forward = vec3 ( sy, 0, -cy);
    vec3 right   = vec3 ( cy, 0,  sy);
  
    // Compute the wish direction and speed from keyboard input
    vec3 wish = vec3 (0.0);
    if (Input.Forward == 1) wish += forward;
    if (Input.Back    == 1) wish -= forward;
    if (Input.Right   == 1) wish += right;
    if (Input.Left    == 1) wish -= right;
    float wish_speed = MAXIMUM_SPEED;
    if (length (wish) > 0.001) wish = normalize (wish); else wish_speed = 0.0;
  
    // Crouch handling
    float target_view = DEFAULT_VIEW_HEIGHT;
    if (Input.Crouch == 1) {
      Player.Ducked = 1;
      target_view = CROUCH_VIEW_HEIGHT;
    } else {
      Player.Ducked = 0;
    }
  
    // Ground trace
    ground_trace ();
  
    // Apply ground or air movement
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
      if (Input.Jump == 1 and Player.Jump_Held == 0) {
        Player.Velocity.y = JUMP_VELOCITY;
        Player.On_Ground  = 0;
      }

    // Air acceleration (enables strafe-jumping)
    } else {
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
  
    // Move and collide
    if (Player.On_Ground == 1)
      step_move ();
    else
      slide_move ();
  
    // Stuck recovery
    recover ();
  
    // Re-check ground after movement
    ground_trace ();
  
    // Smoothly interpolate the view height toward the target
    float delta = target_view - Player.View_Height;
    if (abs(delta) < 0.1) Player.View_Height = target_view;
    else Player.View_Height += delta * min (Input.Dt * 10.0, 1.0);

    // Record horizontal speed for HUD/diagnostics
    Player.Speed_Last = length (Player.Velocity.xz);
  
    // Projectile update: decrement fire cooldown
    if (Fire_Cooldown > 0.0) Fire_Cooldown -= Input.Dt;
  
    // Spawn a new projectile on fire button press
    if (Input.Fire == 1 and Fire_Cooldown <= 0.0 and Projectile_Count < 64) {
      vec3 cam_forward = vec3 (sy, -sin(Player.Pitch), -cy * cos(Player.Pitch));
      cam_forward = normalize (cam_forward);
      vec3 eye = Player.Position + vec3 (0.0, Player.View_Height, 0.0);

      // Initialize the new projectile
      int idx = Projectile_Count;
      Projectiles[idx].Position     = eye + cam_forward * 20.0;
      Projectiles[idx].Velocity     = cam_forward * 900.0;
      Projectiles[idx].Lifetime     = 10.0;
      Projectiles[idx].Active       = 1;
      Projectiles[idx].Material_Hit = 0;
      Projectiles[idx].Radius       = 3.0;
      Projectiles[idx].Damage       = 100.0;
      Projectiles[idx].Hit_U        = 0.0;
      Projectiles[idx].Hit_V        = 0.0;
      Projectiles[idx].Instance_Hit = -1;
      Projectile_Count = idx + 1;
      Fire_Cooldown = 0.8;
    }
  
    // Advance each active projectile: move, trace against TLAS, kill on impact or timeout
    for (int i = 0; i < Projectile_Count; i++) {
      if (Projectiles[i].Active == 0) continue;

      // Decrement lifetime and kill expired projectiles
      Projectiles[i].Lifetime -= Input.Dt;
      if (Projectiles[i].Lifetime <= 0.0) { Projectiles[i].Active = 0; continue; }

      // Compute movement direction and distance for this timestep
      vec3 dir = normalize (Projectiles[i].Velocity);
      float dist = length (Projectiles[i].Velocity) * Input.Dt;
  
      // Ray trace to check for collision
      rayQueryEXT rq;
      rayQueryInitializeEXT (rq, Top_Level, gl_RayFlagsOpaqueEXT, 0xFF,
                             Projectiles[i].Position, 0.0, dir, dist);
      while (rayQueryProceedEXT (rq)) {}

      // Check if the projectile hit geometry
      if (rayQueryGetIntersectionTypeEXT (rq, true) == gl_RayQueryCommittedIntersectionTriangleEXT) {

        // Hit something - mark dead and record hit position + UV for damage map lookup
        float t = rayQueryGetIntersectionTEXT (rq, true);
        Projectiles[i].Position += dir * t;
        Projectiles[i].Active = 0;
  
        // Extract the instance index to identify what was hit (world vs player model)
        Projectiles[i].Instance_Hit = rayQueryGetIntersectionInstanceCustomIndexEXT (rq, true);
  
        // Extract barycentrics and primitive index to compute the hit UV
        vec2 bary = rayQueryGetIntersectionBarycentricsEXT (rq, true);
        uint prim = rayQueryGetIntersectionPrimitiveIndexEXT (rq, true);
  
        // Look up the three vertex indices for the hit triangle
        uint i0 = Indices.Data[prim * 3 + 0];
        uint i1 = Indices.Data[prim * 3 + 1];
        uint i2 = Indices.Data[prim * 3 + 2];
  
        // Read texture UVs from vertex data (vec4[1].xy = texture UV)
        vec2 uv0 = Vertices.Data[i0 * 3 + 1].xy;
        vec2 uv1 = Vertices.Data[i1 * 3 + 1].xy;
        vec2 uv2 = Vertices.Data[i2 * 3 + 1].xy;
  
        // Interpolate UV at hit point using barycentric coordinates
        vec3 bary3 = vec3 (1.0 - bary.x - bary.y, bary.x, bary.y);
        vec2 hit_uv = uv0 * bary3.x + uv1 * bary3.y + uv2 * bary3.z;
        Projectiles[i].Hit_U = hit_uv.x;
        Projectiles[i].Hit_V = hit_uv.y;

      // No hit - advance position
      } else {
        Projectiles[i].Position += dir * dist;
      }
    }
  }
} // Physics

// ═══════════
//   Denoise
// ═══════════

glsl comp Denoise {
  #version 460
  
  layout(binding = 0, rgba16f) uniform image2D Input_Image;  // Read: noisy color (linear HDR)
  layout(binding = 1, rgba16f) uniform image2D Output_Image; // Write: filtered color (linear HDR)
  layout(binding = 2, r32f)  uniform image2D Depth_Image;    // Read: hit distance for edge stopping
  
  layout(push_constant) uniform Denoise_Push {
    int Step_Size;  // A-trous step size: 1, 2, 4 for iterations
    int Budget_256; // Budget × 256 (0 = full quality, 256 = cheap path)
  };
  
  layout(local_size_x = 8, local_size_y = 8) in;
  
  // A 3×3 a-trous kernel weights (Gaussian-like, symmetric)
  const float Kernel[3] = float[3](1.0, 2.0 / 3.0, 1.0 / 6.0);
  
  // Depth-based normal from 3 cached depth values
  vec3 Normal_From_Depths (float D_C, float D_R, float D_U) {
    return normalize (vec3 (D_C - D_R, D_C - D_U, 1.0));
  }
  
  // Denoise main
  void main () {
    ivec2 Pixel = ivec2 (gl_GlobalInvocationID.xy);
    ivec2 Size  = imageSize (Input_Image);
    if (Pixel.x >= Size.x or Pixel.y >= Size.y) return;

    // Load center pixel color
    vec3  Center_Color = imageLoad (Input_Image, Pixel).rgb;
  
    // Budget is informational - denoiser always runs when dispatched.
    // The CPU controls whether to dispatch denoise via Active_Denoise_Passes.
  
    // Batch-load all depth values for the 3×3 kernel in one shot
    // Preload 9 depth values - center normal uses Depths[5] (right) and Depths[7] (up).
    // Combined with 9 color loads = 18 total, vs. original 45 = 60% fewer memory ops.
    float Depths[9];
    ivec2 Sample_Positions[9];
    int Idx = 0;
    for (int Dy = -1; Dy <= 1; Dy++) {
      for (int Dx = -1; Dx <= 1; Dx++) {
        Sample_Positions[Idx] = clamp (Pixel + ivec2 (Dx, Dy) * Step_Size, ivec2 (0), Size - 1);
        Depths[Idx] = imageLoad (Depth_Image, Sample_Positions[Idx]).r;
        Idx++;
      }
    }
    float Center_Depth = Depths[4];  // Center of 3×3 = index 4
  
    // Center normal from preloaded batch: Depths[5] = right, Depths[7] = up (at Step_Size offset).
    // At larger step sizes, kernel-spaced gradients are more appropriate for the a-trous edge stopping.
    vec3  Center_Normal = Normal_From_Depths (Center_Depth, Depths[5], Depths[7]);

    // Compute center pixel luminance for edge stopping
    float Center_Luminance = log2 (1.0 + dot (Center_Color, vec3 (0.2126, 0.7152, 0.0722)));

    // Initialize accumulator for weighted filter output
    vec3  Sum    = vec3 (0.0);
    float Weight = 0.0;
  
    // A 3×3 sparse kernel at current step size - depths already cached
    for (int I = 0; I < 9; I++) {
      vec3  S_Color = imageLoad (Input_Image, Sample_Positions[I]).rgb;
      float S_Depth = Depths[I];
      float Sample_Luminance   = log2 (1.0 + dot (S_Color, vec3 (0.2126, 0.7152, 0.0722)));
  
      // Spatial weight: Gaussian kernel
      int Dx = (I % 3) - 1, Dy = (I / 3) - 1;
      float W_Spatial = Kernel[abs(Dx)] * Kernel[abs(Dy)];
  
      // Depth edge stopping (sharper threshold preserves geometric edges)
      float Depth_Diff = abs (Center_Depth - S_Depth) / max (Center_Depth, 0.1);
      float W_Depth = exp (-Depth_Diff * 100.0);
  
      // Normal edge stopping: compute sample normal from cached depths
      float S_D_Right = ((I % 3) < 2) ? Depths[I + 1] : S_Depth;
      float S_D_Up    = (I < 6) ? Depths[I + 3] : S_Depth;
      vec3  S_Normal  = Normal_From_Depths (S_Depth, S_D_Right, S_D_Up);

      // Pow(x,32) > chained squaring (5 muls vs log+mul+exp transcendental)
      float Ndot = max (dot (Center_Normal, S_Normal), 0.0);
      Ndot *= Ndot; Ndot *= Ndot; Ndot *= Ndot; Ndot *= Ndot; Ndot *= Ndot;  // x^32
      float W_Normal = Ndot;
  
      // Luminance edge stopping (tighter threshold preserves color edges)
      float Luminance_Difference = abs (Center_Luminance - Sample_Luminance);
      float Weight_Luminance = exp (-Luminance_Difference * Luminance_Difference * 500.0);

      // Combine all edge-stopping weights and accumulate
      float W = W_Spatial * W_Depth * W_Normal * Weight_Luminance;
      Sum += S_Color * W;
      Weight += W;
    }

    // Compute weighted average and store filtered result
    vec3 Result = Sum / max (Weight, 1e-6);
    imageStore (Output_Image, Pixel, vec4 (Result, 1.0));
  }
} // Denoise
  
// ════════════════
//   Post_Process
// ════════════════

glsl comp Post_Process {
  #version 460
  
  layout(binding = 0, rgba16f) uniform image2D Color_Image;   // RT output (linear HDR, read-write in-place)
  layout(binding = 1, r32f)    uniform image2D Depth_Image;   // Ray hit distance from closest-hit shader
  layout(binding = 2, rgba16f) uniform image2D History_Image; // Previous frame for temporal accumulation (linear HDR)
  layout(binding = 3, rgba16f) uniform image2D Display_Image; // Final tonemapped output (written instead of Color_Image)
  
  // Run-length-encoded push constants
  layout(push_constant) uniform Push {
    float Time;           // Full-precision seconds since start
    uint  Dt_Frame;       // [15:0] = half(Delta_Time), [31:16] = Frame_Count
    uint  Velocity;       // packHalf2x16(Velocity_X, Velocity_Z)
    uint  Speed_Exposure; // packHalf2x16(Speed, Exposure)
    uint  Bloom_Vignette; // packHalf2x16(Bloom_Strength, Vignette_Strength)
    uint  Reproject[8];   // packHalf2x16-compressed 4×4 reprojection matrix (Proj * Prev_View * Inv_View)
    uint  Inv_Proj_Diag;  // [15:0] = half(InvProj[0][0]), [31:16] = half(InvProj[1][1])
    uint  Sun_Screen_Pos; // packHalf2x16(Sun_Screen_U, Sun_Screen_V)
    uint  Sun_Params;     // packHalf2x16(God_Ray_Intensity, Sun_On_Screen)
  } Params;
  
  layout(local_size_x = 8, local_size_y = 8) in;
  
  // Shared memory focus depth broadcast
  shared float Focus_Depth_Shared;
  
  // Spatiotemporal hash for film grain
  float hash (vec2 p) {
    vec3 p3 = fract (vec3 (p.xyx) * 0.1031);
    p3 += dot (p3, p3.yzx + 33.33);
    return fract ((p3.x + p3.y) * p3.z);
  }
  
  // Decode compressed reprojection matrix from push constants
  mat4 Decode_Reproject () {
    mat4 M;
    for (int I = 0; I < 8; I++) {
      vec2 Pair = unpackHalf2x16 (Params.Reproject[I]);
      M[I / 2][I % 2 * 2]     = Pair.x;
      M[I / 2][I % 2 * 2 + 1] = Pair.y;
    }
    return M;
  }
  
  // Postprocess shader main
  void main () {
    ivec2 Pixel = ivec2 (gl_GlobalInvocationID.xy);
    ivec2 Size  = imageSize (Color_Image);
  
    // Unpack fp16 RLE push constants (1 ALU op each via unpackHalf2x16)
    float Delta_Time     = unpackHalf2x16 (Params.Dt_Frame).x;
    uint  Frame_Count    = (Params.Dt_Frame >> 16) & 0xFFFFu;
    vec2  Velocity       = unpackHalf2x16 (Params.Velocity);
    vec2  Speed_Exposure = unpackHalf2x16 (Params.Speed_Exposure);
    float Speed          = Speed_Exposure.x;
    float Exposure       = Speed_Exposure.y;
    vec2  Bloom_Vignette = unpackHalf2x16 (Params.Bloom_Vignette);
    float Bloom_Strength = Bloom_Vignette.x;
    float Vignette       = Bloom_Vignette.y;
  
    // Elect lane 0 to load focus depth, broadcast to workgroup via LDS
    if (gl_LocalInvocationIndex == 0u)
      Focus_Depth_Shared = imageLoad (Depth_Image, Size / 2).r;
    barrier ();

    // Early-out for pixels outside the image bounds
    if (Pixel.x >= Size.x or Pixel.y >= Size.y) return;

    // Load current pixel color and depth
    vec2 UV = (vec2 (Pixel) + 0.5) / vec2 (Size);
    vec3 Color = imageLoad (Color_Image, Pixel).rgb;
    float Depth = imageLoad (Depth_Image, Pixel).r;

    // Contrast Adaptive Sharpening (CAS)
    {
      vec3 N = imageLoad (Color_Image, ivec2 (Pixel.x, max (Pixel.y - 1, 0))).rgb;
      vec3 S = imageLoad (Color_Image, ivec2 (Pixel.x, min (Pixel.y + 1, Size.y - 1))).rgb;
      vec3 W = imageLoad (Color_Image, ivec2 (max (Pixel.x - 1, 0), Pixel.y)).rgb;
      vec3 E = imageLoad (Color_Image, ivec2 (min (Pixel.x + 1, Size.x - 1), Pixel.y)).rgb;
      vec3 Minimum = min (min (N, S), min (W, E));
      vec3 Maximum = max (max (N, S), max (W, E));

      // Reinhard-compress to [0,1] for adaptive weight (handles HDR > 1.0 gracefully)
      vec3 Minimum_Tonemapped = Minimum / (1.0 + Minimum);
      vec3 Maximum_Tonemapped = Maximum / (1.0 + Maximum);
      vec3 Rcp_Range = 1.0 / (Maximum_Tonemapped - Minimum_Tonemapped + 0.04);
      vec3 Sharpness = clamp (min (Minimum_Tonemapped, 1.0 - Maximum_Tonemapped) * Rcp_Range, 0.0, 1.0) * 0.5;
      vec3 Avg = (N + S + W + E) * 0.25;
      Color = max (mix (Avg, Color, 1.0 + Sharpness * 2.0), vec3 (0.0));
    }

    // Temporal accumulation (TAA) with motion-vector reprojection - reconstruct view-space position from NDC
    if (Frame_Count > 0u) {
      vec2  Inverse_Proj = unpackHalf2x16 (Params.Inv_Proj_Diag);
      vec2  NDC         = UV * 2.0 - 1.0;
      vec3  View_Dir    = normalize (vec3 (NDC.x * Inverse_Proj.x, NDC.y * Inverse_Proj.y, -1.0));
      vec3  View_Pos    = View_Dir * Depth;
  
      // Reproject: Proj * Prev_View * Inverse_View * View_Pos > previous clip-space
      mat4  R           = Decode_Reproject ();
      vec4  Prev_Clip   = R * vec4 (View_Pos, 1.0);
      vec2  Prev_UV     = Prev_Clip.xy / Prev_Clip.w * 0.5 + 0.5;
  
      // Fetch history from reprojected location (nearest-neighbor - imageLoad requires integer coords)
      ivec2 Prev_Pixel  = ivec2 (Prev_UV * vec2 (Size));
      bool  On_Screen   = all (greaterThanEqual (Prev_Pixel, ivec2 (0))) and
                          all (lessThan (Prev_Pixel, Size));
  
      // Blend current frame with reprojected history
      if (On_Screen) {
        vec3 History = imageLoad (History_Image, Prev_Pixel).rgb;
        vec3 M1 = Color, M2 = Color * Color;  // Moments for variance computation
        const ivec2 Offsets[4] = ivec2[4](ivec2(-1,0), ivec2(1,0), ivec2(0,-1), ivec2(0,1));
        for (int I = 0; I < 4; I++) {
          vec3 Neighbor = imageLoad (Color_Image, clamp (Pixel + Offsets[I], ivec2(0), Size - 1)).rgb;
          M1 += Neighbor; M2 += Neighbor * Neighbor;
        }
        M1 /= 5.0; M2 /= 5.0;
        vec3 Sigma = sqrt (max (M2 - M1 * M1, vec3 (0.0)));
        History = clamp (History, M1 - Sigma * 0.25, M1 + Sigma * 0.25);
  
        // Luminance-based history rejection
        //
        // If the clamped history still differs significantly from the current frame,
        // the surface has changed (new geometry, lighting change, disocclusion).
        // Boost alpha toward 1.0 to reject stale history aggressively.
        //
        float Current_Luminance  = dot (Color,   vec3 (0.2126, 0.7152, 0.0722));
        float History_Luminance = dot (History, vec3 (0.2126, 0.7152, 0.0722));
        float Luminance_Difference = abs (Current_Luminance - History_Luminance) / max (Current_Luminance, 0.01);
        float Anti_Lag = clamp (Luminance_Difference * 5.0, 0.0, 1.0); // Reject
  
        // Disocclusion detection
        vec2 Screen_Displacement = vec2 (Prev_Pixel - Pixel) / vec2 (Size);
        float Displacement_Length = length (Screen_Displacement);
        float Disocclusion = clamp (Displacement_Length * 30.0, 0.0, 1.0); // Rejected
  
        // Temporal blend
        float Is_Static = step (Speed, 2.0);
  
        // Static: 1/N convergence floored high
        float Static_Alpha = max (1.0 / max (float (Frame_Count), 1.0), 0.35);
  
        // Moving: very aggressive current-frame dominance
        float Motion      = clamp (Speed * 0.04, 0.0, 1.0);  // ISA: reciprocal multiply vs division
        float Base        = mix (0.65, 0.98, Motion);  // Even slow motion > 65% current frame
        float Framerate_Adaptation   = clamp ((Delta_Time - 0.016) * 30.0, 0.0, 1.0);
        float Moving_Alpha = max (max (max (Base, Framerate_Adaptation), Disocclusion), Anti_Lag);

        // Blend between and moving alpha based on camera speed
        float Alpha = mix (Moving_Alpha, Static_Alpha, Is_Static);
        Color = mix (History, Color, Alpha);
      }

      // Off-screen > keep current frame as-is (no history to blend)
    }

    // Write blended result to history for next frame (pre-tonemap, linear HDR)
    imageStore (History_Image, Pixel, vec4 (Color, 1.0));
  
    // Bloom
    //
    // Horizontal + vertical taps create a cross-shaped bloom pattern (fake god rays)
    // Bloom threshold in linear space (0.4 linear ≈ 0.66 sRGB - bright highlights only)
    //
    vec3 Bloom = max (imageLoad (Color_Image, clamp (Pixel + ivec2 ( 3, 0), ivec2 (0), Size - 1)).rgb - 0.4, vec3 (0.0))
             + max (imageLoad (Color_Image, clamp (Pixel + ivec2 (-3, 0), ivec2 (0), Size - 1)).rgb - 0.4, vec3 (0.0))
             + max (imageLoad (Color_Image, clamp (Pixel + ivec2 (0,  3), ivec2 (0), Size - 1)).rgb - 0.4, vec3 (0.0))
             + max (imageLoad (Color_Image, clamp (Pixel + ivec2 (0, -3), ivec2 (0), Size - 1)).rgb - 0.4, vec3 (0.0));
    Color += Bloom * Bloom_Strength;
  
    // God rays
    //
    // When the sun is on screen, march radially from each pixel toward the sun
    // position, accumulating bright sky samples. This creates volumetric-looking
    // light shafts streaming from the sun through gaps in geometry.
    //
    vec2  Sun_UV  = unpackHalf2x16 (Params.Sun_Screen_Pos);
    vec2  Sun_Parameters = unpackHalf2x16 (Params.Sun_Params);
    float God_Ray_Intensity = Sun_Parameters.x;
    float Sun_Visible       = Sun_Parameters.y;

    // Apply god rays when sun is visible on screen
    if (Sun_Visible > 0.5 and God_Ray_Intensity > 0.0) {
      vec2 Delta = Sun_UV - UV;
      float Dist = length (Delta);
      if (Dist > 0.001) {
        vec2  Dir    = Delta / Dist;

        // Incremental stepping replaces multiply-per-iteration. Saves 8 float casts + 8 vec2 multiplies (replaced with 8 vec2 additions)
        vec2  Step_Vec = Dir * (min (Dist, 0.3) / 8.0);
        vec3  Accum  = vec3 (0.0);
        float Falloff = 1.0;
        vec2  Sample_UV = UV;
        for (int S = 1; S <= 8; S++) {
          Sample_UV += Step_Vec;
          ivec2 Sample_Px = ivec2 (Sample_UV * vec2 (Size));
          if (all (greaterThanEqual (Sample_Px, ivec2 (0))) and all (lessThan (Sample_Px, Size))) {
            vec3  Sample_C = imageLoad (Color_Image, Sample_Px).rgb;
            float Sample_D = imageLoad (Depth_Image, Sample_Px).r;

            // Only accumulate sky/bright pixels (depth > 5000 = sky, or very bright)
            float Is_Sky = step (5000.0, Sample_D);
            float Bright = max (dot (Sample_C, vec3 (0.333)) - 0.3, 0.0);
            Accum += Sample_C * max (Is_Sky, Bright * 0.5) * Falloff;
          }
          Falloff *= 0.85; // Exponential decay away from sun
        }

        // Fade god rays based on distance from sun center (stronger near sun)
        float Sun_Fade = 1.0 - clamp (Dist * 2.0, 0.0, 1.0);
        Color += Accum * God_Ray_Intensity * Sun_Fade / 8.0;
      }
    }
  
    // Vignette + saturation + tonemap
    vec2  From_Center = UV - 0.5;
    float Distance_Sq = dot (From_Center, From_Center);
    Color  = Color * (1.0 - Distance_Sq * Vignette);
    Color *= Exposure;
  
    // Saturation boost before tonemapping: push colors away from grey
    float Luma = dot (Color, vec3 (0.2126, 0.7152, 0.0722));
    Color = mix (vec3 (Luma), Color, 1.35); // 35% saturation increase

    // Warm color grading
    Color *= vec3 (1.05, 1.01, 0.92); // Warm grade
  
    // ACES filmic tone mapping (operates on linear HDR values)
    Color  = clamp (Color * (2.51 * Color + 0.03) / (Color * (2.43 * Color + 0.59) + 0.14), 0.0, 1.0);
  
    // Post-tonemap contrast: shadow deepening for rich, cinematic look
    Color = pow (max (Color, vec3 (0.0)), vec3 (1.16)); // 1.16 darkens midtones/shadows while keeping highlights readable.
  
    // Blue-noise dithering
    //
    // Dither by ~0.5/255 in the output before sRGB conversion to prevent banding
    // in dark gradients. The blit to B8G8R8A8_SRGB swapchain handles sRGB encoding.
    //
    float Dither = hash (vec2 (Pixel) + Params.Time * 1.618) - 0.5;  // [-0.5, 0.5]
    Color += Dither / 255.0;

    // Write final color to the display output image (NOT Color_Image, which must
    // stay in raw RT space so checkerboard stale pixels remain consistent)
    imageStore (Display_Image, Pixel, vec4 (clamp (Color, 0.0, 1.0), 1.0));
  }
} // Denoise

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §14. Main
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ══════════════════════════
//   Constrain_Aspect_Ratio
// ══════════════════════════

void Constrain_Aspect_Ratio (int *W, int *H) {
  if (*W < MINIMUM_WINDOW_SIZE) *W = MINIMUM_WINDOW_SIZE;
  int Min_H = MINIMUM_WINDOW_SIZE * ASPECT_NARROW_Y / ASPECT_NARROW_X;
  if (*H < Min_H) *H = Min_H;

  // Width bounds for the current height
  int Max_Width = *H * ASPECT_NARROW_X / ASPECT_NARROW_Y;  // Widest allowed (21:9)
  int Min_Width = *H * ASPECT_WIDE_X   / ASPECT_WIDE_Y;    // Narrowest allowed (4:3)
  int Fit_W = (*W > Max_Width) ? Max_Width : (*W < Min_Width) ? Min_Width : *W;

  // Height bounds for the current width
  int Max_Height = *W * ASPECT_WIDE_Y   / ASPECT_WIDE_X;   // Tallest allowed (4:3)
  int Min_Height = *W * ASPECT_NARROW_Y / ASPECT_NARROW_X; // Shortest allowed (21:9)
  int Fit_H = (*H > Max_Height) ? Max_Height : (*H < Min_Height) ? Min_Height : *H;

  // Apply the clamped dimensions
  *W = Fit_W;
  *H = Fit_H;
}

// ═══════════════════
//   Set_Menu_Cursor
// ═══════════════════

void Set_Menu_Cursor (Cursor_Kind Kind) {
  if (Kind == Current_Cursor_Kind) return;
  Current_Cursor_Kind = Kind;
  if (not In_Menu) return;
  switch (Kind) {
    case CURSOR_SYSTEM:   SDL_SetCursor (SDL_Cursor_Arrow);     break;
    case CURSOR_ACTIVE:   SDL_SetCursor (SDL_Cursor_Hand);      break;
    case CURSOR_INACTIVE: SDL_SetCursor (SDL_Cursor_Crosshair); break;
  }
}

// ═══════════════════
//   Enter_Menu_Mode
// ═══════════════════

void Enter_Menu_Mode () {
  if (In_Menu) return;
  In_Menu = 1;
  Cursor_Centering = 0;
  SDL_SetRelativeMouseMode (SDL_FALSE);
  if (Current_Window_Mode != FULLSCREEN_MODE)
    SDL_SetWindowGrab (Window, SDL_FALSE);
  SDL_SetCursor (SDL_Cursor_Arrow);
  SDL_ShowCursor (SDL_ENABLE);
  Current_Cursor_Kind = CURSOR_SYSTEM;
  printf ("[window] entered menu mode\n");
}

// ═══════════════════
//   Enter_Game_Mode
// ═══════════════════

void Enter_Game_Mode () {
  if (not In_Menu) return;

  // Save cursor position for when we return to menu
  SDL_GetGlobalMouseState (&Saved_Cursor_X, &Saved_Cursor_Y);
  In_Menu = 0;
  SDL_ShowCursor (SDL_DISABLE);
  SDL_SetRelativeMouseMode (SDL_TRUE);
  SDL_SetWindowGrab (Window, SDL_TRUE);
  Cursor_Centering = 1;
  printf ("[window] entered game mode\n");
}

// ═════════════════════
//   Toggle_Fullscreen  
// ═════════════════════

void Toggle_Fullscreen () {

  // Save windowed geometry for restoration
  if (Current_Window_Mode == WINDOWED_MODE) {
    SDL_GetWindowPosition (Window, &Windowed_X, &Windowed_Y);
    SDL_GetWindowSize (Window, &Windowed_W, &Windowed_H);
    SDL_SetWindowFullscreen (Window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    Current_Window_Mode = FULLSCREEN_MODE;
    SDL_SetWindowGrab (Window, SDL_TRUE);
    Swapchain_Dirty = 1;
    printf ("[window] fullscreen\n");
  } else {
    SDL_SetWindowFullscreen (Window, 0);
    SDL_SetWindowPosition (Window, Windowed_X, Windowed_Y);
    SDL_SetWindowSize (Window, Windowed_W, Windowed_H);
    Current_Window_Mode = WINDOWED_MODE;
    if (In_Menu) SDL_SetWindowGrab (Window, SDL_FALSE);
    Swapchain_Dirty = 1;
    printf ("[window] windowed %dx%d\n", Windowed_W, Windowed_H);
  }
}

// ═════════════════════
//   Handle_Activation
// ═════════════════════

void Handle_Activation (Activated_Kind New_State) {
  if (New_State == Current_Activated) return;

  // Dispatch based on the new activation state
  switch (New_State) {
    case OTHER_ACTIVATED:
      Input_Active = 1;
      switch (Current_Window_Mode) {
        case WINDOWED_MODE:

          // Alt-tab back in windowed game mode: pause game input until click
          if (not In_Menu) {
            Cursor_Centering = 0;
            SDL_SetRelativeMouseMode (SDL_FALSE);
            SDL_SetWindowGrab (Window, SDL_FALSE);
            SDL_ShowCursor (SDL_ENABLE);

            // Temporarily enter menu-like state; user clicks to re-enter game
            In_Menu = 1;
            Current_Cursor_Kind = CURSOR_SYSTEM;
            SDL_SetCursor (SDL_Cursor_Arrow);
          } else {
            SDL_SetCursor (SDL_Cursor_Arrow);
          }
          break;
        case FULLSCREEN_MODE:
          SDL_RestoreWindow (Window);
          if (In_Menu) {
            SDL_SetCursor (SDL_Cursor_Arrow);
            SDL_ShowCursor (SDL_ENABLE);
          } else {
            SDL_ShowCursor (SDL_DISABLE);
            SDL_SetRelativeMouseMode (SDL_TRUE);
            SDL_SetWindowGrab (Window, SDL_TRUE);
            Cursor_Centering = 1;
          }
          break;
      }
      break;

    // Handle click-to-reactivate in windowed mode
    case CLICK_ACTIVATED:
      switch (Current_Window_Mode) {
        case WINDOWED_MODE:
          Input_Active = 1;
          if (In_Menu) {
            SDL_SetCursor (SDL_Cursor_Arrow);

          // Click in main window area re-enters game mode
          } else {
            SDL_GetGlobalMouseState (&Saved_Cursor_X, &Saved_Cursor_Y);
            SDL_ShowCursor (SDL_DISABLE);
            SDL_SetRelativeMouseMode (SDL_TRUE);
            SDL_SetWindowGrab (Window, SDL_TRUE);
            Cursor_Centering = 1;
          }
          break;
        default: break;
      }
      break;

    // Handle focus loss and minimize
    case OTHER_DEACTIVATED:
    case MINIMIZE_DEACTIVATED:

      // Save cursor position if in game mode
      if (not In_Menu) SDL_GetGlobalMouseState (&Saved_Cursor_X, &Saved_Cursor_Y);
      Input_Active = 0;
      Cursor_Centering = 0;
      SDL_SetWindowGrab (Window, SDL_FALSE);
      SDL_SetRelativeMouseMode (SDL_FALSE);
      SDL_SetCursor (SDL_Cursor_Arrow);
      SDL_ShowCursor (SDL_ENABLE);
      if (New_State == OTHER_DEACTIVATED and Current_Window_Mode == FULLSCREEN_MODE)
        SDL_MinimizeWindow (Window);
      break;
  }
  Current_Activated = New_State;

} // Handle_Activation

// ══════════════
//   Poll_Input
// ══════════════

Input Poll_Input () {
  Input Input_Data = {0};
  SDL_Event Event;

  // Process all pending SDL events
  while (SDL_PollEvent (&Event)) {
    switch (Event.type) {

      // Handle application quit
      case SDL_QUIT:
        Quit = 1;
        break;

      // Handle key presses (ESC, F11)
      case SDL_KEYDOWN:
        if (Event.key.repeat) break; // Ignore key repeat for mode toggles
        if (Event.key.keysym.sym == SDLK_ESCAPE) {
          if (In_Menu) Quit = 1;   // ESC in menu = quit
          else Enter_Menu_Mode (); // ESC in game = open menu
        }
        if (Event.key.keysym.sym == SDLK_F11)
          Toggle_Fullscreen ();
        break;

      // Handle mouse button clicks
      case SDL_MOUSEBUTTONDOWN:
        if (Event.button.button == SDL_BUTTON_LEFT) {
          if (In_Menu) Enter_Game_Mode (); // Click in menu = enter game
          else Input_Data.Fire = 1;
        }
        break;

      // Handle mouse movement for camera control
      case SDL_MOUSEMOTION:
        if (not In_Menu and Input_Active) {
          Input_Data.Delta_X += Event.motion.xrel;
          Input_Data.Delta_Y += Event.motion.yrel;
        }
        break;

      // Handle window focus, minimize, and resize events
      case SDL_WINDOWEVENT:
        switch (Event.window.event) {
          case SDL_WINDOWEVENT_FOCUS_GAINED:
            Handle_Activation (OTHER_ACTIVATED);
            break;
          case SDL_WINDOWEVENT_FOCUS_LOST:
            Handle_Activation (OTHER_DEACTIVATED);
            break;
          case SDL_WINDOWEVENT_MINIMIZED:
            Handle_Activation (MINIMIZE_DEACTIVATED);
            break;
          case SDL_WINDOWEVENT_RESTORED:
            Handle_Activation (OTHER_ACTIVATED);
            break;
          case SDL_WINDOWEVENT_RESIZED: {
            int New_W = Event.window.data1;
            int New_H = Event.window.data2;
            if (Current_Window_Mode == WINDOWED_MODE) {
              Constrain_Aspect_Ratio (&New_W, &New_H);
              if (New_W != Event.window.data1 or New_H != Event.window.data2)
                SDL_SetWindowSize (Window, New_W, New_H);
            }
            Width  = New_W;
            Height = New_H;
            Swapchain_Dirty = 1;
            break;
          }
        }
        break;
    }
  }

  // Sample keyboard state only in game mode with active input
  if (not In_Menu and Input_Active) {
    const uint8_t *Keyboard = SDL_GetKeyboardState (NULL);
    Input_Data.Forward = Keyboard[SDL_SCANCODE_W]     or Keyboard[SDL_SCANCODE_UP];
    Input_Data.Back    = Keyboard[SDL_SCANCODE_S]     or Keyboard[SDL_SCANCODE_DOWN];
    Input_Data.Left    = Keyboard[SDL_SCANCODE_A]     or Keyboard[SDL_SCANCODE_LEFT];
    Input_Data.Right   = Keyboard[SDL_SCANCODE_D]     or Keyboard[SDL_SCANCODE_RIGHT];
    Input_Data.Jump    = Keyboard[SDL_SCANCODE_SPACE];
    Input_Data.Crouch  = Keyboard[SDL_SCANCODE_LCTRL] or Keyboard[SDL_SCANCODE_C];
  }

  // Return result
  return Input_Data;

} // Poll_Input

// ════════
//   main
// ════════

int main (int Argc, char **Argv) {

  // Command-line flags
  //   --quality LEVEL   Set quality preset (best/great/good/ok/normal/bad/worst)
  //   --physics-test    Run physics-only test (no rendering), exit after 3s sim
  //   --benchmark N     Run N frames of automated fly-through, print FPS stats, exit
  //   --screenshot F    Render one frame from spawn, save as F (TGA), exit
  //   --no-postprocess  Disable post-processing (raw PBR output)
  //   --spp N           Override samples-per-pixel (1, 2, 4, 8)
  //   --res WxH         Override render resolution
  //   --no-pbr          Disable PBR maps (diffuse + lightmap only, for A/B comparison)
  //   --no-parallax     Disable parallax occlusion mapping
  //   --dump-frames D   Save each benchmark frame as D/frame_NNNN.tga
  //   mapname.bsp       Load specified BSP map instead of default

  // Local variables
  int         Physics_Test      = 0;
  int         Benchmark_Frames  = 0;    // 0 = disabled, >0 = run N frames then exit
  const char *Screenshot_Path   = NULL; // NULL = disabled, otherwise save frame and exit
  const char *Dump_Frames_Dir   = NULL; // NULL = disabled, otherwise dump each frame
  int         No_Postprocess    = 0;
  int         No_PBR            = 0;
  int         No_Parallax       = 0;
  int         Force_Cheap       = 0;    // --cheap: force Budget=1.0 (lightmap-only fallback)
  int         Override_SPP      = 0;    // 0 = use default from quality preset
  int         Override_Res      = 0;    // 1 if --res was specified (overrides quality preset)
  const char *Map_Name = DEFAULT_MAP;

  // Parse command-line arguments
  for (int I = 1; I < Argc; I++) {
    if      (strcmp (Argv[I], "--physics-test")   == 0) Physics_Test = 1;
    else if (strcmp (Argv[I], "--benchmark")      == 0 and I + 1 < Argc) Benchmark_Frames = atoi (Argv[++I]);
    else if (strcmp (Argv[I], "--screenshot")     == 0 and I + 1 < Argc) Screenshot_Path = Argv[++I];
    else if (strcmp (Argv[I], "--dump-frames")    == 0 and I + 1 < Argc) Dump_Frames_Dir = Argv[++I];
    else if (strcmp (Argv[I], "--no-postprocess") == 0) No_Postprocess = 1;
    else if (strcmp (Argv[I], "--no-pbr")         == 0) No_PBR = 1;
    else if (strcmp (Argv[I], "--no-parallax")    == 0) No_Parallax = 1;
    else if (strcmp (Argv[I], "--cheap")          == 0) Force_Cheap = 1;
    else if (strcmp (Argv[I], "--spp")            == 0 and I + 1 < Argc) Override_SPP = atoi (Argv[++I]);
    else if (strcmp (Argv[I], "--res")            == 0 and I + 1 < Argc) {
      sscanf (Argv[++I], "%dx%d", &Width, &Height);
      Override_Res = 1;
    }
    else if (strcmp (Argv[I], "--quality") == 0 and I + 1 < Argc) {
      const char *Q = Argv[++I];
      if      (strcasecmp (Q, "ultra")  == 0) Active_Quality = QUALITY_ULTRA;
      else if (strcasecmp (Q, "high")   == 0) Active_Quality = QUALITY_HIGH;
      else if (strcasecmp (Q, "medium") == 0) Active_Quality = QUALITY_MEDIUM;
      else if (strcasecmp (Q, "low")    == 0) Active_Quality = QUALITY_LOW;
      else if (strcasecmp (Q, "potato") == 0) Active_Quality = QUALITY_POTATO;
      else printf ("[quality] unknown preset '%s' (ultra/high/medium/low/potato)\n", Q);
    }
    else Map_Name = Argv[I];
  }

  // Apply quality presets
  const Quality_Preset *Preset = &QUALITY_PRESETS[Active_Quality];
  if (not Override_Res) { Width = Preset->Width; Height = Preset->Height; }
  Active_Render_Scale  = Preset->Render_Scale;
  Active_Denoise_Passes = Preset->Denoise_Passes;
  Active_Checkerboard   = Preset->Checkerboard;
  if (not No_Parallax) No_Parallax = not Preset->Parallax;
  printf ("[quality] preset: %s (%dx%d @ %.0f%% scale, %d SPP)\n",
          Preset->Name, Width, Height, Active_Render_Scale * 100.f, Override_SPP ? Override_SPP : Preset->SPP);

  // ...
  (void)No_PBR;      // Used after texture loading to zero PBR_Stride
  (void)No_Parallax; // Handled via PBR_Stride=0 (parallax checks Tex_Id < PBR_Stride)
  char Map_Path[256];
  snprintf (Map_Path, sizeof Map_Path, "%smaps/%s", ASSET_ROOT, Map_Name);

  // Initialize SDL2 with video subsystem and create a Vulkan-capable resizable window
  SDL_Init (SDL_INIT_VIDEO);
  Window = SDL_CreateWindow (ENGINE_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             Width, Height, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

  // Create system cursors for menu mode rollover
  SDL_Cursor_Arrow     = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_ARROW);
  SDL_Cursor_Hand      = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_HAND);
  SDL_Cursor_Crosshair = SDL_CreateSystemCursor (SDL_SYSTEM_CURSOR_CROSSHAIR);

  // Start in game mode: capture mouse for FPS controls
  SDL_SetRelativeMouseMode (SDL_TRUE);
  SDL_SetWindowGrab (Window, SDL_TRUE);
  Cursor_Centering = 1;
  Input_Active = 1;
  Windowed_W = Width;
  Windowed_H = Height;
  SDL_GetWindowPosition (Window, &Windowed_X, &Windowed_Y);

  // Create the Vulkan environment
  Vulkan_Create_Instance ();
  Vulkan_Pick_Physical_Device ();
  Vulkan_Create_Logical_Device ();
  Vulkan_Create_Swapchain ();
  Vulkan_Create_Synchronization ();

  // Compute internal render resolution
  Render_Width  = (int)(Width  * Active_Render_Scale);
  Render_Height = (int)(Height * Active_Render_Scale);
  Render_Width  = (Render_Width  + 7) & ~7;  // Round up to multiple of 8 (postprocess workgroup size)
  Render_Height = (Render_Height + 7) & ~7;
  printf ("[render] internal %dx%d > window %dx%d (scale %.0f%%)\n",
          Render_Width, Render_Height, Width, Height, Active_Render_Scale * 100.f);

  // Create the ray tracing storage image (render target) and depth image (R32F for postprocess DOF). Storage images use internal render
  // resolution - bilinear blit upscales to window/swapchain
  Raytracing_Storage_Image = Image_Storage_Create (Render_Width, Render_Height);
  Vulkan_Transition_Storage_Image ();

  // Create R32F depth image for postprocessing
  {
    Depth_Image.Format = VK_FORMAT_R32_SFLOAT;
    VK_CHECK (vkCreateImage (/*device      =>*/ Device,
                             /*pCreateInfo =>*/ &(VkImageCreateInfo){
                               .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                               .imageType     = VK_IMAGE_TYPE_2D,
                               .format        = VK_FORMAT_R32_SFLOAT,
                               .extent        = {Render_Width, Render_Height, 1},
                               .mipLevels     = 1,
                               .arrayLayers   = 1,
                               .samples       = VK_SAMPLE_COUNT_1_BIT,
                               .tiling        = VK_IMAGE_TILING_OPTIMAL,
                               .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                               .usage         = VK_IMAGE_USAGE_STORAGE_BIT},
                             /*pAllocator  =>*/ NULL,
                             /*pImage      =>*/ &Depth_Image.Image));
    VkMemoryRequirements Mem_Req;
    vkGetImageMemoryRequirements (Device, Depth_Image.Image, &Mem_Req);
    VK_CHECK (vkAllocateMemory (/*device       =>*/ Device,
                                /*pAllocateInfo =>*/ &(VkMemoryAllocateInfo){
                                  .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                  .allocationSize  = Mem_Req.size,
                                  .memoryTypeIndex = Find_Memory_Type (Mem_Req.memoryTypeBits,
                                                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)},
                                /*pAllocator    =>*/ NULL,
                                /*pMemory       =>*/ &Depth_Image.Memory));
    VK_CHECK (vkBindImageMemory (Device, Depth_Image.Image, Depth_Image.Memory, 0));
    VK_CHECK (vkCreateImageView (/*device      =>*/ Device,
                                 /*pCreateInfo =>*/ &(VkImageViewCreateInfo){
                                   .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                   .image            = Depth_Image.Image,
                                   .viewType         = VK_IMAGE_VIEW_TYPE_2D,
                                   .format           = VK_FORMAT_R32_SFLOAT,
                                   .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}},
                                 /*pAllocator  =>*/ NULL,
                                 /*pView       =>*/ &Depth_Image.View));

    // Transition depth image to general layout
    VkCommandBuffer Cmd;
    VK_CHECK (vkAllocateCommandBuffers (/*device        =>*/ Device,
                                        /*pAllocateInfo =>*/ &(VkCommandBufferAllocateInfo){
                                          .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                          .commandPool        = Command_Pool,
                                          .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                          .commandBufferCount = 1},
                                        /*pCommandBuffers =>*/ &Cmd));
    VK_CHECK (vkBeginCommandBuffer (/*commandBuffer =>*/ Cmd,
                                    /*pBeginInfo    =>*/ &(VkCommandBufferBeginInfo){
                                      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));
    Image_Layout_Barrier (/*Command_Buffer     =>*/ Cmd,
                          /*Image              =>*/ Depth_Image.Image,
                          /*Old_Layout         =>*/ VK_IMAGE_LAYOUT_UNDEFINED,
                          /*New_Layout         =>*/ VK_IMAGE_LAYOUT_GENERAL,
                          /*Source_Access      =>*/ 0,
                          /*Destination_Access =>*/ VK_ACCESS_SHADER_WRITE_BIT,
                          /*Source_Stage       =>*/ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          /*Destination_Stage  =>*/ VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
    VK_CHECK (vkEndCommandBuffer (Cmd));
    VK_CHECK (vkQueueSubmit (/*queue       =>*/ Queue,
                             /*submitCount =>*/ 1,
                             /*pSubmits    =>*/ &(VkSubmitInfo){
                               .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                               .commandBufferCount = 1,
                               .pCommandBuffers    = &Cmd},
                             /*fence       =>*/ VK_NULL_HANDLE));
    vkQueueWaitIdle (Queue);
    vkFreeCommandBuffers (Device, Command_Pool, 1, &Cmd);
  }

  // Create history image for temporal accumulation (TAA) - same size as render target
  History_Image = Image_Storage_Create (Render_Width, Render_Height);
  {
    VkCommandBuffer Cmd;
    VK_CHECK (vkAllocateCommandBuffers (/*device        =>*/ Device,
                                        /*pAllocateInfo =>*/ &(VkCommandBufferAllocateInfo){
                                          .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                          .commandPool        = Command_Pool,
                                          .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                          .commandBufferCount = 1},
                                        /*pCommandBuffers =>*/ &Cmd));
    VK_CHECK (vkBeginCommandBuffer (/*commandBuffer =>*/ Cmd,
                                    /*pBeginInfo    =>*/ &(VkCommandBufferBeginInfo){
                                      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));
    Image_Layout_Barrier (/*Command_Buffer     =>*/ Cmd,
                          /*Image              =>*/ History_Image.Image,
                          /*Old_Layout         =>*/ VK_IMAGE_LAYOUT_UNDEFINED,
                          /*New_Layout         =>*/ VK_IMAGE_LAYOUT_GENERAL,
                          /*Source_Access      =>*/ 0,
                          /*Destination_Access =>*/ VK_ACCESS_SHADER_WRITE_BIT,
                          /*Source_Stage       =>*/ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          /*Destination_Stage  =>*/ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    VK_CHECK (vkEndCommandBuffer (Cmd));
    VK_CHECK (vkQueueSubmit (/*queue       =>*/ Queue,
                             /*submitCount =>*/ 1,
                             /*pSubmits    =>*/ &(VkSubmitInfo){
                               .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                               .commandBufferCount = 1,
                               .pCommandBuffers    = &Cmd},
                             /*fence       =>*/ VK_NULL_HANDLE));
    vkQueueWaitIdle (Queue);
    vkFreeCommandBuffers (Device, Command_Pool, 1, &Cmd);
  }

  // Create postprocess output image - postprocess writes here instead of back to Color_Image, so that Color_Image (raw RT) stays in a
  // consistent space across frames. Critical for checkerboard: stale pixels remain raw RT, not tonemapped.
  Postprocess_Output_Image = Image_Storage_Create (Render_Width, Render_Height);
  {
    VkCommandBuffer Cmd;
    VK_CHECK (vkAllocateCommandBuffers (/*device        =>*/ Device,
                                        /*pAllocateInfo =>*/ &(VkCommandBufferAllocateInfo){
                                          .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                          .commandPool        = Command_Pool,
                                          .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                          .commandBufferCount = 1},
                                        /*pCommandBuffers =>*/ &Cmd));
    VK_CHECK (vkBeginCommandBuffer (/*commandBuffer =>*/ Cmd,
                                    /*pBeginInfo    =>*/ &(VkCommandBufferBeginInfo){
                                      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));
    Image_Layout_Barrier (/*Command_Buffer     =>*/ Cmd,
                          /*Image              =>*/ Postprocess_Output_Image.Image,
                          /*Old_Layout         =>*/ VK_IMAGE_LAYOUT_UNDEFINED,
                          /*New_Layout         =>*/ VK_IMAGE_LAYOUT_GENERAL,
                          /*Source_Access      =>*/ 0,
                          /*Destination_Access =>*/ VK_ACCESS_SHADER_WRITE_BIT,
                          /*Source_Stage       =>*/ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          /*Destination_Stage  =>*/ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    VK_CHECK (vkEndCommandBuffer (Cmd));
    VK_CHECK (vkQueueSubmit (/*queue       =>*/ Queue,
                             /*submitCount =>*/ 1,
                             /*pSubmits    =>*/ &(VkSubmitInfo){
                               .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                               .commandBufferCount = 1,
                               .pCommandBuffers    = &Cmd},
                             /*fence       =>*/ VK_NULL_HANDLE));
    vkQueueWaitIdle (Queue);
    vkFreeCommandBuffers (Device, Command_Pool, 1, &Cmd);
  }

  // Allocate the camera uniform buffer sizeof(mat4)*2 + 16 (base) + 7*16 (environment vec4s) = 256 bytes
  Camera_Uniform_Buffer = Buffer_Allocate (/*Size         =>*/ 256,
                                           /*Usage        =>*/ VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                           /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                             | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Load the BSP scene and spawn point (no CPU collision map - GPU handles physics via TLAS)
  Spawn Spawn_Point;
  Scene Scene_Data = Scene_Load_From_BSP (Map_Path, &Spawn_Point);

  // Load entity (sarge + machinegun, idle animation).
  // Must happen before Scene_Load_Textures so entity materials are included in the texture array.
  Entity Enemy = Entity_Load (&Scene_Data, Spawn_Point);

  // Infer per-scene environment settings from BSP data (sky textures, worldspawn)
  Active_Environment = Environment_Infer_From_Scene (&Scene_Data);

  // Load scene and weapon textures
  Scene_Load_Textures (&Scene_Data);
  Weapon_Instance Weapon = {0};
  Weapon.Model = Weapon_Model_Load ();
  Weapon_Load_Textures (&Weapon);

  // --no-pbr: set PBR_Stride to 0 to force heuristic PBR for all materials.
  // --no-parallax / Potato quality: disables parallax and reflections via Active_SPP flags
  // but keeps PBR maps enabled (per-material classification still works).
  if (No_PBR) {
    printf ("[mode] PBR maps DISABLED (heuristic fallback)\n");
    PBR_Stride = 0;
  }
  printf ("[mode] PBR maps %s, parallax %s\n",
          No_PBR ? "DISABLED" : "enabled", No_Parallax ? "DISABLED" : "enabled");

  // Build acceleration structures (BLAS for world + weapon + enemy, then TLAS)
  Acceleration_Structure World_Bottom_Level = Build_World_Bottom_Level (&Scene_Data);
  Weapon_Bottom_Level_Initialize (&Weapon);
  Entity_Bottom_Level_Initialize (&Enemy);
  Top_Level_Initialize (4);
  Top_Level_Rebuild (&World_Bottom_Level, &Weapon.Bottom_Level, &Enemy.Bottom_Level, NULL);

  // Create the ray tracing pipeline, shader binding table, and descriptors
  Raytracing_Pipeline_Create ();
  Shader_Binding_Table_Create ();
  Descriptor_Set_Create (&Weapon, &Enemy);

  // Create the post-processing pipeline (reads color + depth, writes color)
  Postprocess_Pipeline_Create ();

  // Create the a-trous spatial denoiser
  Denoise_Ping_Image = Image_Storage_Create (Render_Width, Render_Height);
  {
    VkCommandBuffer Cmd;
    VK_CHECK (vkAllocateCommandBuffers (/*device        =>*/ Device,
                                        /*pAllocateInfo =>*/ &(VkCommandBufferAllocateInfo){
                                          .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                          .commandPool        = Command_Pool,
                                          .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                          .commandBufferCount = 1},
                                        /*pCommandBuffers =>*/ &Cmd));
    VK_CHECK (vkBeginCommandBuffer (/*commandBuffer =>*/ Cmd,
                                    /*pBeginInfo    =>*/ &(VkCommandBufferBeginInfo){
                                      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));
    Image_Layout_Barrier (/*Command_Buffer     =>*/ Cmd,
                          /*Image              =>*/ Denoise_Ping_Image.Image,
                          /*Old_Layout         =>*/ VK_IMAGE_LAYOUT_UNDEFINED,
                          /*New_Layout         =>*/ VK_IMAGE_LAYOUT_GENERAL,
                          /*Source_Access      =>*/ 0,
                          /*Destination_Access =>*/ VK_ACCESS_SHADER_WRITE_BIT,
                          /*Source_Stage       =>*/ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          /*Destination_Stage  =>*/ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    VK_CHECK (vkEndCommandBuffer (Cmd));
    VK_CHECK (vkQueueSubmit (/*queue       =>*/ Queue,
                             /*submitCount =>*/ 1,
                             /*pSubmits    =>*/ &(VkSubmitInfo){
                               .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                               .commandBufferCount = 1,
                               .pCommandBuffers    = &Cmd},
                             /*fence       =>*/ VK_NULL_HANDLE));
    vkQueueWaitIdle (Queue);
    vkFreeCommandBuffers (Device, Command_Pool, 1, &Cmd);
  }
  Denoise_Pipeline_Create ();

  // Create the GPU physics pipeline and resources (with hull binding)
  Physics_Pipeline_Create ();

  // Spawn origin is at Q3 player origin (24 units above feet). Our capsule half-height is 32, so raise by 8 to align capsule bottom
  // with Q3 bounding box bottom.
  Player Initial_Player = {
    .Position = {Spawn_Point.Origin.x, Spawn_Point.Origin.y + 8.f, Spawn_Point.Origin.z},
    .Yaw      = 1.5707963f - Spawn_Point.Angle * 3.14159f / 180.f}; // π/2 - angle: Q3 angle 0 = +X = our yaw π/2
  Physics_Resources_Create (&Initial_Player);

  // Initialize the audio system (OpenAL with synthesized sounds)
  Audio_Init ();

  // Apply runtime mode flags
  Skip_Postprocess = No_Postprocess;

  // Default SPP=1 for maximum speed; override with --spp N
  uint Active_SPP = Override_SPP ? (uint)Override_SPP : (uint)QUALITY_PRESETS[Active_Quality].SPP;

  // Log diagnostic output
  printf ("[init] ready - entering game loop\n");

  // Physics-only test mode: run with --physics-test to skip rendering and simulate movement at fixed 60fps.
  // Exits after ~3 seconds of simulated time with full diagnostic output.
  if (Physics_Test) {
    fprintf (stderr, "[physics-test] starting physics-only test (3s simulated, fixed dt=0.016)\n");
    float Fixed_Dt = 1.f / 60.f;  // 60fps timestep

    // Phase 1: idle for 0.5s (let player settle on ground)
    for (int I = 0; I < 30; I++) {
      Input In = {0};
      Player P = Physics_Dispatch (In, Fixed_Dt);
      if (I % 10 == 0)
        fprintf (stderr, "  [idle  %3d] pos=(%.1f,%.1f,%.1f) vel=(%.1f,%.1f,%.1f) gnd=%d\n",
                 I, P.Position.x, P.Position.y, P.Position.z,
                 P.Velocity.x, P.Velocity.y, P.Velocity.z, P.On_Ground);
    }

    // Phase 2: walk forward (W) for 1s
    for (int I = 0; I < 60; I++) {
      Input In = {.Forward = 1};
      Player P = Physics_Dispatch (In, Fixed_Dt);
      if (I % 15 == 0)
        fprintf (stderr, "  [walk  %3d] pos=(%.1f,%.1f,%.1f) vel=(%.1f,%.1f,%.1f) gnd=%d\n",
                 I, P.Position.x, P.Position.y, P.Position.z,
                 P.Velocity.x, P.Velocity.y, P.Velocity.z, P.On_Ground);
    }

    // Phase 3: jump for 1s
    for (int I = 0; I < 60; I++) {
      Input In = {.Forward = 1, .Jump = (I < 3) ? 1 : 0};  // Tap jump briefly
      Player P = Physics_Dispatch (In, Fixed_Dt);
      if (I % 10 == 0)
        fprintf (stderr, "  [jump  %3d] pos=(%.1f,%.1f,%.1f) vel=(%.1f,%.1f,%.1f) gnd=%d\n",
                 I, P.Position.x, P.Position.y, P.Position.z,
                 P.Velocity.x, P.Velocity.y, P.Velocity.z, P.On_Ground);
    }

    // Phase 4: strafe right for 0.5s
    for (int I = 0; I < 30; I++) {
      Input In = {.Right = 1};
      Player P = Physics_Dispatch (In, Fixed_Dt);
      if (I % 10 == 0)
        fprintf (stderr, "  [strafe %2d] pos=(%.1f,%.1f,%.1f) vel=(%.1f,%.1f,%.1f) gnd=%d\n",
                 I, P.Position.x, P.Position.y, P.Position.z,
                 P.Velocity.x, P.Velocity.y, P.Velocity.z, P.On_Ground);
    }

    // Log diagnostic output
    fprintf (stderr, "[physics-test] done\n");
    vkDeviceWaitIdle (Device);
    return 0;
  }

  // Benchmark mode
  if (Benchmark_Frames > 0 or Screenshot_Path) {
    Camera Bench_Cam = {.Position = {Spawn_Point.Origin.x, Spawn_Point.Origin.y + 8.f + DEFAULT_VIEW_HEIGHT, Spawn_Point.Origin.z},
                         .Yaw = 1.5707963f - Spawn_Point.Angle * 3.14159f / 180.f};
    int Total_Frames = Screenshot_Path ? 1 : Benchmark_Frames;
    float Fixed_Dt = 1.f / 60.f;

    // Warm up: render 3 frames to trigger LLVM JIT compilation and fill CPU caches. On lavapipe, the first Raytracing_Frame()
    // invocation triggers shader compilation (SPIR-V > NIR > LLVM IR > x86 machine code), costing 500-700ms. Running warmup frames
    // before the timer ensures benchmark numbers reflect steady-state performance.
    printf ("[benchmark] warming up (5 frames)...\n");
    {
      VK_CHECK (vkWaitForFences (Device, 1, &Fence, VK_TRUE, UINT64_MAX));
      Weapon_Update (&Weapon, &Bench_Cam, Fixed_Dt, 0);
      Weapon_Bottom_Level_Rebuild (&Weapon);
      Top_Level_Rebuild (&World_Bottom_Level, &Weapon.Bottom_Level, &Enemy.Bottom_Level, NULL);
      mat4 Bench_View = View (Bench_Cam.Position, Bench_Cam.Yaw, Bench_Cam.Pitch);
      mat4 Bench_Proj = Perspective (FIELD_OF_VIEW, (float)Width / Height, 0.1f, 10000.f);
      mat4 Bench_Inv_Proj = Inverse_Projection (Bench_Proj);
      mat4 Bench_Reproj = Mat4_Mul (Bench_Proj, Mat4_Mul (Bench_View, Inverse_Orthogonal (Bench_View)));
      Prev_View_Matrix = Bench_View;

      // Alternate frame parity so checkerboard traces both pixel halves during warmup
      for (int I = 0; I < 5; I++) {
        Bench_Cam.Frame = (uint)I;
        Camera_Upload (&Bench_Cam, FIELD_OF_VIEW, Weapon.Texture_Base_Index, PBR_Stride, Active_SPP);
        Gpu_Postprocess_Push Warmup_Postprocess = {.Time = 0,
          .Dt_Frame       = (uint32_t)Float_To_Half (Fixed_Dt) | ((uint32_t)(Frame_Count & 0xFFFF) << 16),
          .Velocity       = 0,
          .Speed_Exposure = Pack_Half2x16 (0.f, STYLE.Exposure),
          .Bloom_Vignette = Pack_Half2x16 (STYLE.Bloom_Strength, STYLE.Vignette),
          .Inv_Proj_Diag  = Pack_Half2x16 (Bench_Inv_Proj.E[0], Bench_Inv_Proj.E[5]),
          .Sun_Screen_Pos = Pack_Half2x16 (0.5f, 0.5f),
          .Sun_Params     = Pack_Half2x16 (0.f, 0.f)};
        Pack_Mat4_Half (&Bench_Reproj, Warmup_Postprocess.Reproject);
        Raytracing_Frame (Warmup_Postprocess);
        VK_CHECK (vkWaitForFences (Device, 1, &Fence, VK_TRUE, UINT64_MAX));
        Frame_Count++;
      }
    }

    // Log diagnostic output
    printf ("[benchmark] rendering %d frame(s)...\n", Total_Frames);
    (void)SDL_GetPerformanceCounter ();
    uint64_t Bench_Freq  = SDL_GetPerformanceFrequency ();
    float    Frame_Min   = 1e9f, Frame_Max = 0, Frame_Sum = 0;
    float   *Frame_Times = calloc (Total_Frames, sizeof (float));  // For percentile stats
    vec3     Prev_Bench_Pos = Bench_Cam.Position;  // Track camera position for speed computation

    // Render each benchmark frame
    for (int F = 0; F < Total_Frames; F++) {
      // Wait for the previous frame's RT submission to complete before reusing Command_Buffer
      VK_CHECK (vkWaitForFences (Device, 1, &Fence, VK_TRUE, UINT64_MAX));

      // Time this frame
      uint64_t Frame_Start = SDL_GetPerformanceCounter ();

      // Slowly rotate the camera for visual coverage (full 360 over 600 frames = 10s at 60fps)
      if (not Screenshot_Path) {
        Bench_Cam.Yaw += 6.28318f / 600.f;
        Input In = {.Forward = (F % 120 < 60) ? 1 : 0, .Right = (F % 120 >= 60) ? 1 : 0};
        Player P = Physics_Dispatch (In, Fixed_Dt);
        Bench_Cam.Position = P.Position;
        Bench_Cam.Position.y += P.View_Height;
        Bench_Cam.Yaw = P.Yaw;
        Bench_Cam.Pitch = P.Pitch;
      } else {
        Input In = {0};
        Physics_Dispatch (In, Fixed_Dt);
      }

      // Update scene state for this frame
      Bench_Cam.Frame = (uint)F;
      Weapon_Update (&Weapon, &Bench_Cam, Fixed_Dt, 0);
      Weapon_Bottom_Level_Rebuild (&Weapon);
      Enemy.Animation_Time += Fixed_Dt;
      Enemy.Current_Vertices = Enemy.Frame_Vertices[(int)(Enemy.Animation_Time * Enemy.Frame_FPS) % (int)Enemy.Frame_Count];
      Entity_Bottom_Level_Rebuild (&Enemy);
      Top_Level_Rebuild (&World_Bottom_Level, &Weapon.Bottom_Level, &Enemy.Bottom_Level, NULL);
      Camera_Upload (&Bench_Cam, FIELD_OF_VIEW, Weapon.Texture_Base_Index, PBR_Stride, Active_SPP);

      // Build view and projection matrices
      mat4 Bench_View = View (Bench_Cam.Position, Bench_Cam.Yaw, Bench_Cam.Pitch);
      mat4 Bench_Projection = Perspective (FIELD_OF_VIEW, (float)Width / Height, 0.1f, 10000.f);
      mat4 Bench_Inverse_Projection = Inverse_Projection (Bench_Projection);
      mat4 Bench_Reproject = Mat4_Mul (Bench_Projection, Mat4_Mul (Prev_View_Matrix, Inverse_Orthogonal (Bench_View)));

      // Compute actual camera speed from position delta for TAA motion detection
      float Bench_Speed = 0.f;
      if (F > 0) {
        float Dx = Bench_Cam.Position.x - Prev_Bench_Pos.x;
        float Dy = Bench_Cam.Position.y - Prev_Bench_Pos.y;
        float Dz = Bench_Cam.Position.z - Prev_Bench_Pos.z;
        Bench_Speed = sqrtf (Dx*Dx + Dy*Dy + Dz*Dz) / Fixed_Dt;
      }
      Prev_Bench_Pos = Bench_Cam.Position;

      // Configure post-processing push constants
      Gpu_Postprocess_Push Postprocess = {.Time = F * Fixed_Dt,
        .Dt_Frame       = (uint32_t)Float_To_Half (Fixed_Dt) | ((uint32_t)(Frame_Count & 0xFFFF) << 16),
        .Velocity       = 0,
        .Speed_Exposure = Pack_Half2x16 (Bench_Speed, STYLE.Exposure),
        .Bloom_Vignette = Pack_Half2x16 (STYLE.Bloom_Strength, STYLE.Vignette),
        .Inv_Proj_Diag  = Pack_Half2x16 (Bench_Inverse_Projection.E[0], Bench_Inverse_Projection.E[5]),
        .Sun_Screen_Pos = Pack_Half2x16 (0.5f, 0.5f),
        .Sun_Params     = Pack_Half2x16 (0.15f, 0.f)};
      Pack_Mat4_Half (&Bench_Reproject, Postprocess.Reproject);
      Raytracing_Frame (Postprocess);
      Prev_View_Matrix = Bench_View;
      Frame_Count++;

      // Record frame timing statistics
      uint64_t Frame_End = SDL_GetPerformanceCounter ();
      float    Frame_Ms  = (float)(Frame_End - Frame_Start) * 1000.f / (float)Bench_Freq;
      Frame_Times[F] = Frame_Ms;
      if (Frame_Ms < Frame_Min) Frame_Min = Frame_Ms;
      if (Frame_Ms > Frame_Max) Frame_Max = Frame_Ms;
      Frame_Sum += Frame_Ms;

      // Print periodic progress
      if (F % 50 == 0)
        printf ("  [frame %4d/%d] %.2f ms (%.1f fps)\n", F, Total_Frames, Frame_Ms, 1000.f / Frame_Ms);

      // Dump_Frame_To_Disk:
      if (Dump_Frames_Dir) {
        VK_CHECK (vkWaitForFences (Device, 1, &Fence, VK_TRUE, UINT64_MAX));
        uint64_t Pixel_Buffer_Size = (uint64_t)Render_Width * Render_Height * 8;  // R16G16B16A16_SFLOAT = 8 bytes/pixel

        // Allocate_Readback_Buffer:
        Gpu_Buffer Readback = Buffer_Allocate (Pixel_Buffer_Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        // Record_Download_Command:
        VkCommandBuffer Download_Command;
        VK_CHECK (vkAllocateCommandBuffers (/*device        =>*/ Device,
                                            /*pAllocateInfo =>*/ &(VkCommandBufferAllocateInfo){
                                              .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                              .commandPool        = Command_Pool,
                                              .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                              .commandBufferCount = 1},
                                            /*pCommandBuffers =>*/ &Download_Command));
        VK_CHECK (vkBeginCommandBuffer (/*commandBuffer =>*/ Download_Command,
                                        /*pBeginInfo    =>*/ &(VkCommandBufferBeginInfo){
                                          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                          .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));

        // Transition_To_Transfer_Source:
        Image_Layout_Barrier (/*Command_Buffer     =>*/ Download_Command,
                              /*Image              =>*/ Postprocess_Output_Image.Image,
                              /*Old_Layout         =>*/ VK_IMAGE_LAYOUT_GENERAL,
                              /*New_Layout         =>*/ VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              /*Source_Access      =>*/ VK_ACCESS_SHADER_WRITE_BIT,
                              /*Destination_Access =>*/ VK_ACCESS_TRANSFER_READ_BIT,
                              /*Source_Stage       =>*/ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              /*Destination_Stage  =>*/ VK_PIPELINE_STAGE_TRANSFER_BIT);

        // Copy_Image_To_Buffer:
        vkCmdCopyImageToBuffer (Download_Command, Postprocess_Output_Image.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          Readback.Buffer, 1, &(VkBufferImageCopy){
            .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
            .imageExtent      = {(uint32_t)Render_Width, (uint32_t)Render_Height, 1}});

        // Transition_Back_To_General:
        Image_Layout_Barrier (/*Command_Buffer     =>*/ Download_Command,
                              /*Image              =>*/ Postprocess_Output_Image.Image,
                              /*Old_Layout         =>*/ VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              /*New_Layout         =>*/ VK_IMAGE_LAYOUT_GENERAL,
                              /*Source_Access      =>*/ VK_ACCESS_TRANSFER_READ_BIT,
                              /*Destination_Access =>*/ VK_ACCESS_SHADER_WRITE_BIT,
                              /*Source_Stage       =>*/ VK_PIPELINE_STAGE_TRANSFER_BIT,
                              /*Destination_Stage  =>*/ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        // Submit_And_Wait:
        VK_CHECK (vkEndCommandBuffer (Download_Command));
        VK_CHECK (vkQueueSubmit (/*queue       =>*/ Queue,
                                 /*submitCount =>*/ 1,
                                 /*pSubmits    =>*/ &(VkSubmitInfo){
                                   .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                   .commandBufferCount = 1,
                                   .pCommandBuffers    = &Download_Command},
                                 /*fence       =>*/ VK_NULL_HANDLE));
        vkQueueWaitIdle (Queue);

        // Map_And_Write_TGA:
        uint16_t *Pixels;
        vkMapMemory (Device, Readback.Memory, 0, Pixel_Buffer_Size, 0, (void **)&Pixels);
        char Path[512];
        snprintf (Path, sizeof (Path), "%s/frame_%04d.tga", Dump_Frames_Dir, F);
        FILE *Tga_File = fopen (Path, "wb");
        if (Tga_File) {
          uint8_t Header[18] = {0};
          Header[2]  = 2;
          Header[12] = Render_Width & 0xFF;  Header[13] = (Render_Width >> 8) & 0xFF;
          Header[14] = Render_Height & 0xFF; Header[15] = (Render_Height >> 8) & 0xFF;
          Header[16] = 32; Header[17] = 0x20;
          fwrite (Header, 1, 18, Tga_File);
          for (int Y = 0; Y < Render_Height; Y++)
            for (int X = 0; X < Render_Width; X++) {
              uint16_t *P = Pixels + (Y * Render_Width + X) * 4;
              uint8_t BGRA[4];
              for (int C = 0; C < 3; C++) {
                uint16_t H = P[C]; uint32_t Exp = (H >> 10) & 0x1F; uint32_t Man = H & 0x3FF;
                float V; if (Exp == 0) V = (float)Man / 1024.0f * (1.0f / 16384.0f);
                else if (Exp == 31) V = 1.0f;
                else { uint32_t Fb = ((Exp + 112) << 23) | (Man << 13); memcpy (&V, &Fb, 4); }
                if (V < 0.0f) V = 0.0f;
                if (V > 1.0f) V = 1.0f;
                float S = (V <= 0.0031308f) ? V * 12.92f : 1.055f * powf (V, 1.0f / 2.4f) - 0.055f;
                BGRA[2 - C] = (uint8_t)(S * 255.0f + 0.5f);
              }
              BGRA[3] = 255;
              fwrite (BGRA, 1, 4, Tga_File);
            }
          fclose (Tga_File);
        }
        vkUnmapMemory (Device, Readback.Memory);
        vkDestroyBuffer (Device, Readback.Buffer, NULL);
        vkFreeMemory (Device, Readback.Memory, NULL);
        vkFreeCommandBuffers (Device, Command_Pool, 1, &Download_Command);
      }
    }

    // Wait for GPU to finish
    vkDeviceWaitIdle (Device);

    // Compute percentiles: sort frame times for P50/P95/P99
    for (int I = 0; I < Total_Frames - 1; I++)
      for (int J = I + 1; J < Total_Frames; J++)
        if (Frame_Times[I] > Frame_Times[J]) { float T = Frame_Times[I]; Frame_Times[I] = Frame_Times[J]; Frame_Times[J] = T; }
    float P50 = Frame_Times[Total_Frames / 2];
    float P95 = Frame_Times[(int)(Total_Frames * 0.95f)];
    float P99 = Frame_Times[(int)(Total_Frames * 0.99f)];
    free (Frame_Times);

    // Print benchmark results
    float Avg_Ms  = Frame_Sum / Total_Frames;
    float Avg_Fps = 1000.f / Avg_Ms;
    printf ("\n[benchmark] ════════════════════════════════════════════════\n");
    printf ("  Resolution:    %dx%d (render %dx%d, scale %.0f%%)\n",
            Width, Height, Render_Width, Render_Height, Active_Render_Scale * 100.f);
    printf ("  Frames:        %d\n", Total_Frames);
    printf ("  PBR maps:      %s\n", PBR_Stride > 0 ? "ON" : "OFF (heuristic)");
    printf ("  Post-process:  %s\n", No_Postprocess ? "OFF" : "ON");
    printf ("  Avg frame:     %.2f ms (%.1f fps)\n", Avg_Ms, Avg_Fps);
    printf ("  P50 frame:     %.2f ms (%.1f fps)\n", P50, 1000.f / P50);
    printf ("  P95 frame:     %.2f ms (%.1f fps)\n", P95, 1000.f / P95);
    printf ("  P99 frame:     %.2f ms (%.1f fps)\n", P99, 1000.f / P99);
    printf ("  Min frame:     %.2f ms (%.1f fps)\n", Frame_Min, 1000.f / Frame_Min);
    printf ("  Max frame:     %.2f ms (%.1f fps)\n", Frame_Max, 1000.f / Frame_Max);
    printf ("  Total time:    %.2f s\n", Frame_Sum / 1000.f);
    printf ("[benchmark] ════════════════════════════════════════════════\n");

    // Screenshot mode: read back the display output image and write a TGA
    if (Screenshot_Path) {
      printf ("[screenshot] saving to %s...\n", Screenshot_Path);

      // Allocate_Readback_Buffer: R16G16B16A16_SFLOAT = 8 bytes/pixel
      uint64_t Pixel_Size = (uint64_t)Render_Width * Render_Height * 8;
      Gpu_Buffer Readback = Buffer_Allocate (/*Size         =>*/ Pixel_Size,
                                             /*Usage        =>*/ VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                             /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                               | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      // Record_Download_Command:
      VkCommandBuffer Cmd;
      VK_CHECK (vkAllocateCommandBuffers (/*device        =>*/ Device,
                                          /*pAllocateInfo =>*/ &(VkCommandBufferAllocateInfo){
                                            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                            .commandPool        = Command_Pool,
                                            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                            .commandBufferCount = 1},
                                          /*pCommandBuffers =>*/ &Cmd));
      VK_CHECK (vkBeginCommandBuffer (/*commandBuffer =>*/ Cmd,
                                      /*pBeginInfo    =>*/ &(VkCommandBufferBeginInfo){
                                        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));

      // Transition_To_Transfer_Source:
      Image_Layout_Barrier (/*Command_Buffer     =>*/ Cmd,
                            /*Image              =>*/ Postprocess_Output_Image.Image,
                            /*Old_Layout         =>*/ VK_IMAGE_LAYOUT_GENERAL,
                            /*New_Layout         =>*/ VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            /*Source_Access      =>*/ VK_ACCESS_SHADER_WRITE_BIT,
                            /*Destination_Access =>*/ VK_ACCESS_TRANSFER_READ_BIT,
                            /*Source_Stage       =>*/ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            /*Destination_Stage  =>*/ VK_PIPELINE_STAGE_TRANSFER_BIT);

      // Copy_Image_To_Buffer:
      vkCmdCopyImageToBuffer (Cmd, Postprocess_Output_Image.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        Readback.Buffer, 1, &(VkBufferImageCopy){
          .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
          .imageExtent      = {(uint32_t)Render_Width, (uint32_t)Render_Height, 1}});

      // Transition_Back_To_General:
      Image_Layout_Barrier (/*Command_Buffer     =>*/ Cmd,
                            /*Image              =>*/ Postprocess_Output_Image.Image,
                            /*Old_Layout         =>*/ VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            /*New_Layout         =>*/ VK_IMAGE_LAYOUT_GENERAL,
                            /*Source_Access      =>*/ VK_ACCESS_TRANSFER_READ_BIT,
                            /*Destination_Access =>*/ VK_ACCESS_SHADER_WRITE_BIT,
                            /*Source_Stage       =>*/ VK_PIPELINE_STAGE_TRANSFER_BIT,
                            /*Destination_Stage  =>*/ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

      // Submit_And_Wait:
      VK_CHECK (vkEndCommandBuffer (Cmd));
      VK_CHECK (vkQueueSubmit (/*queue       =>*/ Queue,
                               /*submitCount =>*/ 1,
                               /*pSubmits    =>*/ &(VkSubmitInfo){
                                 .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                 .commandBufferCount = 1,
                                 .pCommandBuffers    = &Cmd},
                               /*fence       =>*/ VK_NULL_HANDLE));
      vkQueueWaitIdle (Queue);

      // Map the readback buffer and write a TGA file. Storage is R16G16B16A16_SFLOAT - convert fp16 to 8-bit sRGB for TGA output
      uint16_t *Pixels_F16;
      vkMapMemory (Device, Readback.Memory, 0, Pixel_Size, 0, (void **)&Pixels_F16);

      // Write pixel data to TGA file
      FILE *TGA = fopen (Screenshot_Path, "wb");
      if (TGA) {
        // TGA header (18 bytes)
        uint8_t Header[18] = {0};
        Header[2]  = 2;  // Uncompressed true-color
        Header[12] = Render_Width & 0xFF;  Header[13] = (Render_Width >> 8) & 0xFF;
        Header[14] = Render_Height & 0xFF; Header[15] = (Render_Height >> 8) & 0xFF;
        Header[16] = 32;  // 32 bpp (BGRA)
        Header[17] = 0x20; // Top-left origin
        fwrite (Header, 1, 18, TGA);

        // Write pixels: fp16 linear > clamp > linear-to-sRGB > 8-bit BGRA for TGA
        for (int Y = 0; Y < Render_Height; Y++) {
          for (int X = 0; X < Render_Width; X++) {
            uint16_t *P = Pixels_F16 + (Y * Render_Width + X) * 4;

            // Convert fp16 to float (use half-to-float bit manipulation)
            uint8_t BGRA[4];
            for (int C = 0; C < 3; C++) {
              // IEEE 754 fp16 to fp32 conversion
              uint16_t H = P[C];
              uint32_t Sign = (uint32_t)(H >> 15) << 31;
              uint32_t Exp  = (H >> 10) & 0x1F;
              uint32_t Man  = H & 0x3FF;
              float V;
              if (Exp == 0) V = (Man == 0) ? 0.0f : (float)Man / 1024.0f * (1.0f / 16384.0f);
              else if (Exp == 31) V = 1.0f;
              else { uint32_t F = Sign | ((Exp + 112) << 23) | (Man << 13); memcpy (&V, &F, 4); }

              // Clamp and apply sRGB gamma
              if (V < 0.0f) V = 0.0f;
              if (V > 1.0f) V = 1.0f;
              float S = (V <= 0.0031308f) ? V * 12.92f : 1.055f * powf (V, 1.0f / 2.4f) - 0.055f;
              BGRA[2 - C] = (uint8_t)(S * 255.0f + 0.5f);  // RGB > BGR for TGA
            }
            BGRA[3] = 255;
            fwrite (BGRA, 1, 4, TGA);
          }
        }
        fclose (TGA);
        printf ("[screenshot] saved %dx%d to %s\n", Render_Width, Render_Height, Screenshot_Path);
      }

      // Clean up readback resources
      vkUnmapMemory (Device, Readback.Memory);
      vkDestroyBuffer (Device, Readback.Buffer, NULL);
      vkFreeMemory (Device, Readback.Memory, NULL);
      vkFreeCommandBuffers (Device, Command_Pool, 1, &Cmd);
    }

    // Wait for GPU and exit
    vkDeviceWaitIdle (Device);
    return 0;
  }

  // Game loop
  Camera   Cam   = {.Position = {Spawn_Point.Origin.x, Spawn_Point.Origin.y + 8.f + DEFAULT_VIEW_HEIGHT, Spawn_Point.Origin.z},
                     .Yaw = 1.5707963f - Spawn_Point.Angle * 3.14159f / 180.f};
  Prev_View_Matrix = View (Cam.Position, Cam.Yaw, Cam.Pitch);
  uint64_t Last  = SDL_GetPerformanceCounter ();
  uint64_t Freq  = SDL_GetPerformanceFrequency ();
  uint     Frame = 0;
  float    Total_Time = 0;
  Quit = 0;

  // Main game loop
  while (not Quit) {

    // Wait for the previous frame's RT submission to complete before reusing Command_Buffer
    VK_CHECK (vkWaitForFences (Device, 1, &Fence, VK_TRUE, UINT64_MAX));

    // Compute delta time from the high-resolution performance counter
    uint64_t Now = SDL_GetPerformanceCounter ();
    float    Delta_Time = (float)(Now - Last) / (float)Freq;
    if (Delta_Time > MAX_DELTA_TIME) Delta_Time = MAX_DELTA_TIME;
    Last = Now;
    Total_Time += Delta_Time;

    // Poll input (handles windowing events, mode transitions, resize)
    Input In = Poll_Input ();

    // Skip rendering when minimized (window may be 0x0, nothing to present)
    if (Current_Activated == MINIMIZE_DEACTIVATED) {
      SDL_Delay (16);  // Don't spin-wait when minimized
      Last = SDL_GetPerformanceCounter ();
      continue;
    }

    // Recreate swapchain if window was resized or fullscreen was toggled
    if (Swapchain_Dirty) {
      Vulkan_Recreate_Swapchain ();
      Swapchain_Dirty = 0;
    }

    // Spawn projectile on fire and upload to GPU before physics dispatch
    if (In.Fire) {
      float sy = sinf (Cam.Yaw), cy = cosf (Cam.Yaw);
      float sp = sinf (Cam.Pitch), cp = cosf (Cam.Pitch);
      vec3 Forward = {sy * cp, -sp, -cy * cp};
      Projectile_Spawn (Cam.Position, Forward);
    }
    Projectiles.Fire_Cooldown -= Delta_Time;
    if (Projectiles.Fire_Cooldown < 0) Projectiles.Fire_Cooldown = 0;
    Projectile_Pool_Upload ();

    // Dispatch GPU physics simulation
    Player Physics   = Physics_Dispatch (In, Delta_Time);

    // Read back projectile state after GPU physics step
    Projectile_Pool_Readback ();

    // Update audio: footsteps, landing sounds
    Audio_Update_Footsteps (&Physics, Delta_Time);

    // Update the camera from the physics result - add View_Height to get eye position
    Cam.Position    = Physics.Position;
    Cam.Position.y += Physics.View_Height; // Raise camera from feet to eye level (26 units standing)
    Cam.Yaw         = Physics.Yaw;
    Cam.Pitch       = Physics.Pitch;
    Cam.Frame       = Frame;

    // Animate and rebuild the weapon viewmodel
    Weapon_Update (&Weapon, &Cam, Delta_Time, In.Fire);
    Weapon_Bottom_Level_Rebuild (&Weapon);

    // Advance enemy idle animation and rebuild BLAS
    Enemy.Animation_Time += Delta_Time;
    {
      int Frame_Index = (int)(Enemy.Animation_Time * Enemy.Frame_FPS) % (int)Enemy.Frame_Count;
      Enemy.Current_Vertices = Enemy.Frame_Vertices[Frame_Index];
    }
    Entity_Bottom_Level_Rebuild (&Enemy);

    // Compute player body TLAS transform
    float Body_Yaw = -Physics.Yaw;  // GL entity yaw = -Player.Yaw (facing convention)
    float D_Yaw = Body_Yaw - Enemy.GL_Yaw;
    float Cosine_Yaw = cosf (D_Yaw), Sine_Yaw = sinf (D_Yaw);
    vec3  Entity_Origin = Enemy.GL_Origin;
    float Translation_X = Physics.Position.x - (Cosine_Yaw * Entity_Origin.x + Sine_Yaw * Entity_Origin.z);
    float Translation_Y = Physics.Position.y - Entity_Origin.y;
    float Translation_Z = Physics.Position.z - (-Sine_Yaw * Entity_Origin.x + Cosine_Yaw * Entity_Origin.z);
    float Player_Body_Transform[12] = {
      Cosine_Yaw,  0.f, Sine_Yaw,  Translation_X,
      0.f,         1.f, 0.f,       Translation_Y,
      -Sine_Yaw,   0.f, Cosine_Yaw, Translation_Z};

    // Rebuild the top-level acceleration structure
    Top_Level_Rebuild (&World_Bottom_Level, &Weapon.Bottom_Level, &Enemy.Bottom_Level, Player_Body_Transform);

    // Adaptive quality budget — target frame time depends on quality tier
    float Target_Frame_Time = (Active_Quality == QUALITY_POTATO) ? 0.033f : 0.016f;
    float Budget = 0.0f;
    if (Force_Cheap) {
      Budget = 1.0f;
    } else if (Delta_Time > Target_Frame_Time) {
      Budget = (Delta_Time - Target_Frame_Time) / (0.15f - Target_Frame_Time);
      if (Budget > 1.0f) Budget = 1.0f;
    }
    uint Budget_Byte = (uint)(Budget * 255.0f);
    Current_Budget_Byte = (int)Budget_Byte;
    uint Packed_SPP = (Active_SPP & 0xFF) | (Budget_Byte << 8);

    // Upload the camera and dispatch ray tracing + postprocess
    Camera_Upload (&Cam, FIELD_OF_VIEW, Weapon.Texture_Base_Index, PBR_Stride, Packed_SPP);
    float Horizontal_Speed = sqrtf (Physics.Velocity.x * Physics.Velocity.x +
                           Physics.Velocity.z * Physics.Velocity.z);

    // Build reprojection matrix: Proj * Prev_View * Inverse_View (maps current view-space > previous clip-space)
    mat4 Cur_View = View (Cam.Position, Cam.Yaw, Cam.Pitch);
    mat4 Cur_Inv_View = Inverse_Orthogonal (Cur_View);
    mat4 Proj = Perspective (FIELD_OF_VIEW, (float)Width / Height, 0.1f, 10000.f);
    mat4 Reproject = Mat4_Mul (Proj, Mat4_Mul (Prev_View_Matrix, Cur_Inv_View));
    mat4 Inv_Proj = Inverse_Projection (Proj);

    // Project sun direction to screen space for god rays
    vec3 Sun_D = Normalize (Active_Environment.Sun_Direction);
    vec3 Sun_World = Add (Cam.Position, Scale (Sun_D, 1000.f));  // Far point in sun direction

    // Transform through View * Proj to get clip space
    mat4 View_Projection = Mat4_Mul (Proj, Cur_View);
    float Clip_X = View_Projection.E[0]*Sun_World.x + View_Projection.E[4]*Sun_World.y + View_Projection.E[8]*Sun_World.z  + View_Projection.E[12];
    float Clip_Y = View_Projection.E[1]*Sun_World.x + View_Projection.E[5]*Sun_World.y + View_Projection.E[9]*Sun_World.z  + View_Projection.E[13];
    float Clip_W = View_Projection.E[3]*Sun_World.x + View_Projection.E[7]*Sun_World.y + View_Projection.E[11]*Sun_World.z + View_Projection.E[15];
    float Sun_U = 0.5f, Sun_V = 0.5f;
    float Sun_Visible = 0.f;
    if (Clip_W > 0.01f) {  // Sun is in front of camera
      Sun_U = (Clip_X / Clip_W) * 0.5f + 0.5f;
      Sun_V = (Clip_Y / Clip_W) * 0.5f + 0.5f;

      // Check if sun is roughly on screen (with margin for off-screen glow)
      if (Sun_U > -0.5f and Sun_U < 1.5f and Sun_V > -0.5f and Sun_V < 1.5f)
        Sun_Visible = 1.f;
    }

    // Build post-processing push constants and render frame
    Gpu_Postprocess_Push Postprocess = {
      .Time           = Total_Time,
      .Dt_Frame       = (uint32_t)Float_To_Half (Delta_Time) | ((uint32_t)(Frame_Count & 0xFFFF) << 16),
      .Velocity       = Pack_Half2x16 (Physics.Velocity.x, Physics.Velocity.z),
      .Speed_Exposure = Pack_Half2x16 (Horizontal_Speed, STYLE.Exposure),
      .Bloom_Vignette = Pack_Half2x16 (STYLE.Bloom_Strength, STYLE.Vignette),
      .Inv_Proj_Diag  = Pack_Half2x16 (Inv_Proj.E[0], Inv_Proj.E[5]),
      .Sun_Screen_Pos = Pack_Half2x16 (Sun_U, Sun_V),
      .Sun_Params     = Pack_Half2x16 (0.15f, Sun_Visible)};  // God ray intensity
    Pack_Mat4_Half (&Reproject, Postprocess.Reproject);
    Raytracing_Frame (Postprocess);
    Prev_View_Matrix = Cur_View;
    Frame++;
    Frame_Count++;
  }

  // Cleanup
  Audio_Shutdown ();
  Damage_Cache_Free ();
  vkDeviceWaitIdle (Device);
  printf ("[shutdown] %u frames rendered\n", Frame);

  // Free SDL cursors
  if (SDL_Cursor_Arrow)     SDL_FreeCursor (SDL_Cursor_Arrow);
  if (SDL_Cursor_Hand)      SDL_FreeCursor (SDL_Cursor_Hand);
  if (SDL_Cursor_Crosshair) SDL_FreeCursor (SDL_Cursor_Crosshair);

  // Free scene data
  free (Scene_Data.Vertices);
  free (Scene_Data.Indices);
  free (Scene_Data.Materials);
  free (Scene_Data.Texture_Ids);
  free (Scene_Data.Texture_Names);
  if (Scene_Data.Lightmap_Atlas) free (Scene_Data.Lightmap_Atlas);

  // Pipelines and layouts
  vkDestroyPipeline            (Device, Denoise_Pipeline, NULL);
  vkDestroyPipelineLayout      (Device, Denoise_Pipeline_Layout, NULL);
  vkDestroyDescriptorPool      (Device, Denoise_Descriptor_Pool, NULL);
  vkDestroyDescriptorSetLayout (Device, Denoise_Descriptor_Layout, NULL);
  vkDestroyImageView           (Device, Denoise_Ping_Image.View, NULL);
  vkDestroyImage               (Device, Denoise_Ping_Image.Image, NULL);
  vkFreeMemory                 (Device, Denoise_Ping_Image.Memory, NULL);
  vkDestroyPipeline            (Device, Postprocess_Pipeline, NULL);
  vkDestroyPipelineLayout      (Device, Postprocess_Pipeline_Layout, NULL);
  vkDestroyDescriptorPool      (Device, Postprocess_Descriptor_Pool, NULL);
  vkDestroyDescriptorSetLayout (Device, Postprocess_Descriptor_Layout, NULL);
  vkDestroyPipeline            (Device, Physics_Pipeline, NULL);
  vkDestroyPipelineLayout      (Device, Physics_Pipeline_Layout, NULL);
  vkDestroyDescriptorPool      (Device, Physics_Descriptor_Pool, NULL);
  vkDestroyDescriptorSetLayout (Device, Physics_Descriptor_Layout, NULL);
  vkDestroyPipeline            (Device, Pipeline, NULL);
  vkDestroyPipelineLayout      (Device, Pipeline_Layout, NULL);
  vkDestroyDescriptorPool      (Device, Descriptor_Pool, NULL);
  vkDestroyDescriptorSetLayout (Device, Descriptor_Set_Layout, NULL);
  vkDestroyPipelineCache       (Device, Pipeline_Cache, NULL);

  // Acceleration structures
  vkDestroyAccelerationStructure (Device, Top_Level.Handle, NULL);
  vkDestroyBuffer (Device, Top_Level.Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Top_Level.Buffer.Memory, NULL);
  vkDestroyBuffer (Device, Top_Level_Instance_Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Top_Level_Instance_Buffer.Memory, NULL);
  vkDestroyBuffer (Device, Top_Level_Scratch_Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Top_Level_Scratch_Buffer.Memory, NULL);
  vkDestroyAccelerationStructure (Device, Bottom_Level.Handle, NULL);
  vkDestroyBuffer (Device, Bottom_Level.Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Bottom_Level.Buffer.Memory, NULL);

  // Weapon resources
  vkDestroyAccelerationStructure (Device, Weapon.Bottom_Level.Handle, NULL);
  vkDestroyBuffer (Device, Weapon.Bottom_Level.Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Weapon.Bottom_Level.Buffer.Memory, NULL);
  vkDestroyBuffer (Device, Weapon.Bottom_Level_Scratch.Buffer, NULL);
  vkFreeMemory    (Device, Weapon.Bottom_Level_Scratch.Memory, NULL);
  vkDestroyBuffer (Device, Weapon.Vertex_Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Weapon.Vertex_Buffer.Memory, NULL);
  vkDestroyBuffer (Device, Weapon.Index_Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Weapon.Index_Buffer.Memory, NULL);
  vkDestroyBuffer (Device, Weapon.Texture_Id_Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Weapon.Texture_Id_Buffer.Memory, NULL);
  free (Weapon.Model.Vertices);
  free (Weapon.Model.Indices);
  free (Weapon.Model.Texture_Ids);
  free (Weapon.Transformed_Vertices);

  // Entity (enemy) resources
  vkDestroyAccelerationStructure (Device, Enemy.Bottom_Level.Handle, NULL);
  vkDestroyBuffer (Device, Enemy.Bottom_Level.Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Enemy.Bottom_Level.Buffer.Memory, NULL);
  vkDestroyBuffer (Device, Enemy.Bottom_Level_Scratch.Buffer, NULL);
  vkFreeMemory    (Device, Enemy.Bottom_Level_Scratch.Memory, NULL);
  vkDestroyBuffer (Device, Enemy.Vertex_Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Enemy.Vertex_Buffer.Memory, NULL);
  vkDestroyBuffer (Device, Enemy.Index_Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Enemy.Index_Buffer.Memory, NULL);
  vkDestroyBuffer (Device, Enemy.Texture_Id_Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Enemy.Texture_Id_Buffer.Memory, NULL);
  for (uint I = 0; I < Enemy.Frame_Count; I++) free (Enemy.Frame_Vertices[I]);
  free (Enemy.Indices);
  free (Enemy.Texture_Ids);

  // Shader binding table
  vkDestroyBuffer (Device, Shader_Binding_Table_Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Shader_Binding_Table_Buffer.Memory, NULL);

  // GPU storage images
  vkDestroyImageView (Device, Raytracing_Storage_Image.View, NULL);
  vkDestroyImage     (Device, Raytracing_Storage_Image.Image, NULL);
  vkFreeMemory       (Device, Raytracing_Storage_Image.Memory, NULL);
  vkDestroyImageView (Device, Depth_Image.View, NULL);
  vkDestroyImage     (Device, Depth_Image.Image, NULL);
  vkFreeMemory       (Device, Depth_Image.Memory, NULL);
  vkDestroyImageView (Device, History_Image.View, NULL);
  vkDestroyImage     (Device, History_Image.Image, NULL);
  vkFreeMemory       (Device, History_Image.Memory, NULL);
  vkDestroyImageView (Device, Postprocess_Output_Image.View, NULL);
  vkDestroyImage     (Device, Postprocess_Output_Image.Image, NULL);
  vkFreeMemory       (Device, Postprocess_Output_Image.Memory, NULL);

  // Scene buffers
  vkDestroyBuffer (Device, Camera_Uniform_Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Camera_Uniform_Buffer.Memory, NULL);
  vkDestroyBuffer (Device, Vertex_Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Vertex_Buffer.Memory, NULL);
  vkDestroyBuffer (Device, Index_Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Index_Buffer.Memory, NULL);
  vkDestroyBuffer (Device, Material_Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Material_Buffer.Memory, NULL);
  vkDestroyBuffer (Device, Texture_Id_Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Texture_Id_Buffer.Memory, NULL);
  vkDestroyBuffer (Device, Player_State_Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Player_State_Buffer.Memory, NULL);
  vkDestroyBuffer (Device, Hull_Storage_Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Hull_Storage_Buffer.Memory, NULL);
  vkDestroyBuffer (Device, Projectile_Buffer.Buffer, NULL);
  vkFreeMemory    (Device, Projectile_Buffer.Memory, NULL);

  // Textures
  for (uint I = 0; I < Texture_Count; I++) {
    vkDestroyImageView (Device, Texture_Views[I], NULL);
    vkDestroyImage     (Device, Texture_Images[I], NULL);
    vkFreeMemory       (Device, Texture_Memories[I], NULL);
  }
  free (Texture_Views);
  free (Texture_Images);
  free (Texture_Memories);
  vkDestroySampler (Device, Texture_Sampler, NULL);

  // Lightmap
  vkDestroyImageView (Device, Lightmap_View, NULL);
  vkDestroyImage     (Device, Lightmap_Image, NULL);
  vkFreeMemory       (Device, Lightmap_Memory, NULL);
  vkDestroySampler   (Device, Lightmap_Sampler, NULL);

  // Swapchain image views
  for (uint I = 0; I < Swapchain_Image_Count; I++)
    vkDestroyImageView (Device, Swapchain_Views[I], NULL);

  // Core Vulkan objects
  vkDestroySemaphore    (Device, Semaphore_Image_Available, NULL);
  vkDestroySemaphore    (Device, Semaphore_Render_Finished, NULL);
  vkDestroyFence        (Device, Fence, NULL);
  vkDestroyCommandPool  (Device, Command_Pool, NULL);
  vkDestroySwapchainKHR (Device, Swapchain, NULL);
  vkDestroyDevice       (Device, NULL);
  vkDestroySurfaceKHR   (Instance, Surface, NULL);
  vkDestroyInstance     (Instance, NULL);

  // Media layer
  SDL_DestroyWindow (Window);
  SDL_Quit ();
  return 0;
  
} // main
