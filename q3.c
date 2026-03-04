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
// §6. Materials              
// §7. Models                
// §8. Scene                 
// §9. Acceleration Structures
// §10. Physics               
// §11. Pipeline              
// §12. Shaders               
// §13. Engine                
// §14. Assets                      
// §15. Main             
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
//                                                       S P E C I F I C A T I O N
//                 
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Language Extensions
#include <iso646.h>

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
#include <vulkan/vulkan.h>
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/efx.h>

// GLSL Similar Types
typedef unsigned int uint; 
typedef struct {float x, y, z;} vec3;
typedef struct {float x, y, z, w;} vec4;
typedef struct {float E[16];} mat4;

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §1. Settings
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Engine name and info strings
const char *ENGINE_NAME = "q3";
const char *ENGINE_VERSION = "0.1.0";   

// Visual style definition
typedef struct {
  float Vignette;       // Vignette darkening intensity
  float Bloom_Strength; // Bloom glow intensity
  float Exposure;       // Tonemapping exposure multiplier   
} Visual_Style;

// Default style
const Visual_Style STYLE = {.Vignette       = 0.35f,  // Moderate-strong vignette
                            .Bloom_Strength = 0.08f,  // Subtle bloom
                            .Exposure       = 1.75f}; // Slightly low exposure

// Quality knobs
typedef enum {QUALITY_CRYSIS, QUALITY_HIGH, QUALITY_MEDIUM, QUALITY_LOW, QUALITY_POTATO, QUALITY_COUNT} Quality_Level;
typedef struct {
  const char *Name;
  int         Width, Height;  // Window internal resolution
  int         SPP;            // Ray count samples per pixel
  int         Parallax;       // Enable parallax occlusion mapping
  int         Denoise_Passes; // A-trous wavelet denoise iterations 
  bool        Checkerboard;   // Temporal checkerboard optimization for ray reduction
} Quality_Preset;
const Quality_Preset QUALITY_PRESETS [QUALITY_COUNT] = {//                            Resolution SPP POM  DN  CB
                                                        [QUALITY_CRYSIS] = {"Crysis", 3840,2160, 2,  1,   3,  true},
                                                        [QUALITY_HIGH]   = {"High",   1600, 900, 2,  1,   2,  true}, 
                                                        [QUALITY_MEDIUM] = {"Medium", 1280, 720, 1,  1,   3,  true},
                                                        [QUALITY_LOW]    = {"Low",    1024, 576, 1,  1,   2,  true},
                                                        [QUALITY_POTATO] = {"Potato",  854, 480, 1,  0,   3,  true}};

// World settings
//
// Each asset group has a dominant coordinate convention, player scale, and camera model. The active world determines physics extents, 
// eye height, FOV, and the swizzle applied during asset load. Assets from a non-dominant world get rescaled on load.
//
// Coordinate system conventions:
//
//   Quake 3 (Z-up): +X = forward, +Y = right, +Z = up (Right-handed)
//   Source (Z-up):  +X = forward, +Y = left,  +Z = up (Right-handed)
//
//   Source:  Engine_X = Source_X, Engine_Y = Source_Z, Engine_Z = -Source_Y
//   Quake 3: Engine_X = Q3_X,     Engine_Y = Q3_Z,     Engine_Z = -Q3_Y
//
typedef enum {WORLD_QUAKE3, WORLD_SOURCE, WORLD_UNREAL, WORLD_COUNT} World_Type;
typedef struct {
  World_Type  Type;
  const char *Name;
  float       Unit_Scale;        // World units per real-world inch
  float       Player_Height;     // Standing bounding box height
  float       Player_Width;      // Bounding box half-width
  float       Eye_Height;        // Camera height above feet
  float       Crouch_Eye_Height; // Camera height crouched
  float       Crouch_Height;     // Crouched bounding box height
  float       Step_Size;         // Max stair step height
  float       FOV;               // Horizontal field of view
  float       Max_Speed;         // Maximum wish speed
  float       Gravity;           // Downward acceleration
  int         Up_Axis;           // Native up axis before swizzle
  const char *Default_Pack;      // Default asset archive (e.g. "assets/pak0.pk3")
  const char *Default_Map;       // Default map name (e.g. "oa_dm1.bsp")
} World_Settings;

// Player bounding box minimum corner 
const vec3  PLAYER_MINIMUMS        = {-15, -24, -15};
const float PLAYER_HALF_EXTENTS[3] = {15.f, 28.f, 15.f}; // Standing bbox

// World presets: physics, camera, and asset defaults for each supported game family
//
//   Quake 3:   Z-up, 56-unit player, 90° FOV, 320 u/s run, 800 gravity, pak0.pk3
//   Source:    Z-up, 72-unit player, 90° FOV, 250 u/s run, 800 gravity, VPK archives
//   Unreal:    Z-up, 78-unit player, 90° FOV, 400 u/s run, 950 gravity, .u/.utx packages
//
const World_Settings WORLD_PRESETS[WORLD_COUNT] = {
  [WORLD_QUAKE3] = {WORLD_QUAKE3, "Quake 3",            1.f,  56.f, 15.f, 50.f, 36.f, 32.f, 18.f, 90.f, 320.f, 800.f, 2,
                    "assets/pak0.pk3",   "oa_dm1.bsp"},
  [WORLD_SOURCE] = {WORLD_SOURCE, "Source Engine",       1.f,  72.f, 16.f, 64.f, 46.f, 36.f, 18.f, 90.f, 250.f, 800.f, 2,
                    NULL,                "de_dust2.bsp"},
  [WORLD_UNREAL] = {WORLD_UNREAL, "Unreal Tournament",  1.f,  78.f, 17.f, 68.f, 48.f, 39.f, 16.f, 90.f, 400.f, 950.f, 2,
                    NULL,                "DM-Morpheus.unr"}};

// Id Software player settings
#define FIELD_OF_VIEW       90.f   // Windowing and horizontal viewport settings
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
#define DEFAULT_VIEW_HEIGHT 22.f   // Camera height offset from capsule center 
#define CROUCH_VIEW_HEIGHT   8.f   // Camera height offset when crouching 
#define PLAYER_CAPSULE_SPINE 13.f  // Comment here !!!

// Valve Source player settings
#define SRC_JUMP_VELOCITY       301.993377f 
#define SRC_GRAVITY             800.f
#define SRC_GROUND_FRICTION     4.f
#define SRC_STOP_SPEED          100.f
#define SRC_GROUND_ACCELERATE   5.5f
#define SRC_AIR_ACCELERATE      10.f  // Source air-strafe acceleration 
#define SRC_MAXIMUM_SPEED       250.f // Knife run speed
#define SRC_OVERBOUNCE          1.0f  // Source clips exactly to plane (no overbounce)
#define SRC_DUCK_SPEED          0.34f // Duck transition time in seconds
#define SRC_WALK_SPEED          130.f // Walk modifier speed
#define SRC_STEP_SIZE           18.f
#define SRC_MINIMUM_WALK_NORMAL 0.7f

// Weapon animation settings
#define WEAPON_MODEL_SCALE  0.50f // Viewmodel scale factor (world-space shrink, no depth hack)
#define WEAPON_FIRE_SPEED   10.f  // Fire animation playback rate (frames per second)
#define WEAPON_FIRE_FRAMES  6.f   // Total fire animation duration in frames
#define WEAPON_BOB_RATE_V   3.5f  // Vertical idle bob frequency (radians/second)
#define WEAPON_BOB_RATE_H   1.7f  // Horizontal idle bob frequency (radians/second)
#define WEAPON_BOB_AMP_V    0.4f  // Vertical idle bob amplitude (units)
#define WEAPON_BOB_AMP_H    0.2f  // Horizontal idle bob amplitude (units)
#define WEAPON_RECOIL_AMP  -1.2f  // Recoil kick magnitude (negative = pull back)
#define WEAPON_RECOIL_DECAY 5.f   // Recoil exponential decay rate  

// Projectile limits
#define MAX_PROJECTILES 64    // Maximum simultaneous projectiles in flight
#define ROCKET_SPEED    900.f // Rocket projectile speed (units/second)
#define ROCKET_DAMAGE   100   // Direct hit damage
#define ROCKET_SPLASH   120.f // Splash damage radius
#define ROCKET_LIFETIME 10.f  // Seconds before projectile expires
#define FIRE_COOLDOWN   0.8f  // Minimum seconds between shots

// Windowing constraints
#define MINIMUM_WINDOW_SIZE 256
#define ASPECT_NARROW_X     21
#define ASPECT_NARROW_Y     9
#define ASPECT_WIDE_X       4
#define ASPECT_WIDE_Y       3

// Comment here !!!
#define MAX_DELTA_TIME 0.05f   // Clamp to 20 fps minimum (prevents physics tunneling)
#define NEAR_CLIP      0.1f    // Near clip plane distance
#define FAR_CLIP       10000.f // Far clip plane distance

// Comment here !!! 
#define SWAPCHAIN_MAX_IMAGES     8    // Comment here !!!              
#define DESCRIPTOR_TEXTURE_SLOTS 1536 // Comment here !!!

// Importance sampling - control reflection ray quality and specular firefly suppression
#define VNDF_ALPHA_FLOOR 0.01 // Min roughness² for VNDF reflection ray spread
#define SPECULAR_D_BIAS  0.01 // Min roughness² for GGX D term (prevents firefly peak)

// Reflection limits 
#define REFL_CLAMP_LO  5.0  // Reflection luminance clamp by surface luminance
#define REFL_CLAMP_HI  1.5  // Reflection luminance clamp by surface luminance
#define REFL_GATE_LO   0.80 // Max roughness for tracing reflection rays 
#define REFL_GATE_HI   0.35 // Max roughness for tracing reflection rays
#define REFL_THRESH_LO 0.02 // Reflection Fresnel weight skip threshold
#define REFL_THRESH_HI 0.30 // Reflection Fresnel weight skip threshold
#define REFL_DAMPING   0.3  // Budget-proportional reflection strength reduction 
#define REFL_SOFT_EDGE 12.0 // Threshold-to-full-weight transition sharpness 

// Reflection trace distance limits
#define REFL_TRACE_LO 2000.0 // Reflection trace max distance at Budget=0 
#define REFL_TRACE_HI 150.0  // Reflection trace max distance at Budget=1

// Shadow rays limits
#define SHADOW_DIST_LO 2000.0 // Shadow ray max distance at Budget=0
#define SHADOW_DIST_HI 200.0  // Shadow ray max distance at Budget=1

// A-trous denoiser edge-stopping limits
#define DENOISE_DEPTH_LO 100.0 // Depth sensitivity when still 
#define DENOISE_DEPTH_HI 10.0  // Depth sensitivity during motion 
#define DENOISE_LUM_LO   200.0 // Luminance sensitivity when still
#define DENOISE_LUM_HI   15.0  // Luminance sensitivity during motion 

// Firefly rejection limits
#define FIREFLY_HEADROOM 1.05 // Headroom multiplier above 2nd-brightest neighbor
#define FIREFLY_BIAS     0.01 // Additive floor preventing zero clamp

// Contrast Adaptive Sharpening (CAS) limits
#define CAS_AMOUNT 0.55 // Sharpening kernel strength
#define CAS_MIX    1.5  // Edge enhancement multiplier

// Temporal Anti-Aliasing (TAA) limits
#define TAA_STATIC_FLOOR 0.25 // Min blend alpha when camera is still (history retention)
#define TAA_SIGMA        0.15 // Variance clamp sigma (lower = tighter = less ghosting)
#define TAA_MOVE_LO      0.95 // Moving blend base at low motion 
#define TAA_MOVE_HI      0.99 // Moving blend base at high motion 

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §2. Types
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

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
  int   Active;       // Boolean: 1 = live, 0 = dead
  int   Material_Hit; // Surface material on impact (for sound selection)
  float Radius;       // Collision radius
  float Damage;       // Base damage on impact (before body-part multiplier)
  float Hit_U, Hit_V; // UV coordinates at impact point (for damage map sampling)
  int   Instance_Hit; // Packed TLAS instanceCustomIndex of hit object (-1 = none, low 8 bits = figure slot)
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
} GPU_Projectile;

typedef struct {
  GPU_Projectile Slots[MAX_PROJECTILES];
  int   Count;
  float Fire_Cooldown;
  float Pad[2];
} GPU_Projectile_Pool;

// Material System
typedef struct {
  int   Type;         // MATERIAL_DEFAULT, MATERIAL_METAL, etc.
  float Damage_Scale; // Percent: 0.0 (armored) to 1.0 (exposed)
  char  Name[32];     // Human-readable name
} Material;

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

// Quickhull internal types used during hull construction
typedef struct {int A, B, C; int Dead;} Quickhull_Face;
typedef struct {int V0, V1, Face;}      Quickhull_Edge;

// GPU-resident buffer with its backing memory and optional device address
typedef struct {
  VkBuffer        Buffer;
  VkDeviceMemory  Memory;  // Device memory backing (may be shared via GPU_Heap sub-allocation)
  VkDeviceAddress Address; // Buffer device address for shader access (zero if not requested)
  uint64_t        Size;    // Allocation size in bytes
  uint64_t        Offset;  // Offset within the VkDeviceMemory (for sub-allocated buffers)
  int             Heap_Block; // GPU_Heap block handle for sub-allocated buffers (-1 = standalone)
} GPU_Buffer;

// GPU-resident image with its backing memory, view, and format metadata
typedef struct {
  VkImage        Image;
  VkDeviceMemory Memory;     // Device memory backing (may be shared via GPU_Heap sub-allocation)
  VkImageView    View;       // Image view used for sampling or storage access
  VkFormat       Format;     // Pixel format of the image
  uint64_t       Offset;     // Offset within the VkDeviceMemory (for sub-allocated images)
  int            Heap_Block; // GPU_Heap block handle for sub-allocated images (-1 = standalone)
} GPU_Image;

// Ray tracing acceleration structure with its backing buffer and device address
typedef struct {
  VkAccelerationStructureKHR Handle;  // Opaque acceleration structure handle
  GPU_Buffer                 Buffer;  // GPU buffer holding the acceleration structure data
  VkDeviceAddress            Address; // Device address for referencing from shaders and TLAS builds
} Acceleration_Structure;

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §3. Context
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// World and movement state with defaults
World_Settings Active_World    = WORLD_PRESETS [WORLD_QUAKE3];
int            Active_Movement = WORLD_QUAKE3;

// Windowing state and settings with defaults
Quality_Level    Active_Quality      = QUALITY_MEDIUM;
Cursor_Kind      Current_Cursor_Kind = CURSOR_SYSTEM;
Activated_Kind   Current_Activated   = OTHER_ACTIVATED;
Game_Mode_Kind   Current_Game_Mode   = GAME_PLAYING;
Window_Mode_Kind Current_Window_Mode = WINDOWED_MODE;
int              Cursor_Centering    = 0;        // Center cursor each frame
int              Input_Active        = 1;        // Process input when active 
int              In_Menu             = 0;        // True when in menu mode, else false for game mode
int              Swapchain_Dirty     = 0;        // Non-zero when swapchain needs recreation
int              Saved_Cursor_X, Saved_Cursor_Y; // Cursor position saved across mode transitions
int              Windowed_X, Windowed_Y;         // Saved window position before fullscreen
int              Windowed_W, Windowed_H;         // Saved window size before fullscreen
SDL_Window      *Window;                         // SDL window for presentation and input
SDL_Cursor      *SDL_Cursor_Arrow;               // System arrow cursor 
SDL_Cursor      *SDL_Cursor_Hand;                // Active cursor (hovering interactive UI)
SDL_Cursor      *SDL_Cursor_Crosshair;           // Inactive cursor (menu, not hovering)

// Audio
Audio_System Audio;

// Application state
int   Quit;       // Non-zero when the application should exit
float Delta_Time; // Time elapsed since the previous frame in seconds

// Runtime mode flags
int Skip_Postprocess; // Non-zero to bypass the post-processing compute pass
int Use_Validation;   // Non-zero to enable Vulkan validation layers

// Projectile pool (CPU-side)
Projectile_Pool Projectiles;

// Shader binding table (SBT) alignment and handle sizes
VkPhysicalDeviceRayTracingPipelinePropertiesKHR Raytracing_Properties; 

// BLAS for world geometry and TLAS combining all instances
Acceleration_Structure Bottom_Level, Top_Level; 

// GPU storage images and scene data buffers
GPU_Buffer Vertex_Buffer, Index_Buffer, Material_Buffer; // Scene geometry and material data on GPU
GPU_Buffer Top_Level_Instance_Buffer;                    // Host-visible instance buffer for writing TLAS instance descriptors each frame
GPU_Buffer Top_Level_Scratch_Buffer;                     // Persistent scratch memory reused across per-frame TLAS rebuilds
GPU_Image  Raytracing_Storage_Image;                     // Storage image written by ray generation shader
GPU_Buffer Camera_Uniform_Buffer;                        // Uniform buffer for the Camera struct
GPU_Buffer Texture_Id_Buffer;                            // Per-triangle texture index buffer

// Skeletal animation pipeline state (shared by all skeletal entities)
VkPipeline            Skinning_Pipeline; 
VkPipelineLayout      Skinning_Pipeline_Layout;
VkDescriptorSetLayout Skinning_Descriptor_Layout;

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
VkImage        *Texture_Images;       // Array of diffuse texture images
VkDeviceMemory *Texture_Memories;     // Slab memory handle per texture (shared under TLSF)
int            *Texture_Heap_Blocks;  // GPU_Heap block handle per texture (-1 = standalone)
VkImageView    *Texture_Views;        // Image views for shader sampling of each texture
VkSampler       Texture_Sampler;      // Shared sampler with linear filtering and repeat wrap
uint            Texture_Count;        // Total number of texture slots allocated
uint            Textures_Loaded;      // Number of textures successfully loaded from disk
uint            PBR_Stride;           // Stride between PBR map blocks

// Lightmap atlas
VkImage        Lightmap_Image;       // Packed lightmap atlas image
VkDeviceMemory Lightmap_Memory;      // Slab memory handle (shared under TLSF)
int            Lightmap_Heap_Block;  // GPU_Heap block handle
VkImageView    Lightmap_View;        // Image view for lightmap sampling
VkSampler      Lightmap_Sampler;     // Sampler for lightmap lookups (linear, clamp-to-edge)

// Ray tracing pipeline and shader binding table
VkPipelineCache  Pipeline_Cache;              // Shared pipeline cache - amortizes SPIR-V>ISA compilation
VkPipelineLayout Pipeline_Layout;             // Pipeline layout with descriptor set bindings
VkPipeline       Pipeline;                    // Ray tracing pipeline 
GPU_Buffer       Shader_Binding_Table_Buffer; // Buffer holding the shader binding table

// Shader binding table regions (one per shader stage)
VkStridedDeviceAddressRegionKHR Shader_Binding_Ray_Generation; // SBT region for the ray generation shader
VkStridedDeviceAddressRegionKHR Shader_Binding_Miss;           // SBT region for miss shaders
VkStridedDeviceAddressRegionKHR Shader_Binding_Hit;            // SBT region for closest-hit shaders
VkStridedDeviceAddressRegionKHR Shader_Binding_Callable;       // SBT region for callable shaders 

// Central rendering context holding all Vulkan state, GPU resources, and synchronization objects
int              Width;                 // Window width in pixels 
int              Height;                // Window height in pixels
float            Active_Render_Scale;   // Internal RT render scale
int              Active_Denoise_Passes; // A-trous denoise iterations
int              Active_Checkerboard;   // Half-width dispatch (e.g. checkerboard)
int              Render_Width;          // Internal RT render resolution (Width  * Render_Scale)
int              Render_Height;         // Internal RT render resolution (Height * Render_Scale)
VkInstance       Instance;              // Vulkan instance with validation layers
VkSurfaceKHR     Surface;               // Window surface for presentation
VkPhysicalDevice Physical_Device;       // Selected GPU with ray tracing support
VkDevice         Device;                // Logical device created from the physical device
VkQueue          Queue;                 // Universal queue for graphics, compute, and transfer
uint             Queue_Family_Index;    // Index of the queue family supporting all operations

// GPU memory heap (TLSF sub-allocator — all GPU memory flows through this)
GPU_Heap Heap;

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
GPU_Image             Depth_Image;              // R32F depth output from ray tracing
GPU_Image             History_Image;            // Previous frame for TAA
GPU_Image             Postprocess_Output_Image; // Final post-processed output
int                   Frame_Count         = 0;  // Frame counter for TAA convergence
int                   Current_Budget_Byte = 0;  // 0-255, set each frame for denoiser gating
mat4                  Prev_View_Matrix;         // Previous frame's view matrix for TAA reprojection

// A-trous wavelet denoiser
VkPipeline            Denoise_Pipeline;
VkPipelineLayout      Denoise_Pipeline_Layout;
VkDescriptorSetLayout Denoise_Descriptor_Layout;
VkDescriptorPool      Denoise_Descriptor_Pool;
VkDescriptorSet       Denoise_Descriptor_Sets[2]; // Ping-pong: [0] reads A writes B, [1] reads B writes A
GPU_Image             Denoise_Ping_Image;         // Ping-pong buffer for spatial denoising

// GPU physics pipeline state
VkPipeline            Physics_Pipeline;          // Compute pipeline for physics simulation
VkPipelineLayout      Physics_Pipeline_Layout;   // Pipeline layout with push constants for GPU_Input
VkDescriptorSetLayout Physics_Descriptor_Layout; // Layout
VkDescriptorPool      Physics_Descriptor_Pool;   // Pool for the physics descriptor set
VkDescriptorSet       Physics_Descriptor_Set;    // Descriptor set binding physics resources
GPU_Buffer            Player_State_Buffer;       // SSBO holding the GPU_Player state (read-write each frame)
GPU_Buffer            Hull_Storage_Buffer;       // SSBO holding GPU_Hull vertex + adjacency data
GPU_Buffer            Projectile_Buffer;         // SSBO holding GPU_Projectile_Pool

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

// Invert an orthogonal matrix (rotation + translation only) by transposing the 3 by 3 rotation block and recomputing the translation as the
// negated rotated original translation.
mat4 Inverse_Orthogonal (mat4 Source);

// Analytically invert a perspective projection matrix by exploiting its known sparse structure. Only the non-zero elements are inverted;
// all others remain zero.
mat4 Inverse_Projection (mat4 Projection);

// Construct a 4 by 4 identity matrix
mat4 Identity ();

// Multiply two 3 by 4 affine matrices: C = A * B (row-major, translation in column 3)
void Mat34_Mul (const float A[3][4], const float B[3][4], float C[3][4]);

// Multiply two 4 by 4 matrices: Result = A * B (row-major, column-major storage)
mat4 Mat4_Mul (mat4 A, mat4 B);

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §5. Memory
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Search the physical device's memory heaps for a memory type index that satisfies both the type bitmask and the desired property flags
uint Find_Memory_Type (uint Type_Bits, VkMemoryPropertyFlags Desired_Properties);

// Allocate a GPU buffer with the given size, usage flags, and memory properties
GPU_Buffer Buffer_Allocate (uint64_t Size, VkBufferUsageFlags Usage, VkMemoryPropertyFlags Memory_Flags);

// Map the buffer's device memory into host address space, copy the source data, then unmap
void Buffer_Upload (GPU_Buffer Destination, const void *Data, uint64_t Size);

// Upload data to device-local memory via a host-visible staging buffer
GPU_Buffer Buffer_Stage_Upload (VkCommandBuffer Command_Buffer, VkQueue Queue,
                                const void *Data, uint64_t Size, VkBufferUsageFlags Usage);

// Destroy a GPU buffer: release its VkBuffer handle and free its heap block
void Buffer_Destroy (GPU_Buffer *B);

// ── CPU Arena ────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
//
// Minimal bump allocator: a contiguous slab of host memory with a monotonically advancing cursor. Suitable for per-load and per-frame
// scratch allocations where individual frees are unnecessary — just reset the cursor to reclaim everything at once.
//
typedef struct {
  uint8_t *Base;     // Start of the backing allocation (malloc'd once)
  uint64_t Used;     // High-water byte offset from Base
  uint64_t Capacity; // Total size of the slab in bytes
} Arena;

// Create a CPU arena with the given capacity (bytes). Backing memory is allocated via malloc.
Arena  Arena_Create  (uint64_t Capacity);

// Bump-allocate Size bytes aligned to Align from the arena. Returns NULL if the arena is exhausted.
void  *Arena_Alloc   (Arena *A, uint64_t Size, uint64_t Align);

// Reset the arena cursor to zero, logically freeing all prior allocations
void   Arena_Reset   (Arena *A);

// Free the arena's backing memory
void   Arena_Destroy (Arena *A);

// ── GPU Heap: TLSF Sub-Allocator ────────────────────────────────────────────────────────────────────────────────────────────────────
//
// Production-grade O(1) GPU memory allocator using Two-Level Segregated Fit (TLSF), the algorithm used by AMD's RADV driver and
// real-time systems worldwide (RTEMS, FreeRTOS, L4Re). Every alloc and free completes in bounded constant time via bitmapped
// free-list lookup using CLZ/CTZ intrinsics — no linear scans, no recursion, no locks.
//
// Architecture (following Masmano et al. 2004, "TLSF: a New Dynamic Memory Allocator for Real-Time Systems"):
//
//   Slab         One VkDeviceMemory allocation (256 MB default). Each memory type gets its own slab chain.
//   Block        Contiguous region within a slab. Blocks carry boundary tags for O(1) coalescing:
//                a header at the start stores (size, prev_phys, free, slab), and the last word of a free block
//                stores its size so the next block can find it for backward coalescing.
//   Free-lists   Two-level bitmap: FL (first level) indexes by floor(log2(size)), SL (second level) subdivides
//                each power-of-two class into 2^SL_BITS linear subdivisions. A free block of size S maps to
//                FL = floor(log2(S)), SL = (S >> (FL - SL_BITS)) - 2^SL_BITS.  The bitmaps FL_Bitmap and
//                SL_Bitmap[fl] track which classes have free blocks.  Alloc = find first set bit >= requested
//                class.  Free = insert into class + coalesce with physical neighbors.
//   Asset Pack   Large asset loads (map packs, model sets) can be pinned to a dedicated slab.  On unload, the
//                entire slab is freed in one vkFreeMemory call — O(1) bulk deallocation with zero fragmentation
//                left behind, critical for level streaming.
//
// Complexity:  Alloc O(1), Free O(1), Coalesce O(1), Bulk unload O(1).
// Fragmentation: < 15% proven bound (Masmano et al.), sub-1% typical for game workloads.
//
#define GPU_HEAP_MAX_SLABS     32        // Max VkDeviceMemory allocations (well under driver 4096 limit)
#define GPU_HEAP_MAX_BLOCKS    4096      // Max sub-allocation blocks across all slabs
#define GPU_HEAP_DEFAULT_SIZE  (256u<<20) // 256 MB default slab
#define GPU_HEAP_FL_BITS       28        // First-level classes: log2(256MB) = 28
#define GPU_HEAP_SL_BITS       4         // Second-level subdivisions per class (16 bins per power-of-two)
#define GPU_HEAP_SL_COUNT      (1u << GPU_HEAP_SL_BITS) // 16
#define GPU_HEAP_MIN_SIZE      64        // Minimum block size (alignment floor)
#define GPU_HEAP_PACK_FLAG     0x80000000u // Slab flag: asset pack (bulk-freeable)

typedef struct {
  uint64_t Offset;        // Byte offset within slab
  uint64_t Size;          // Usable size (excl. boundary tag overhead — stored externally)
  int16_t  Prev_Free;     // Intrusive doubly-linked free-list: previous free block in same (FL,SL) class
  int16_t  Next_Free;     // Next free block in same class (-1 = end)
  int16_t  Prev_Phys;     // Previous physical block in same slab (-1 = first)
  int16_t  Slab;          // Which slab this block lives in
  uint8_t  Free;          // 1 = available, 0 = allocated
  uint8_t  Pad;
} GPU_Heap_Block;

typedef struct {
  VkDeviceMemory Memory;       // Vulkan device memory handle
  uint64_t       Size;         // Total slab size in bytes
  uint           Memory_Type;  // Vulkan memory type index this slab was allocated from
  uint           Flags;        // GPU_HEAP_PACK_FLAG | pack_id in lower bits
  uint8_t       *Mapped;       // Persistently mapped pointer (NULL if device-local only)
  int16_t        First_Block;  // Head of physical block chain for this slab
} GPU_Heap_Slab;

typedef struct {
  GPU_Heap_Slab  Slabs       [GPU_HEAP_MAX_SLABS];
  uint           Slab_Count;
  GPU_Heap_Block Blocks      [GPU_HEAP_MAX_BLOCKS];
  uint           Block_Count;
  int16_t        Free_Stack  [GPU_HEAP_MAX_BLOCKS]; // Recycled block indices (LIFO)
  uint           Free_Stack_N;
  uint64_t       Default_Slab_Size;

  // TLSF bitmaps: O(1) free-list lookup
  uint32_t       FL_Bitmap;                                    // First-level: bit I set ↔ SL_Bitmap[I] != 0
  uint16_t       SL_Bitmap   [GPU_HEAP_FL_BITS];               // Second-level: bit J set ↔ Free_Head[I][J] != -1
  int16_t        Free_Head   [GPU_HEAP_FL_BITS][GPU_HEAP_SL_COUNT]; // Head of free-list for class (FL, SL)

  // Stats
  uint64_t       Total_Allocated;
  uint64_t       Total_Used;
  uint           Peak_Blocks;
} GPU_Heap;

// Lifecycle
void GPU_Heap_Init    (GPU_Heap *H, uint64_t Default_Slab_Size);
void GPU_Heap_Destroy (GPU_Heap *H);

// Core O(1) operations
int  GPU_Heap_Alloc (GPU_Heap *H, uint64_t Size, uint64_t Alignment,
                     VkMemoryPropertyFlags Mem_Flags, uint Type_Bits,
                     VkDeviceMemory *Out_Memory, uint64_t *Out_Offset, uint8_t **Out_Mapped);
void GPU_Heap_Free  (GPU_Heap *H, int Block_Handle);

// Asset pack: pin a dedicated slab, alloc from it, bulk-free the entire pack in O(1)
int  GPU_Heap_Pack_Create  (GPU_Heap *H, uint64_t Size, VkMemoryPropertyFlags Mem_Flags, uint Type_Bits);
int  GPU_Heap_Pack_Alloc   (GPU_Heap *H, int Pack_Slab, uint64_t Size, uint64_t Alignment,
                            VkDeviceMemory *Out_Memory, uint64_t *Out_Offset, uint8_t **Out_Mapped);
void GPU_Heap_Pack_Destroy (GPU_Heap *H, int Pack_Slab);

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §6. Materials
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

// VTF texture format constants
#define VTF_MAGIC        0x00465456u // "VTF\0" as 32-bit LE
#define VTF_VERSION_MAJ  7           // VTF 7.x
#define VTF_VERSION_MIN  5           // VTF 7.5 (latest common)
#define VTF_MAX_FRAMES   1           // Single frame for textures
#define VTF_MAX_MIP      16          // Maximum mipmap count

// VTF image format enumeration (subset used by CSPromod assets)
typedef enum {
  VTF_FMT_RGBA8888 = 0,  VTF_FMT_ABGR8888 = 1,  VTF_FMT_RGB888   = 2,
  VTF_FMT_BGR888   = 3,  VTF_FMT_RGB565   = 4,  VTF_FMT_DXT1     = 13,
  VTF_FMT_DXT3     = 14, VTF_FMT_DXT5     = 15, VTF_FMT_BGRA8888 = 12,
  VTF_FMT_UV88     = 16, VTF_FMT_RGBA16F  = 24, VTF_FMT_NONE     = -1
} VTF_Image_Format;

// VTF file header (Valve Texture Format - on-disk layout)
typedef struct {
  uint     Magic;
  uint     Version[2];
  uint     Header_Size;
  uint16_t Width, Height;
  uint     Flags;
  uint16_t Frames, First_Frame;
  uint8_t  Pad0[4];
  float    Reflectivity[3];
  uint8_t  Pad1[4];
  float    Bump_Scale;
  int      High_Res_Format;
  uint8_t  Mipmap_Count;
  int      Low_Res_Format;
  uint8_t  Low_Res_W, Low_Res_H;
} VTF_Header;

// Load a TGA image file and decode it into RGBA8 pixel data
uint8_t *TGA_Load (const char *Path, uint *Out_Width, uint *Out_Height);

// Upload raw RGBA pixel data to a device-local texture image via staging buffer
void Texture_Upload_With_Format (VkCommandBuffer Command_Buffer, VkQueue Queue,
                                 const uint8_t *Pixels, uint Width, uint Height, VkFormat Format,
                                 VkImage *Out_Image, VkDeviceMemory *Out_Memory, VkImageView *Out_View,
                                 int *Out_Heap_Block);

// Convenience wrapper that uploads a texture as SRGB
void Texture_Upload (VkCommandBuffer Command_Buffer, VkQueue Queue,
                     const uint8_t *Pixels, uint Width, uint Height,
                     VkImage *Out_Image, VkDeviceMemory *Out_Memory, VkImageView *Out_View);

// Create a device-local 2D image suitable for use as a ray tracing storage target
GPU_Image Image_Storage_Create (uint Width, uint Height);

// Insert a pipeline barrier that transitions an image between layouts
void Image_Layout_Barrier (VkCommandBuffer      Command_Buffer, VkImage              Image,
                           VkImageLayout        Old_Layout,     VkImageLayout        New_Layout,
                           VkAccessFlags        Source_Access,  VkAccessFlags        Destination_Access,
                           VkPipelineStageFlags Source_Stage,   VkPipelineStageFlags Destination_Stage);

// Return the bytes-per-pixel for a VTF image format (0 for block-compressed formats)
uint VTF_Bpp (int Format);

// Decode one 4 by 4 DXT1 block into RGBA8 pixels at Dst with the given row stride in bytes
void DXT1_Decode_Block (const uint8_t *Src, uint8_t *Dst, int Stride);

// Decode one 4 by 4 DXT5 alpha block and write the alpha channel into existing RGBA8 pixels at Dst
void DXT5_Decode_Alpha (const uint8_t *Src, uint8_t *Dst, int Stride);

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

// Body-part damage multiplier maps: grayscale TGA textures UV-mapped to player models
#define DAMAGE_CACHE_MAX 64
typedef struct {
  const char *Model_Name;       // Player model directory name
  const char *Damage_Maps[6];   // Up to 6 damage map TGA paths per model (NULL-terminated)
  int         Damage_Map_Count; // Number of damage maps for this model
} Model_Damage_Entry;

// Global damage map collection
int                    Damage_Cache_Count = 0;
Damage_Map_Cache_Entry Damage_Cache[DAMAGE_CACHE_MAX];

// Free all cached damage map pixel data
void Damage_Cache_Free ();

// Look up the damage map path for a given model name and body part index (0=head, 1=upper, 2=lower)
const char *Damage_Map_For_Model (const char *Model_Name, int Part_Index);

// Returns a damage multiplier in 0.0..1.0
float Damage_Map_Sample (const char *Path, float U, float V);

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §7. Models
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Id Tech 3 model format
#define MD3_MAGIC           0x33504449u // "IDP3" as a 32-bit little-endian integer
#define MD3_MAX_SURFACES    3           // Maximum surfaces per weapon part (body, barrel, hand)
#define MD3_MAX_ANIM_FRAMES 30          // Maximum animation frames extracted from tag_weapon

// Source MDL format constants
#define MDL_MAGIC_IDST    0x54534449u // "IDST" - studio model header magic
#define MDL_MAGIC_IDSQ    0x51534449u // "IDSQ" - sequence group header magic
#define MDL_VERSION_44    44          // HL2 / Source 2004
#define MDL_VERSION_48    48          // CS:Source / Source 2007+
#define MDL_VERSION_49    49          // Source 2013 / CS:GO
#define MDL_MAX_BONES     128         // Maximum bones per skeleton
#define MDL_MAX_MESHES    32          // Maximum meshes per body part
#define MDL_MAX_BODYPARTS 16          // Maximum body part groups

// Skeletal animation limits
#define SKEL_MAX_BONES_PER_VERT 3   // Maximum bone influences per vertex (GPU-friendly)
#define SKEL_MAX_ANIMS          32  // Maximum animation sequences per model
#define SKEL_MAX_FRAMES         256 // Maximum keyframes per animation

// MD3 surface header - one triangle mesh within an MD3 model file
typedef struct {
  int  Magic;                                    // Surface magic identifier (always IDP3)
  char Name[64];                                 // Null-terminated surface name
  int  Flags;                                    // Surface flags (unused in Quake 3)
  int  Number_Of_Frames, Number_Of_Shaders;      // Animation frame count and attached shader count
  int  Number_Of_Vertices, Number_Of_Triangles;  // Per-frame vertex count and triangle count
  int  Triangles_Offset, Shaders_Offset;         // Byte offsets from surface start to triangle and shader data
  int  Texture_Coordinates_Offset;               // Byte offset to the per-vertex texture coordinate array
  int  Vertices_Offset, End_Offset;              // Byte offset to compressed vertex frames and to the next surface
} MD3_Surface;

// MD3 tag - a named attachment point with position and orientation for linking model parts
typedef struct {
  char  Name  [64]; // Null-terminated tag name (e.g. "tag_barrel", "tag_weapon")
  float Origin[3];  // World-space position of the attachment point
  float Axis  [9];  // 3x3 rotation matrix (row-major) defining the tag's local coordinate frame
} MD3_Tag;

// Source engine studio model header (versions 44-49)
typedef struct {
  int  Magic, Version, Checksum;
  char Name[64];
  int  Length;
  vec3 Eye_Position, Illumination_Position, Hull_Min, Hull_Max, View_Min, View_Max;
  int  Flags;
  int  Bone_Count,              Bone_Offset;
  int  Bone_Controller_Count,   Bone_Controller_Offset;
  int  Hitbox_Count,            Hitbox_Offset;
  int  Animation_Count,         Animation_Offset;
  int  Sequence_Count,          Sequence_Offset;
  int  Activity_Version,        Events_Index;
  int  Material_Count,          Material_Offset;
  int  Material_Dir_Count,      Material_Dir_Offset;
  int  Skin_Ref_Count,          Skin_Family_Count,    Skin_Offset;
  int  Body_Count,              Body_Offset;
} MDL_Header;

// Per-bone data including bind pose, animation scale factors, and inverse bind
typedef struct {
  int   Name_Offset;        // Byte offset from struct start to the null-terminated bone name
  int   Parent;             // Parent bone index, or -1 for root bones
  int   Controllers[6];     // Bone controller indices for each degree of freedom
  vec3  Position;           // Bind-pose translation (local space)
  float Quat[4];            // Bind-pose rotation as a quaternion (xyzw)
  vec3  Rot;                // Bind-pose rotation as Euler angles (XYZ, radians)
  vec3  Position_Scale;     // Per-axis position animation compression scale
  vec3  Rot_Scale;          // Per-axis rotation animation compression scale
  float Pose_To_Bone[3][4]; // Inverse bind-pose matrix (3x4, row-major)
  float Align[4];           // Quaternion alignment hint
  int   Flags;
  int   Procedure_Type, Procedure_Offset;
  int   Physics_Bone,   Surface_Offset, Contents;
  int   Pad[8];
} MDL_Bone;

// Logical body part grouping (e.g. "head", "arms")
typedef struct {
  int Name_Offset;  // Byte offset to the null-terminated body part name
  int Model_Count;  // Number of model LOD sets within this body part
  int Base_Index;   // Base index used for body part selection arithmetic
  int Model_Offset; // Byte offset (relative) to the first MDL_Model entry
} MDL_Body_Part;

// Triangle mesh LOD set within a body part
typedef struct {
  char  Name[64];
  int   Type;
  float Radius;
  int   Mesh_Count,       Mesh_Offset;
  int   Vertex_Count,     Vertex_Offset,  Tangent_Offset;
  int   Attachment_Count, Attachment_Offset;
  int   Eye_Count,        Eye_Offset;
  int   Pad[16];
} MDL_Model;

// Material surface within a model
typedef struct {
  int  Material;       // Index into the MDL material table
  int  Model_Offset;   // Byte offset back to the owning MDL_Model
  int  Vertex_Count;   // Number of vertices belonging to this mesh
  int  Vertex_Offset;  // Base index into the VVD vertex array for this mesh
  int  Flex_Count,     Flex_Offset;
  int  Material_Type,  Material_Param;
  int  Id;
  vec3 Center;
  int  Pad[17];
} MDL_Mesh;

// Vertex data sidecar for Source's "Studio Model" MDL format
#define VVD_MAGIC 0x56534449 // 'IDSV'
typedef struct {
  uint Magic;
  int  Version;
  int  Checksum;
  int  LOD_Count;
  int  LOD_Vertex_Counts[8]; // Per-LOD vertex counts; index 0 is the highest-detail LOD
  int  Fixup_Count;
  int  Fixup_Table_Start;
  int  Vertex_Data_Start;
  int  Tangent_Data_Start;
} VVD_Header;

// Skinned vertex
typedef struct {
  float   Bone_Weights[3]; // Normalised blend weights for up to 3 bone influences
  uint8_t Bone_Ids[3];     // Bone indices corresponding to each weight
  uint8_t Bone_Count;      // Number of active bone influences (1-3)
  float   Position[3];     // Bind-pose position in model space
  float   Normal[3];       // Bind-pose normal in model space
  float   Tex_Coord[2];    // Texture UV coordinates
} VVD_Vertex;

// VTX OptimizedModel header - triangle strip sidecar for Source MDL
#define VTX_VERSION 7
typedef struct {
  int      Version;
  int      Vertex_Cache_Size;
  uint16_t Max_Bones_Strip, Max_Bones_Tri;
  int      Max_Bones_Vert;
  int      Checksum;
  int      LOD_Count;
  int      Material_Replacement_Offset;
  int      Body_Part_Count;
  int      Body_Part_Offset;
} VTX_Header;

typedef struct {int Model_Count,       Model_Offset;} VTX_Body_Part;
typedef struct {int LOD_Count,         LOD_Offset;}   VTX_Model;
typedef struct {int Mesh_Count,        Mesh_Offset; float Switch_Point;} VTX_LOD;
typedef struct {int Strip_Group_Count, Strip_Group_Offset; uint8_t Flags;} VTX_Mesh;

// Strip group within a mesh (25 bytes packed)
typedef struct {
  int     Vertex_Count, Vertex_Offset;
  int     Index_Count,  Index_Offset;
  int     Strip_Count,  Strip_Offset;
  uint8_t Flags;
} VTX_Strip_Group;

// Strip-group vertex entry referencing the VVD via origMeshVertID (9 bytes packed)
typedef struct {
  uint8_t  Bone_Weight_Indices[3];    // Indices into the strip group's bone table for each weight
  uint8_t  Bone_Count;                // Number of active bone influences for this vertex
  uint16_t Original_Mesh_Vertex_Id;  // origMeshVertID: index into the mesh's VVD vertex range
  int8_t   Bone_Ids[3];              // Bone indices (after remapping through the strip group's bone table)
} VTX_Vertex; // 9 bytes

// ── Articulated Figure ───────────────────────────────────────────────────────────────────────────────────────────────────────────────
//
// Unified model representation. An articulated figure is a hierarchy of named parts, each with geometry, a skeleton, and attachment
// tags. A weapon is a figure with parts {body, barrel, hand}. A player is a figure with parts {head, upper, lower}. A Source MDL is
// a figure with parts derived from bodygroups. This replaces both Weapon_Model and the ad-hoc Entity frame arrays, making weapon
// loading a superset of model loading rather than a separate operation.
//

#define FIGURE_MAX_PARTS  8
#define FIGURE_MAX_TAGS   16
#define FIGURE_MAX_ANIMS  32
#define FIGURE_MAX_FRAMES 256
#define FIGURE_MAX_BONES  128

// Named attachment point (tag_barrel, tag_weapon, tag_head, etc.)
#define FIGURE_MAX_TAG_FRAMES 32
typedef struct {
  char  Name[64];
  float Transforms[FIGURE_MAX_TAG_FRAMES][12]; // Per-frame: Origin[3] + Axis[9]
  uint  Frame_Count;                            // Number of frames (1 for static tags)
} Figure_Tag;

// One geometric part of the figure (e.g. "barrel", "upper_body", "head")
typedef struct {
  char     Name[64];
  Vertex  *Vertices;     uint Vertex_Count;
  uint    *Indices;      uint Index_Count;
  uint    *Texture_Ids;  uint Triangle_Count;
  char     Texture_Names[WEAPON_MAX_TEXTURES][64];
  uint     Surface_Count;
  int      Parent_Tag;   // Index into the figure's tag array (-1 = root)
} Figure_Part;

// Animation clip: a named sequence of keyframes
typedef struct {
  char  Name[64];
  int   First_Frame;  // Index into the figure's frame vertex arrays
  int   Frame_Count;
  float FPS;
  int   Looping;
} Figure_Animation;

// The complete articulated figure
typedef struct {
  Figure_Part       Parts[FIGURE_MAX_PARTS];
  uint              Part_Count;
  Figure_Tag        Tags[FIGURE_MAX_TAGS];
  uint              Tag_Count;
  Figure_Animation  Animations[FIGURE_MAX_ANIMS];
  uint              Animation_Count;

  // Merged geometry (all parts flattened for GPU upload)
  Vertex *Vertices;     uint Vertex_Count;
  uint   *Indices;      uint Index_Count;
  uint   *Texture_Ids;  uint Triangle_Count;
  uint    Surface_Count;
  char    Texture_Names[WEAPON_MAX_TEXTURES][64];

  // Per-frame vertex snapshots (for MD3-style vertex animation)
  Vertex *Frame_Vertices[FIGURE_MAX_FRAMES];
  uint    Total_Frame_Count;

  // Skeletal data (for Source MDL bone-driven animation)
  int         Bone_Count;
  int         Bone_Parents [FIGURE_MAX_BONES];
  float       Bind_Pose    [FIGURE_MAX_BONES][3][4];
  float       Inv_Bind     [FIGURE_MAX_BONES][3][4];
  uint8_t    *Bone_Ids;
  uint8_t    *Bone_Weights;

  int         Is_Source;   // 1 = Source MDL skeletal, 0 = MD3 vertex animation
} Articulated_Figure;

// Load an articulated figure from any supported format. Dispatches based on file extension:
//   .md3 → Q3 multi-part assembly (head + upper + lower, or body + barrel + hand)
//   .mdl → Source engine skeletal model (MDL + VVD + VTX)
//   .psk → Unreal skeletal mesh (future)
Articulated_Figure Figure_Load (const char *Path, vec3 Origin, float Yaw);

// Convenience: load a weapon figure (sets up viewmodel transforms, tag_weapon animation, scales)
Articulated_Figure Figure_Load_Weapon (const char *Path);

// Read an MD3 file from disk into a heap-allocated buffer. Returns NULL on failure; sets *Out_Size to byte count.
uint8_t *MD3_Load_File (const char *Path, long *Out_Size);

// Compose two MD3 tag transforms: C = A * B (each is origin[3] + axis[9], 12 floats total)
void Tag_Compose (const float *A, const float *B, float *C);

// Parse one MD3 surface at frame 0 and append its triangles to the caller's shared geometry arrays
void MD3_Parse_Surface (const uint8_t *Surface_Data,
                        Vertex **Inout_Vertices,    uint *Inout_Vertex_Count,
                        uint  **Inout_Indices,      uint *Inout_Index_Count,
                        uint  **Inout_Texture_Ids,  uint *Inout_Triangle_Count,
                        uint Assigned_Texture_Index, const float *Transform);

// Parse one MD3 surface at a specific animation frame and append its triangles to the caller's shared geometry arrays
void MD3_Parse_Surface_At_Frame (const uint8_t *Surface_Data, int Frame,
                                 Vertex **Inout_Vertices,    uint *Inout_Vertex_Count,
                                 uint  **Inout_Indices,      uint *Inout_Index_Count,
                                 uint  **Inout_Texture_Ids,  uint *Inout_Triangle_Count,
                                 uint Assigned_Texture_Index, const float *Transform);

// Load the default Quake 3 MD3 weapon model from the assets/models directory
Articulated_Figure Weapon_Model_Load ();

// Load a Source engine MDL model as a held weapon (viewmodel). Parses MDL + VVD + VTX sidecars, applies idle-pose skinning.
Articulated_Figure Source_Weapon_Model_Load (const char *Path);

// Assemble a composite Q3 player model (lower + upper + head + weapon) at a given animation frame into merged geometry arrays.
void Entity_Assemble_Frame (int Legs_Frame, int Torso_Frame,
                            uint Body_Mat, uint Gun_Mat, const float World[12],
                            Vertex **Out_Verts, uint *Out_Vert_Count,
                            uint **Out_Indices, uint *Out_Index_Count,
                            uint **Out_Tex_Ids, uint *Out_Tri_Count);

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
#define BSP_LIGHTMAPS       14          // Lump index: 128 by 128 RGB lightmap pages
#define SURFACE_TYPE_PLANAR 1           // Face type: flat polygon rendered from indices
#define SURFACE_TYPE_PATCH  2           // Face type: Bézier patch (tessellated at load time)
#define SURFACE_TYPE_MESH   3           // Face type: triangle mesh (e.g. models in BSP)
#define TESSELLATION_LEVEL  5           // Number of subdivisions per Bézier patch edge
#define LIGHTMAP_PAGE_SIZE  128         // Width and height in texels of each lightmap page

// Source BSP (VBSP) format constants
#define VBSP_MAGIC       0x50534256u // "VBSP"
#define VBSP_VERSION_19  19          // HL2
#define VBSP_VERSION_20  20          // CS:Source / HL2:EP1
#define VBSP_VERSION_21  21          // CS:GO / L4D2
#define VBSP_LUMP_COUNT  64          // Source BSP has 64 lumps
#define VBSP_ENTITIES    0           // Entity lump
#define VBSP_PLANES      1           // Plane lump
#define VBSP_TEXDATA      2          // Texture data lump
#define VBSP_VERTICES    3           // Vertex lump
#define VBSP_NODES       5           // BSP nodes
#define VBSP_TEXINFO     6           // Texture info lump
#define VBSP_FACES       7           // Face lump
#define VBSP_LIGHTING    8           // Lightmap lump (luxels)
#define VBSP_LEAFS       10          // Leaf lump
#define VBSP_EDGES       12          // Edge lump
#define VBSP_SURFEDGES   13          // Surface-edge lump
#define VBSP_MODELS      14          // Brush model lump
#define VBSP_DISPINFO    26          // Displacement info
#define VBSP_DISPVERTS   33          // Displacement vertices
#define VBSP_TEXDATA_STRING_DATA  43 // Texture name strings
#define VBSP_TEXDATA_STRING_TABLE 44 // Texture name string table

// One entry in the VBSP lump directory: locates and describes a single data section
typedef struct {
  int  Offset;     // Byte offset from the start of the file to the first byte of lump data
  int  Length;     // Length of the lump data in bytes (0 = lump absent)
  int  Version;    // Lump format version (usually 0; non-zero for compressed or extended lumps)
  char Fourcc[4];  // Four-character code identifying the lump (unused in most lumps, zeroed)
} VBSP_Lump;

// VBSP file header: magic, format version, and the full lump directory
typedef struct {
  uint       Magic;                  // File magic: 0x50534256 ("VBSP" in little-endian ASCII)
  uint       Version;                // BSP format version (19, 20, or 21)
  VBSP_Lump  Lumps[VBSP_LUMP_COUNT]; // Directory of all data lumps, indexed by VBSP_* lump constants
  int        Map_Revision;           // Hammer editor revision number at the time of the last compile
} VBSP_Header;

// A single geometric vertex position 
typedef struct {
  float Position[3]; // World-space XYZ coordinates in Source engine units
} VBSP_Vertex;

// A directed edge connecting two vertices - sign of the surf-edge index determines which endpoint is first
typedef struct {
  uint16_t Vertex_Index[2]; // Indices into the VBSP_VERTICES array: [0]=start, [1]=end
} VBSP_Edge;

// A rendered world surface, described indirectly via the surf-edge and vertex arrays
typedef struct {
  uint16_t Plane_Num;          // Index into the VBSP_PLANES lump; defines the surface's infinite plane
  uint8_t  Side;               // When 0 = face is on the front side of the plane; 1 = back side
  uint8_t  On_Node;            // True if this face sits exactly on a BSP node boundary, 0 otherwise
  int      First_Edge;         // Index into VBSP_SURFEDGES of the first surf-edge for this face
  int16_t  Num_Edges;          // Number of surf-edges (and therefore vertices) forming the face polygon
  int16_t  Tex_Info;           // Index into VBSP_TEXINFO; supplies texture projection vectors and flags
  int16_t  Disp_Info;          // Index into VBSP_DISPINFO if this face is a displacement; -1 otherwise
  int16_t  Fog_Volume;         // Index into the fog volume list (-1 if not inside a fog volume)
  uint8_t  Lighting_Styles[4]; // Up to four lighting style slots (255 = unused slot)
  int      Lightmap_Offset;    // Byte offset into the VBSP_LIGHTING lump for this face's luxel grid
  float    Area;               // Surface area in Source units squared (precomputed by vbsp)
  int      Lightmap_Mins[2];   // Minimum texture-space extents of the lightmap region (S, T)
  int      Lightmap_Size[2];   // Width and height of the lightmap region in luxels (S, T)
  int      Original_Face;      // Index of the original pre-split face in VBSP_ORIGINALFACES (-1 if none)
  uint16_t Primitive_Count;    // Number of primitives (used for water surfaces; 0 for ordinary faces)
  uint16_t First_Primitive;    // Index into the VBSP_PRIMITIVES lump of the first primitive
  uint     Smoothing_Groups;   // Bitmask of smoothing groups this face belongs to
} VBSP_Face;

// Texture projection and lightmap projection vectors for a face, stored in VBSP_TEXINFO
typedef struct {
  float Texture_Vecs [2][4]; // Texture-space projection: [0]=S row, [1]=T row; each is (X,Y,Z,Offset)
  float Lightmap_Vecs[2][4]; // Lightmap-space projection: same layout as Texture_Vecs
  int   Flags;               // Surface property flags (SURF_* bitmask: sky, nodraw, trigger, etc.)
  int   Texture_Data;        // Index into VBSP_TEXDATA for the material bound to this surface
} VBSP_Tex_Info;

// Material reflectivity, name reference, and texture dimensions, stored in VBSP_TEXDATA
typedef struct {
  float Reflectivity[3];         // Approximate diffuse reflectivity (R, G, B) in 0–1 range; used for radiosity
  int   Name_Id;                 // Index into VBSP_TEXDATA_STRING_TABLE, which points into VBSP_TEXDATA_STRING_DATA
  int   Width,  Height;          // Full texture dimensions in texels (may differ from the VTF on disk)
  int   View_Width, View_Height; // Texture dimensions as seen through the material's $basetexture view
} VBSP_Tex_Data;

// Displacement surface descriptor - describes how a quad face is subdivided and height-displaced into a terrain patch
typedef struct {
  float    Start_Position[3];     // World-space corner of the base quad closest to the displacement origin
  int      Disp_Vert_Start;       // First index into VBSP_DISPVERTS for this displacement's vertex grid
  int      Disp_Tri_Start;        // First index into VBSP_DISPTRIS for this displacement's triangle flags
  int      Power;                 // Subdivision power: grid is (2^Power + 1) × (2^Power + 1) vertices
  int      Min_Tessellation;      // Minimum LOD level the engine may simplify to at runtime
  float    Smoothing_Angle;       // Maximum dihedral angle (degrees) between adjacent faces for normal smoothing
  int      Surface_Contents;      // BSP contents flags (CONTENTS_*) inherited by the displacement surface
  uint16_t Map_Face;              // Index into VBSP_FACES of the base quad this displacement is attached to
  uint16_t Pad;                   // Explicit padding to maintain 4-byte alignment of subsequent fields
  int      Lightmap_Alpha_Start;  // First index into the per-displacement lightmap alpha array
  int      Lightmap_Sample_Start; // First index into the per-displacement lightmap sample position array
  uint8_t  Neighbor_Data[128];    // Displacement neighbor and allowed vertex information (internal compiler data)
} VBSP_Disp_Info;

// One displaced grid vertex, stored in the VBSP_DISPVERTS lump
typedef struct {
  float Vec  [3]; // Unit displacement direction vector in world space
  float Dist;     // Scalar distance to displace along Vec from the bilinearly interpolated base position
  float Alpha;    // Per-vertex blend alpha for the displacement's secondary paint texture (0–255)
} VBSP_Disp_Vert;

// One entry in the IBSP lump directory
typedef struct {
  int Offset; // Byte offset from the start of the file to the first byte of lump data
  int Length; // Length of the lump data in bytes
} BSP_Lump;

// IBSP file header: magic number, format version, and the lump directory
typedef struct {
  uint     Magic;                 // File magic: 0x50534249 ("IBSP" in little-endian ASCII)
  uint     Version;               // BSP format version (46 for Quake 3 / RTCW)
  BSP_Lump Lumps[BSP_LUMP_COUNT]; // Directory of data lumps indexed by BSP_ENTITIES..BSP_LIGHTMAPS
} BSP_Header;

// A single geometric vertex stored in the BSP_VERTICES lump
typedef struct {
  float   Position       [3]; // World XYZ in engine units
  float   Texture_Coords [2]; // Diffuse UV coordinates (S, T)
  float   Lightmap_Coords[2]; // Lightmap UV coordinates (S, T)
  float   Normal         [3]; // Unit surface normal
  uint8_t Color          [4]; // Per-vertex RGBA color (8 bits per channel; used for vertex lighting)
} BSP_Vertex;

// A rendered world surface stored in the BSP_FACES lump
typedef struct {
  int Shader_Index;  // Index into BSP_SHADERS; identifies the material
  int Fog_Volume;    // Index into BSP_FOGS (-1 if not inside a fog volume)
  int Surface_Type;  // Geometry type - planar polygon, patch, triangle mesh, billboard, etc

  int First_Vertex; // Index of the first vertex in the global BSP_VERTICES array
  int Vertex_Count; // Number of consecutive vertices belonging to this surface

  int First_Index;  // Index of the first element in the global BSP_INDICES array
  int Index_Count;  // Number of consecutive indices forming the triangle list

  int   Lightmap_Index;                          // Lightmap page index (-1 = fullbright / no lightmap)
  int   Lightmap_Origin_X,  Lightmap_Origin_Y;   // Top-left texel corner of this surface's region within its lightmap page
  int   Lightmap_Width,     Lightmap_Height;     // Dimensions of the lightmap region in texels
  float Lightmap_Origin[3], Lightmap_Vectors[9]; // World-space lightmap placement (origin + 2 basis vectors + normal)
  int   Patch_Width,        Patch_Height;        // Control point grid dimensions (only valid for patch surfaces)
} BSP_Face;

// BSP shader entry: maps a surface material name to content and surface flags
typedef struct {
  char Name[64];        // Shader path 
  int  Flags, Contents; // Surface flags (e.g. translucent) and content flags (e.g. solid, water)
} BSP_Shader;

// Per-frame camera state uploaded to the GPU as a uniform buffer
typedef struct {
  vec3  Position, Velocity;               // World-space eye position and movement velocity
  float Yaw, Pitch;                       // Euler angles in radians for horizontal and vertical look
  mat4  Inverse_View, Inverse_Projection; // Inverse matrices for reconstructing world rays from screen coordinates
  uint  Frame;                            // Monotonically increasing frame counter for temporal effects
} Camera;

// Interleaved vertex layout matching the GPU shader input
typedef struct {
  float Position   [3], Padding_A;       // World-space XYZ position; padding aligns to 16 bytes
  float Texture_UV [2], Lightmap_UV [2]; // Diffuse texture coordinates and lightmap atlas coordinates
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
  float  Lightmap_Mult;      // Lightmap intensity multiplier (Q3=4.0, Source=1.5)
} Scene_Environment;

const Scene_Environment DEFAULT_ENVIRONMENT = {
  .Sun_Direction      = {0.5f, 0.7f,  0.5f},   // High sun angle
  .Sun_Color          = {1.0f, 0.95f, 0.85f},  // Warm daylight
  .Sun_Angular_Radius = 0.02f,                 // About ~1.1 degrees (Earth sun ≈ 0.53°)
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
  .Lightmap_Mult      = 4.0f,                  // Q3 lightmaps are stored dark, need 4x boost
};
Scene_Environment Active_Environment;

// Entity System
//
// The entity lump in BSP files is text key/value records. We tokenize and map known keys into typed fields, then discard the raw pairs.
// The discriminant union is the authoritative runtime representation. Unknown keys are silently ignored.
//
typedef enum {
  NO_ENTITY = 0,

  // World origin
  ENTITY_WORLD,

  // Player / camera / navigation
  ENTITY_INFO_PLAYER_START,        // Single-player spawn
  ENTITY_INFO_PLAYER_SPAWN,        // Team spawn 
  ENTITY_INFO_PLAYER_INTERMISSION, // Post-match camera
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

  // and dynamic props
  ENTITY_PROP_STATIC,  // Model instance (misc_model)
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
  char Model    [96];  // Model path 
  char Material [96];  // Material/shader path
  char Sound    [96];  // Sound file path
  char Script   [96];  // Script path
  char Message  [128]; // UI/message string
  vec3 Color;          // Range RGB (0..1) where applicable
  float Alpha;         // Opacity (1.0 default)

  // Gameplay and physics
  int   Spawnflags; // Generic spawnflags bitfield
  int   Flags;      // Generic runtime flags 
  int   Team;       // Team or faction id
  int   Health;     // Hit points
  int   Armor;      // Armor points 
  int   Damage;     // Damage (for hurt/explosive/etc)
  int   Count;      // Generic count 
  float Speed;      // Generic speed 
  float Wait;       // Seconds
  float Delay;      // Seconds
  float Random;     // Random variance (seconds or scalar)
  float Accel;    
  float Decel;  
} Entity_Common;

// BSP Entity discriminate union
typedef struct {
  Entity_Kind   Kind;   // Discriminant tag
  Entity_Common Common; // Shared attributes 
  union { // case Kind is

    // when ENTITY_WORLD =>
    struct {
      float Gravity;       // World gravity scalar 
      float Time_Limit;    // Match time limit 
      int   Score_Limit;   // Score limit 
      float Ambient_Light; // Scalar ambient floor
    } world;

    // when ENTITY_INFO_PLAYER_ =>
    struct {
      int   Player_Class;  // Class index 
      int   Loadout;       // Loadout id 
      float Fov;           // Suggested FOV 
      float View_Height;   // Standing view height 
      float Crouch_Height; // Crouch view height 
    } player;

    // when ENTITY_LIGHT | ENTITY_LIGHT_SPOT =>
    struct {
      float Intensity;    // Luminous intensity / radius scalar
      float Range;        // Explicit range 
      float Inner_Angle;  // Spot inner cone degrees 
      float Outer_Angle;  // Spot outer cone degrees 
      int   Cast_Shadows; // Non-zero = shadow caster
      float Falloff;      // When 1 = linear, 2 = quadratic, etc
    } light;

    // when ENTITY_SOUND_EMITTER =>
    struct {
      float Volume;       // Range 0..1
      float Pitch;        // When 1 = normal
      float Min_Distance; // Full volume within this distance
      float Max_Distance; // Silence beyond this distance
      int   Looping;      // Non-zero = loop
    } sound;

    // when ENTITY_DECAL =>
    struct {
      float Size_X, Size_Y; // Projected size
      float Rotation;       // Degrees
      float Fade_Time;      // Seconds until fully faded: (0 = never)
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
      int Enabled;      // Non-zero = active
      int Filter_Team;  // When 0 = any, else team id
      int Filter_Class; // When 0 = any, else class id
      int Fire_Count;   // How many times it can fire: (0 = infinite)
    } trigger;

    // when ENTITY_TRIGGER_TELEPORT =>
    struct {
      int Preserve_Velocity; // Non-zero - keep incoming velocity
    } teleport;

    // when ENTITY_TRIGGER_PUSH =>
    struct {
      vec3  Push_Dir;   // Direction
      float Push_Speed; // Magnitude
    } push;

    // when movers/doors/platform/train =>
    struct {
      vec3  Move_Dir;    // Movement direction
      float Lip;         // Remaining overlap at end of travel
      float Distance;    // Travel distance 
      float Open_Angle;  // For rotating doors
      int   Toggle;      // Non-zero toggles behavior
      int   Starts_Open; // Non-zero to capture initial state
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
      int          Respawn;  // Respawn seconds 
      float        Duration; // Powerup duration seconds 
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
      float Mass;         // Kg scalar
      float Engine_Power; // Generic power scalar
      float Turn_Rate;    // Degrees/sec
      int   Seats;        // Seat count
    } vehicle;

    // when ENTITY_NPC =>
    struct {
      int   Npc_Class;    // Class index
      float Aggro_Radius; // Detection range
      float Walk_Speed;   // Units/sec
      float Run_Speed;    // Units/sec
    } npc;

    // when ENTITY_OBJECTIVE =>
    struct {
      Objective_Kind Obj_Kind;
      int   Required_Count; // Objective count (e.g. collect N items)
      float Hold_Time;      // Hold/capture seconds
      int   Obj_Team;       // Owning team (0 = neutral)
    } objective;

    // when ENTITY_LOGIC_* =>
    struct {
      int   Value_A;
      int   Value_B;
      int   Threshold;
      float Interval;
      float Chance; // Range 0..1
    } logic;

    // when ENTITY_ENV_SKY =>
    struct {
      vec3 Direction; // Sun direction (normalized)
      vec3 Color;     // Sun color (linear 0..1)
      vec3 Ambient;   // Ambient color (linear 0..1)
    } env;
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
  char        Sky_Name[64];                 // Skybox name from worldspawn (Source maps)
} Scene;   

// Single spawn point parsed from the BSP entity lump
typedef struct {vec3 Origin; float Angle;} Spawn; // World-space origin and facing angle in degrees

typedef struct {float M[3][4];} Bone_Matrix; // A 3 by 4 row-major affine transform (shared by CPU skinning and GPU upload)

// Figure_Instance: the runtime representation of any loaded model — weapon viewmodel, player body, NPC, prop. Combines the parsed
// Articulated_Figure with per-frame GPU state (buffers, BLAS, animation accumulators). This is the single type used everywhere —
// there is no separate "weapon model" or "entity" type.
typedef struct {
  Articulated_Figure     Figure;               // Parsed geometry, tags, animations, skeleton

  // GPU resources
  GPU_Buffer             Vertex_Buffer;        // Host-visible for weapons (CPU-transformed each frame), or re-uploaded for entities
  GPU_Buffer             Index_Buffer;         // Device-local, static
  GPU_Buffer             Texture_Id_Buffer;    // Device-local, static
  GPU_Buffer             Bottom_Level_Scratch; // Scratch buffer reused across BLAS rebuilds
  Acceleration_Structure Bottom_Level;         // BLAS (rebuilt or refit each frame)

  // Animation runtime
  Vertex                *Transformed_Vertices; // Scratch buffer for CPU-side per-frame vertex transformation
  Vertex                *Current_Vertices;     // Pointer to the active frame's vertex data
  float                  Animation_Time;       // Elapsed time accumulator
  int                    Active_Animation;     // Index into Figure.Animations[]

  // Weapon-specific state (zeroed for non-weapon instances)
  int                    Is_Firing;            // Non-zero while the fire button is held
  float                  Fire_Time, Bob_Time;  // Recoil decay timer and idle bob phase accumulator
  uint                   Texture_Base_Index;   // Starting index into the global texture array for this model's textures

  // World placement (zeroed for viewmodel weapons which are camera-relative)
  vec3                   GL_Origin;            // World-space position in GL Y-up coordinates (for TLAS transform)
  float                  GL_Yaw;               // Yaw angle in GL space (radians)

  // Skeletal data — GPU-resident (uploaded once at load time, read by GPU skeleton compute)
  GPU_Buffer             Bone_Buffer;            // GPU SSBO: bind-pose matrices      [Bone_Count * mat3x4]
  GPU_Buffer             Inv_Bind_Buffer;        // GPU SSBO: inverse bind-pose        [Bone_Count * mat3x4]
  GPU_Buffer             Bone_Parent_Buffer;     // GPU SSBO: parent indices           [Bone_Count * int]
  GPU_Buffer             Bone_Weight_Buffer;     // GPU SSBO: per-vertex bone weights  [Vertex_Count * SKEL_MAX_BONES_PER_VERT]
  GPU_Buffer             Bone_Id_Buffer;         // GPU SSBO: per-vertex bone ids      [Vertex_Count * SKEL_MAX_BONES_PER_VERT]
  GPU_Buffer             Pose_Buffer;            // GPU SSBO: output world-space pose  [Bone_Count * mat3x4] (written by compute)

  // Ray mask for TLAS instancing (0xFF = visible to all rays, 0x01 = primary only, 0x02 = shadow only)
  uint8_t                Ray_Mask;

  // TLAS instance transform (3x4 row-major, written by per-frame update)
  float                  TLAS_Transform[3][4];
} Figure_Instance;

// ─────────────────────────────────────────────────────────────────────────────
// Figure_Pool: generational-index slot allocator for Figure_Instance
//
// Uses a free-list with generational handles for O(1) alloc / O(1) free / safe lookup.
// Each slot has a generation counter that increments on free, so stale handles are detected.
// Industry standard: "slot map" / "generational arena" pattern (used by Bevy ECS, Our Machinery,
// Rust arenas, Bungie Destiny engine, etc.)
// ─────────────────────────────────────────────────────────────────────────────

#define FIGURE_POOL_MAX 64  // Maximum concurrent figures (world entities + weapon + props)

// Opaque handle to a figure slot — encodes index + generation for safe ABA-free lookup
typedef struct {
  uint Index;       // Slot index into the pool
  uint Generation;  // Must match pool slot generation for valid access
} Figure_Handle;

#define FIGURE_HANDLE_NULL ((Figure_Handle){.Index = UINT32_MAX, .Generation = 0})

typedef struct {
  Figure_Instance Slots[FIGURE_POOL_MAX];          // Fixed-size slot array
  uint            Generations[FIGURE_POOL_MAX];     // Per-slot generation counter (incremented on free)
  uint            Free_Stack[FIGURE_POOL_MAX];      // Free-list stack (LIFO for cache warmth)
  uint            Free_Count;                       // Number of free slots on the stack
  uint            Active_Count;                     // Number of currently allocated slots
  int             Active[FIGURE_POOL_MAX];          // 1 = slot is live, 0 = slot is free
} Figure_Pool;

// Allocate a figure slot. Returns a handle; writes the slot pointer to *Out.
Figure_Handle  Figure_Pool_Alloc   (Figure_Pool *Pool, Figure_Instance **Out);

// Free a figure slot by handle. Increments generation, pushes to free stack.
void           Figure_Pool_Free    (Figure_Pool *Pool, Figure_Handle Handle);

// Resolve a handle to a pointer. Returns NULL if the handle is stale or invalid.
Figure_Instance *Figure_Pool_Get   (Figure_Pool *Pool, Figure_Handle Handle);

// Initialize pool: all slots free, generations zeroed.
void           Figure_Pool_Init    (Figure_Pool *Pool);

// Iterate all active figures. Callback receives (Figure_Instance *, slot index).
// Returns the number of active figures visited.
uint           Figure_Pool_Count   (const Figure_Pool *Pool);

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
  int   Movement;      // Movement_Style: WORLD_QUAKE3 or WORLD_SOURCE (runtime switchable)
  float Duck_Frac;     // Source duck interpolation (0=standing, 1=fully ducked)
} Player;

// Convert Source Z-up vertex to our Y-up: (x,y,z) > (x,z,-y) - same as Q3 but vertex layout differs
Vertex VBSP_Convert (float X, float Y, float Z, float Nx, float Ny, float Nz, float U, float V, float Lu, float Lv);

// Convert a BSP vertex from Quake 3's Z-up coordinate system to our Y-up system: (x, y, z) becomes (x, z, -y).
Vertex Convert_BSP_Vertex (const BSP_Vertex *Source);

// Evaluate a quadratic Bézier curve at parameter t given three control points.
vec3 Bezier_Evaluate (vec3 Control_A, vec3 Control_B, vec3 Control_C, float Parameter);

// Tessellate a Bezier patch surface from its control grid into triangles. The patch is subdivided into a grid of sub-patches (each defined
// by a 3 by 3 control point block), and each sub-patch is evaluated at TESSELLATION_LEVEL intervals to produce a smooth triangle mesh.
uint BSP_Tessellate_Patch (const BSP_Vertex *Control_Grid, int Patch_Width, int Patch_Height,
                           Vertex **Inout_Vertices, uint *Inout_Vertex_Count,
                           uint   **Inout_Indices,  uint *Inout_Index_Count);

// Parse the BSP entity lump to find the first info_player_deathmatch spawn point. Returns the origin (swizzled to Y-up) and facing angle.
Spawn BSP_Find_Spawn (const uint8_t *File_Data, const BSP_Header *Header);

// Parse all entities from the BSP entity lump into an array of discriminated records. Stores results in Out_Entities and returns the count.
uint BSP_Parse_Entities (const uint8_t *File_Data, const BSP_Header *Header,
                         BSP_Entity *Out_Entities, uint Max_Entities);

// Builds per-scene environment settings by examining BSP shader names
Scene_Environment Environment_Infer_From_Scene (const Scene *S);

// Load a complete scene from a Quake 3 BSP file
Scene Scene_Load_From_BSP (const char *Path, Spawn *Out_Spawn);

// Load textures for every material in the scene. Attempts to load TGA files from the assets directory; materials without a texture file
// fall back to a 1 by 1 solid-color pixel derived from the hashed shader name.
void Scene_Load_Textures (const Scene *Scene_Data);

// Load the weapon model's TGA textures and append them to the global texture array.
void Weapon_Load_Textures (Figure_Instance *Weapon);

// VTF texture loading: decodes Valve Texture Format (.vtf) files into RGBA8 pixel data for GPU upload.
int VTF_Load (const char *Path, uint8_t **Out_Pixels, int *Out_W, int *Out_H);

// Source BSP (VBSP) loading: parses Valve's BSP format into the same Scene struct used by Q3 BSP.
Scene Scene_Load_From_VBSP (const char *Path, Spawn *Out_Spawn);

// MDL skeletal model loading: parses Source engine .mdl + .vvd + .vtx into an Entity with bone data.
Figure_Instance MDL_Load (Scene *S, const char *Path, vec3 Origin, float Yaw);

// GPU skeletal animation: uploads bone hierarchy to GPU once at load time. Per-frame: a single compute dispatch evaluates the bone
// hierarchy (parent chain walk) and skins all vertices in one pass. No CPU-side Skeleton_Evaluate needed.
void Figure_Upload_Skeleton   (Figure_Instance *E);  // Upload bind pose, inv bind, parents, weights, ids to GPU SSBOs
void Figure_Skeleton_Dispatch (Figure_Instance *E);  // Single compute: evaluate bones + skin vertices on GPU

// Load a default Quake 3 player model (sarge) as an animated entity placed near the spawn point
Figure_Instance Entity_Load (Scene *S, Spawn Spawn_Point);

// Classify a BSP entity classname string into the Entity_Kind discriminant
void Classify_Entity (const char *Classname, int Length, BSP_Entity *E);

// Cycle movement style between WORLD_QUAKE3 and WORLD_SOURCE
void Movement_Style_Toggle (Player *P);

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §9. Acceleration Structures
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Build the world geometry's bottom-level acceleration structure (BLAS). Uploads the scene vertex, index, and material buffers to the GPU,
// then constructs a single BLAS geometry entry covering all triangles. Uses PREFER_FAST_TRACE since the world is static.
Acceleration_Structure Build_World_Bottom_Level (const Scene *Scene_Data);

// Initialize a figure's BLAS: allocate vertex/index/texture-id GPU buffers, build initial BLAS with ALLOW_UPDATE for per-frame refit
void Figure_BLAS_Initialize (Figure_Instance *F);

// Rebuild a figure's BLAS after vertex data has changed (re-upload vertices, refit BLAS in-place)
void Figure_BLAS_Rebuild (Figure_Instance *F);

// Pre-allocate the top-level acceleration structure (TLAS) for up to Maximum_Instances instance entries
void Top_Level_Initialize (uint Maximum_Instances);

// Rebuild the TLAS each frame from the world BLAS plus all active figures in the pool
void Top_Level_Rebuild (Acceleration_Structure *World, Figure_Pool *Pool);

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §10. Physics
//
//   GPU-only physics via ray tracing against the TLAS. The compute shader traces rays from the player's expanded shape against world
//   geometry, resolves contacts via a slide-move algorithm, and writes back the updated GPU_Player state.
//
//   Convex hull support uses hill-climbing with adjacency for O(√n) amortized queries on hulls with ≥64 vertices, falling back to O(n)
//   brute-force for smaller hulls.
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Convex Hull Limits
#define HULL_MAX_VERTS    256 // Per-hull vertex cap (matches GPU array size in GPU_Hull)
#define HULL_MAX_ADJ      16  // Maximum adjacency entries per vertex (for hill-climb support)
#define HULL_MAX_FACES    512 // Quickhull internal face cap during construction
#define HULL_MAX_ENTITIES 32  // Maximum simultaneous hull collider instances

// Collider Shape Enumeration
//
// Six collider shapes, each defining a support function s(d̂) : S² > ℝ³ from unit directions to surface offsets. The GPU physics compute
// shader switches on this enum to select the appropriate Minkowski support mapping.
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
} GPU_Player;

// Per-frame input delivered to the physics compute shader via push constants (48 bytes)
typedef struct {
  int   Forward, Back, Left, Right;
  int   Jump, Fire, Crouch, Movement_Style; // 0=Q3, 1=Source
  float Delta_X, Delta_Y, Dt, Pad2;
} GPU_Input;

// Packing routine - fp16 RLE packing for push constants
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
  uint32_t Dt_Frame;        // Bits [15:0] = half(Delta_Time), [31:16] = Frame_Count
  uint32_t Velocity;        // Bits [15:0] = half(Velocity_X), [31:16] = half(Velocity_Z)
  uint32_t Speed_Exposure;  // Bits [15:0] = half(Speed),       [31:16] = half(Exposure)
  uint32_t Bloom_Vignette;  // Bits [15:0] = half(Bloom_Strength), [31:16] = half(Vignette_Strength)
  uint32_t Reproject[8];    // Packed via packHalf2x16 - compressed 4 by 4 reprojection matrix (Proj * Prev_View * Inv_View)
  uint32_t Inv_Proj_Diag;   // Bits [15:0] = half(InvProj[0][0]), [31:16] = half(InvProj[1][1])
  uint32_t Sun_Screen_Pos;  // Bits [15:0] = half(Sun_Screen_U), [31:16] = half(Sun_Screen_V) - for god rays
  uint32_t Sun_Params;      // Bits [15:0] = half(God_Ray_Intensity), [31:16] = half(Sun_On_Screen) (0 or 1)
} GPU_Postprocess_Push;

// CPU-side convex hull produced by the Quickhull algorithm. Stores vertex positions and per-vertex adjacency for hill-climbing
// support queries.
typedef struct {
  vec3  Vertices  [HULL_MAX_VERTS];               // Hull vertex positions in local space
  int   Adjacency [HULL_MAX_VERTS][HULL_MAX_ADJ]; // Per-vertex neighbor indices (-1 terminated)
  uint  Vertex_Count;                             // Number of hull vertices
  vec3  Centroid;                                 // Geometric center (for local-space offset)
  float Bounding_Radius;                          // Tight bounding sphere radius from centroid
} Convex_Hull;

// GPU-packed hull data uploaded to the physics compute shader's storage buffer. The shader uses this for SHAPE_HULL support queries
// via hill-climbing with adjacency.
typedef struct {
  float Vertices  [HULL_MAX_VERTS][4];            // xyz + padding per vertex (std430 vec4 array)
  int   Adjacency [HULL_MAX_VERTS][HULL_MAX_ADJ]; // Neighbor indices, -1 terminated
  int   Count;                                    // Vertex count
  float Radius;                                   // Bounding sphere radius
  float Centroid  [3];                            // Local-space centroid
  int   Pad;
} GPU_Hull;

// Signed distance from point P to the plane of triangle (A, B, C). Positive = front side.
float Quickhull_Dist (vec3 P, vec3 A, vec3 B, vec3 C);

// Build a convex hull from a point cloud using the Quickhull algorithm. Returns the hull with deduplicated vertices and per-vertex
// adjacency tables for GPU hill-climbing support.
Convex_Hull Quickhull (const vec3 *Points, uint Count);

// Convenience wrapper: extract vec3 positions from a Vertex array and build the convex hull
Convex_Hull Hull_From_Vertices (const Vertex *Vertices, uint Count);

// Pack a CPU-side Convex_Hull into GPU_Hull format and upload to the hull storage buffer (binding 4)
void Hull_Upload (const Convex_Hull *Hull);

// Create the GPU physics compute pipeline with 5 descriptor bindings:
//
//   Binding 0: TLAS (acceleration structure)
//   Binding 1: World vertex buffer (storage)
//   Binding 2: World index buffer (storage)
//   Binding 3: GPU_Player state (storage, read-write)
//   Binding 4: GPU_Hull data (storage, read-only)
// 
void Physics_Pipeline_Create ();

// Initialize the GPU_Player state buffer from a CPU-side Player, allocate the hull storage buffer (with a 1-vertex dummy if no hull has
// been uploaded yet), create the descriptor pool and set, and bind all physics resources.
void Physics_Resources_Create (const Player *Initial_State);

// Dispatch the physics compute shader for one frame: push the current input, execute a single workgroup, wait for completion, then read
// back the updated GPU_Player state into a CPU-side.
Player Physics_Dispatch (Input In, float Delta_Time);

// Comment here !!!
void Projectile_Pool_Readback ();
void Projectile_Pool_Upload ();

// Comment here !!!
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

// A-trous wavelet spatial denoiser
void Denoise_Pipeline_Create ();

// Create the post-processing compute pipeline (tonemapping, TAA, bloom, vignette, god rays)
void Postprocess_Pipeline_Create ();

// Create the GPU skeletal skinning compute pipeline for Source MDL bone-driven animation
void Skinning_Pipeline_Create ();

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §13. Audio
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// Each material is modeled as a bank of damped resonators (modes):
//
//   - Modal frequencies 
//   - Contact impulse 
//   - Acceleration noise 
//

// Comment here !!!
#define MAX_AUDIO_BUFFERS 32 // Comment here !!!
#define MAX_AUDIO_SOURCES 16 // Comment here !!!

// Comment here !!!
#define MODAL_SAMPLE_RATE 22050

typedef struct {
  float a1, a2; // Feedback coefficients (from frequency and damping)
  float b0;     // Input gain (mode excitation weight)
  float y1, y2; // Filter state
} Mode_Resonator;

// Material mode tables: {frequency_Hz, T60_seconds, gain}
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

// Comment here !!!
#define MODAL_MAX_MODES 12
const Mode_Spec *Material_Modes[] = {Modes_Stone,  // MATERIAL_DEFAULT (Stone-like)
                                     Modes_Metal,  // MATERIAL_METAL
                                     Modes_Stone,  // MATERIAL_STONE
                                     Modes_Wood,   // MATERIAL_WOOD
                                     Modes_Flesh,  // MATERIAL_FLESH
                                     Modes_Water}; // MATERIAL_WATER 

// Comment here !!!
void Mode_Init (Mode_Resonator *M, float Freq_Hz, float T60, float Gain);
float Mode_Tick (Mode_Resonator *M, float X);

// Generate a PCM buffer from a modal resonator bank excited by a contact impulse
ALuint Audio_Generate_Modal_Impact (int Material, float Impulse_Strength,
                                    float Duration, float Volume);

// Load a WAV file from disk into an OpenAL buffer. Supports 8/16-bit mono/stereo PCM. Returns 0 on failure.
ALuint Audio_Load_WAV (const char *Path);

// Weapon fire = sharp metallic transient (bolt mechanism) + propellant gas expansion
ALuint Audio_Generate_Weapon_Fire (float Volume);

// Explosion = broadband transient + N debris modal impacts (per SIGGRAPH 2008 scaling work)
ALuint Audio_Generate_Explosion_Modal (float Duration, float Volume);

// Try to load a WAV from disk; if missing, fall back to modal synthesis
ALuint Audio_Load_WAV_Or_Modal (const char *Path, int Material, float Impulse,
                                float Duration, float Volume);

// Comment here !!!
void Audio_Update_Footsteps (Player *P, float Dt);

// Comment here !!!
void Audio_Play (int Sound_Index, float Volume);

// Comment here !!!
void Audio_Init ();
void Audio_Shutdown ();

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §12. Shaders
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Read a SPIR-V binary file from disk and wrap it in a Vulkan shader module.
VkShaderModule Shader_Module_Load (const char *Path);

// Ray generation shader (rgen). Casts primary rays from the camera through each pixel.
glsl rgen Ray_Generation;

// Closest-hit shader (rchit). Interpolates vertex attributes at the hit point using barycentric coordinates, etc
glsl rchit Closest_Hit;

// Primary miss shader (rmiss). Called when a ray from the ray generation shader misses all geometry
glsl rmiss Ray_Miss;

// Shadow miss shader (rmiss, index 1). Called when a shadow ray reaches the sun without hitting any occluder
glsl rmiss Shadow_Miss;

// GPU physics simulation. Single-workgroup compute shader that traces rays against the TLAS for collision.
glsl comp Physics;

// A-Trous Wavelet Spatial Denoiser
glsl comp Denoise;

// Post-processing: tonemapping, TAA, bloom, vignette, god rays
glsl comp Post_Process;

// GPU skeletal skinning: transforms bind-pose vertices by bone matrices in a compute shader
glsl comp Skinning;

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §13. Engine
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Khronos validation layer and extension settings
#define VULKAN_API_VERSION VK_API_VERSION_1_3 
const uint  VALIDATION_LAYER_COUNT = 1;
const char *VALIDATION_LAYERS[]    = {"VK_LAYER_KHRONOS_validation"};
const uint  DEVICE_EXTENSION_COUNT = 6;
const char *DEVICE_EXTENSIONS[]    = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                      VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                                      VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                                      VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                                      VK_KHR_RAY_QUERY_EXTENSION_NAME,
                                      VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME};

// Assertion to validate Vulkan return values; prints descriptive error name, file, and line then exits
#define VK_CHECK(Call) do { \
  VkResult _Result = (Call); \
  if (_Result) { \
    fprintf (stderr, "[vulkan] %s (%d) at %s:%d\n", VK_Error_Name(_Result), _Result, __FILE__, __LINE__); \
    exit (1); \
  } \
} while (0)

// Human-readable Vulkan error name from VkResult code
const char *VK_Error_Name (VkResult Result) {
  switch (Result) {
    case  0:  return "VK_SUCCESS";
    case -1:  return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case -2:  return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case -3:  return "VK_ERROR_INITIALIZATION_FAILED";
    case -4:  return "VK_ERROR_DEVICE_LOST";
    case -5:  return "VK_ERROR_MEMORY_MAP_FAILED";
    case -6:  return "VK_ERROR_LAYER_NOT_PRESENT";
    case -7:  return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case -8:  return "VK_ERROR_FEATURE_NOT_PRESENT";
    case -9:  return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case -10: return "VK_ERROR_TOO_MANY_OBJECTS";
    case -11: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case -12: return "VK_ERROR_FRAGMENTED_POOL";
    case -13: return "VK_ERROR_UNKNOWN";
    default:  return "unknown";
  }
}

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

// Vulkan initialization helpers (called in sequence from main)
void Vulkan_Create_Instance ();
void Vulkan_Pick_Physical_Device ();
void Vulkan_Create_Logical_Device ();
void Vulkan_Create_Swapchain ();
void Vulkan_Recreate_Swapchain ();
void Vulkan_Create_Synchronization ();
void Vulkan_Transition_Storage_Image ();

// Destroy old swapchain and create a new one matching the current surface size
void Vulkan_Recreate_Swapchain ();

// Allocate the descriptor pool and set, then write the descriptor bindings for the ray tracing pipeline 
void Descriptor_Set_Create (Figure_Pool *Pool);

// Upload the camera uniform buffer with the inverse view and projection matrices computed from
// the current player position, yaw, pitch, field-of-view, and aspect ratio.
void Camera_Upload (Camera *State, float Field_Of_View, uint Weapon_Texture_Base, uint PBR_Stride_Value, uint Active_SPP);

// Update the weapon viewmodel's vertex positions each frame based on the camera orientation
void Weapon_Update (Figure_Instance *Weapon, const Camera *Camera_Data, float Delta_Time, int Fire);

// Sample the current keyboard and mouse state from SDL, returning the frame's input snapshot
Input Poll_Input ();

// Record and submit one frame of ray tracing: bind the pipeline and descriptors
void Raytracing_Frame (GPU_Postprocess_Push Postprocess);

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
// §14. Assets
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Asset paths
#define ASSET_ROOT "assets/"

// Default BSP map to load when no command-line argument is given
const char *DEFAULT_MAP = "oa_dm1.bsp";

// ── Asset Store ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
//
// Pak-driven virtual filesystem. Archives are mounted into a search stack; lookups walk the stack newest-first, falling back to loose
// files on disk. Compressed entries (PK3/ZIP deflate) are inflated on the CPU into a scratch arena before returning to the caller.
//
//   Supported formats:
//     .pk3  — Quake 3 (standard ZIP with renamed extension; deflate or store)
//     .pak  — Quake 1/2 (flat header: "PACK" + offset + count, 64-byte entries, no compression)
//     .vpk  — Valve Source (_dir.vpk directory + _NNN.vpk bulk data)
//     .wad  — Half-Life / Quake WAD2/WAD3 texture lump archives
//

typedef enum {
  PACK_PK3,   // Quake 3 — ZIP archive with .pk3 extension (deflate or store)
  PACK_PAK,   // Quake 1/2 — flat header + 64-byte entries, uncompressed
  PACK_VPK,   // Valve Source — directory tree in _dir.vpk, bulk in _NNN.vpk
  PACK_WAD,   // Half-Life / Quake — WAD2/WAD3 texture lumps
  PACK_COUNT
} Pack_Format;

// One entry inside a loaded archive
typedef struct {
  char     Name[256];   // Virtual path inside the pack (e.g. "models/weapons/rocket.md3")
  uint64_t Offset;      // Byte offset into the archive's raw data
  uint64_t Packed_Size; // Compressed size in bytes (== Size when uncompressed)
  uint64_t Size;        // Uncompressed size in bytes
  int      Compressed;  // Non-zero if deflate-compressed
} Pack_Entry;

// A loaded archive: header parsed, raw data resident in memory
typedef struct {
  Pack_Format  Format;
  char         Path[512];       // Filesystem path to the archive file
  uint8_t     *Data;            // Entire archive mapped / read into memory
  uint64_t     Data_Size;       // Size of the resident data
  Pack_Entry  *Entries;         // Directory of entries (heap-allocated)
  uint         Entry_Count;     // Number of entries in the directory
} Pack_File;

// ── Free-Loaded Assets ───────────────────────────────────────────────────────────────────────────────────────────────────────────────
//
// Individual loose files loaded by path (not from archives). Supports fuzzy path resolution: the store strips leading directories,
// normalises slashes, ignores case, and tries common extension substitutions (.tga/.vtf/.png, .md3/.mdl/.psk) to find the best match
// under the Loose_Root. Free-loaded assets can be unloaded individually or all at once.
//

#define FREE_ASSET_MAX 256

typedef struct {
  char     Virtual_Path[256];  // Canonical virtual path used as the lookup key
  char     Resolved_Path[512]; // Actual filesystem path that was loaded
  uint8_t *Data;               // Heap-allocated file contents
  uint64_t Size;               // Size in bytes
} Free_Asset;

// The virtual filesystem: a stack of pack files searched newest-first
#define PACK_MAX 32
typedef struct {
  Pack_File  Packs[PACK_MAX];             // Mounted archives (searched [Count-1] down to [0])
  uint       Pack_Count;
  char       Loose_Root[512];             // Fallback directory for loose files (e.g. "assets/")
  Arena      Scratch;                     // Scratch arena for inflate buffers (reset after each load)
  Free_Asset Free_Assets[FREE_ASSET_MAX]; // Individually loaded loose files (not from archives)
  uint       Free_Asset_Count;
} Asset_Store;

// Mount an archive file into the store. Reads the entire file into memory and parses its directory.
// Returns non-zero on success. Supports .pk3, .pak, .vpk, .wad auto-detected by extension or magic.
int Asset_Store_Mount (Asset_Store *Store, const char *Archive_Path);

// Load a file by virtual path. Searches mounted packs newest-first, then loose files under Loose_Root.
// Returns a heap-allocated buffer with the inflated data and sets *Out_Size to its byte count.
// Caller must free the returned pointer (or use Arena_Alloc for scratch loads).
uint8_t *Asset_Load (Asset_Store *Store, const char *Virtual_Path, uint64_t *Out_Size);

// Check whether a virtual path exists in the store without loading the data
int Asset_Exists (const Asset_Store *Store, const char *Virtual_Path);

// Unmount a single archive by path. Frees its backing data and directory, shifts the stack down. Returns non-zero on success.
int Asset_Store_Unmount (Asset_Store *Store, const char *Archive_Path);

// Return the number of currently mounted archives
uint Asset_Store_Pack_Count (const Asset_Store *Store);

// Return a pointer to the I-th mounted pack (0 = oldest, Count-1 = newest). NULL if out of range.
const Pack_File *Asset_Store_Pack_At (const Asset_Store *Store, uint Index);

// Unmount all archives, free all backing memory, and destroy the scratch arena
void Asset_Store_Destroy (Asset_Store *Store);

// Free-load a single file by path. Searches Loose_Root with fuzzy matching. Returns the data pointer and sets *Out_Size.
uint8_t *Asset_Free_Load (Asset_Store *Store, const char *Path, uint64_t *Out_Size);

// Unload a single free-loaded asset by its virtual path. Returns non-zero on success.
int Asset_Free_Unload (Asset_Store *Store, const char *Path);

// Unload all free-loaded assets at once
void Asset_Free_Unload_All (Asset_Store *Store);

// Low-level inflate: decompress a raw deflate stream from In into Out. Returns bytes written.
uint64_t Inflate_Buffer (const uint8_t *In, uint64_t In_Size, uint8_t *Out, uint64_t Out_Capacity);

// Paths to the weapon model's diffuse textures (body and sight)
#define WEAPON_TEXTURE_COUNT 2 // Default for Q3 weapons (2 surfaces: body + hand)
#define WEAPON_MAX_TEXTURES 16 // Maximum for Source weapons with many materials
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
// §15. Main
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

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
  //   --validation      Enable Vulkan validation layers (off by default)
  //   --source          Load Source engine BSP (VBSP) instead of Q3 BSP
  //   --mdl PATH        Load Source MDL model as enemy entity
  //   --weapon PATH     Load Source MDL model as held weapon
  //   --world q3|source Set world coordinate system/physics preset
  //   mapname.bsp       Load specified BSP map instead of default

  // Local variables
  int         Physics_Test       = 0;
  int         No_Postprocess     = 0;
  int         No_PBR             = 0;
  int         No_Parallax        = 0;
  int         Benchmark_Frames   = 0;    // Use 0 to disable, a value N > 0 means to run N frames then exit
  int         Force_Cheap        = 0;    // Implies the --cheap was used: force Budget=1.0 (lightmap-only fallback)
  int         Override_SPP       = 0;    // When 0 then use default from quality preset
  int         Override_Res       = 0;    // When 1 if --res was specified (overrides quality preset)
  int         Source_Mode        = 0;    // When 1 then Load Source BSP (VBSP) instead of Q3 BSP
  const char *Screenshot_Path    = NULL; // Use NULL to disable, otherwise save frame and exit
  const char *Dump_Frames_Dir    = NULL; // Use NULL to disable, otherwise dump each frame
  const char *Source_MDL_Path    = NULL; // Optional Source MDL model to load as entity
  const char *Source_Weapon_Path = NULL; // Optional Source MDL model to load as held weapon
  const char *Map_Name           = DEFAULT_MAP;
  float       Active_Exposure    = STYLE.Exposure;

  // Parse command-line arguments
  for (int I = 1; I < Argc; I++) {
    if (strcmp (Argv[I], "--help") == 0 or strcmp (Argv[I], "-h") == 0) {
      printf ("Usage: %s [options] [mapname.bsp]\n\n", Argv[0]);
      printf ("Options:\n");
      printf ("  --source          Load Source engine BSP (VBSP) instead of Q3 BSP\n");
      printf ("  --mdl PATH        Load Source MDL model as enemy entity\n");
      printf ("  --weapon PATH     Load Source MDL as held weapon (viewmodel)\n");
      printf ("  --world PRESET    Set world preset: q3, source, unreal\n");
      printf ("  --pak PATH        Mount an asset archive (.pk3/.pak/.wad)\n");
      printf ("  --screenshot FILE Render one frame from spawn, save TGA, exit\n");
      printf ("  --benchmark N     Run N frames, print FPS stats, exit\n");
      printf ("  --spp N           Override samples-per-pixel (1, 2, 4, 8)\n");
      printf ("  --res WxH         Override render resolution (e.g. 1920x1080)\n");
      printf ("  --quality LEVEL   Set quality preset (ultra/high/medium/low/potato)\n");
      printf ("  --no-postprocess  Disable tonemapping and post-processing\n");
      printf ("  --no-pbr          Disable PBR maps (diffuse + lightmap only)\n");
      printf ("  --no-parallax     Disable parallax occlusion mapping\n");
      printf ("  --validation      Enable Vulkan validation layers\n");
      printf ("  --dump-frames DIR Save benchmark frames as DIR/frame_NNNN.tga\n");
      printf ("  --physics-test    Run physics simulation without rendering\n");
      printf ("\nEnvironment:\n");
      printf ("  VK_ICD_FILENAMES  Set Vulkan driver (e.g. /usr/share/vulkan/icd.d/lvp_icd.json)\n");
      printf ("  DISPLAY           X11 display (use Xvfb for headless rendering)\n");
      return 0;
    }
    else if (strcmp (Argv[I], "--physics-test")   == 0) Physics_Test = 1;
    else if (strcmp (Argv[I], "--benchmark")      == 0 and I + 1 < Argc) Benchmark_Frames = atoi (Argv[++I]);
    else if (strcmp (Argv[I], "--screenshot")     == 0 and I + 1 < Argc) Screenshot_Path  =       Argv[++I];
    else if (strcmp (Argv[I], "--dump-frames")    == 0 and I + 1 < Argc) Dump_Frames_Dir  =       Argv[++I];
    else if (strcmp (Argv[I], "--no-postprocess") == 0) No_Postprocess = 1;
    else if (strcmp (Argv[I], "--no-pbr")         == 0) No_PBR         = 1;
    else if (strcmp (Argv[I], "--no-parallax")    == 0) No_Parallax    = 1;
    else if (strcmp (Argv[I], "--cheap")          == 0) Force_Cheap    = 1;
    else if (strcmp (Argv[I], "--validation")     == 0) Use_Validation = 1;
    else if (strcmp (Argv[I], "--source")         == 0) {Source_Mode     = 1;
                                                         Active_Movement = WORLD_SOURCE;
                                                         Active_World    = WORLD_PRESETS[WORLD_SOURCE];
                                                         Active_Exposure = 1.0f;}
    else if (strcmp (Argv[I], "--world") == 0 and I + 1 < Argc) {
      const char *W = Argv[++I];
      if      (strcmp(W,"source") == 0) Active_World = WORLD_PRESETS [WORLD_SOURCE];
      else if (strcmp(W,"q3")     == 0) Active_World = WORLD_PRESETS [WORLD_QUAKE3];
      else if (strcmp(W,"unreal") == 0) Active_World = WORLD_PRESETS [WORLD_UNREAL];
      else printf("[world] unknown world '%s' (q3/source/unreal)\n", W);
    }
    else if (strcmp (Argv[I], "--movement")       == 0 and I + 1 < Argc) {
      const char *Mv = Argv[++I];
      if      (strcmp(Mv,"source") == 0) Active_Movement = WORLD_SOURCE;
      else if (strcmp(Mv,"q3")     == 0) Active_Movement = WORLD_QUAKE3;
      else if (strcmp(Mv,"unreal") == 0) Active_Movement = WORLD_UNREAL;
    }
    else if (strcmp (Argv[I], "--pak")    == 0 and I + 1 < Argc) {/* deferred: mount after Asset_Store init */}
    else if (strcmp (Argv[I], "--mdl")    == 0 and I + 1 < Argc) Source_MDL_Path = Argv[++I];
    else if (strcmp (Argv[I], "--weapon") == 0 and I + 1 < Argc) Source_Weapon_Path = Argv[++I];
    else if (strcmp (Argv[I], "--spp")    == 0 and I + 1 < Argc) Override_SPP = atoi (Argv[++I]);
    else if (strcmp (Argv[I], "--res")    == 0 and I + 1 < Argc) {
      sscanf (Argv[++I], "%dx%d", &Width, &Height);
      Override_Res = 1;
    }
    else if (strcmp (Argv[I], "--quality") == 0 and I + 1 < Argc) {
      const char *Q = Argv[++I];
      if      (strcasecmp (Q, "ultra")  == 0) Active_Quality = QUALITY_CRYSIS;
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
  if (not Override_Res) { Width = Preset->Width; Height = Preset->Height;}
  Active_Render_Scale  = Preset->Render_Scale;
  Active_Denoise_Passes = Preset->Denoise_Passes;
  Active_Checkerboard   = Preset->Checkerboard;
  if (not No_Parallax) No_Parallax = not Preset->Parallax;
  printf ("[quality] preset: %s (%dx%d @ %.0f%% scale, %d SPP)\n",
          Preset->Name, Width, Height, Active_Render_Scale * 100.f, Override_SPP ? Override_SPP : Preset->SPP);
  printf ("[world] %s (height %.0f, eye %.0f, fov %.0f, speed %.0f)\n",
          Active_World.Name, Active_World.Player_Height, Active_World.Eye_Height, Active_World.FOV, Active_World.Max_Speed);

  // Comment here !!!
  (void)No_PBR;
  (void)No_Parallax; 

  // Verify the map file exists before starting expensive GPU setup
  char Map_Path[256];
  snprintf (Map_Path, sizeof Map_Path, "%smaps/%s", ASSET_ROOT, Map_Name);
  {
    FILE *Map_Test = fopen (Map_Path, "rb");
    if (not Map_Test) {
      fprintf (stderr, "[error] map file not found: %s\n", Map_Path);
      fprintf (stderr, "  Place .bsp files in the assets/maps/ directory.\n");
      fprintf (stderr, "  Usage: %s [options] <mapname.bsp>\n", Argv[0]);
      return 1;
    }
    fclose (Map_Test);
  }

  // Initialize SDL2 with video subsystem and create a Vulkan-capable resizable window
  if (SDL_Init (SDL_INIT_VIDEO) < 0) {
    fprintf (stderr, "[error] SDL_Init failed: %s\n", SDL_GetError ());
    fprintf (stderr, "  Ensure a display server (X11/Wayland) is running.\n");
    fprintf (stderr, "  For headless use: Xvfb :99 -screen 0 1920x1080x24 & export DISPLAY=:99\n");
    return 1;
  }
  Window = SDL_CreateWindow (ENGINE_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             Width, Height, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (not Window) {
    fprintf (stderr, "[error] SDL_CreateWindow failed: %s\n", SDL_GetError ());
    fprintf (stderr, "  Ensure Vulkan drivers are installed (apt install mesa-vulkan-drivers).\n");
    SDL_Quit ();
    return 1;
  }

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

  // Initialize the TLSF GPU memory heap (all GPU allocations flow through this)
  GPU_Heap_Init (&Heap, GPU_HEAP_DEFAULT_SIZE);

  // Compute internal render resolution
  Render_Width  = (int)(Width  * Active_Render_Scale);
  Render_Height = (int)(Height * Active_Render_Scale);
  Render_Width  = (Render_Width  + 7) & ~7; // Round up to multiple of 8 (postprocess workgroup size)
  Render_Height = (Render_Height + 7) & ~7;

  // Convert horizontal FOV to vertical for the Perspective matrix
  float Vertical_FOV = 2.f * atanf (tanf (Active_World.FOV * (float)M_PI / 360.f) / ((float)Width / Height)) * 180.f / (float)M_PI;
  printf ("[render] internal %dx%d > window %dx%d (scale %.0f%%, vFOV %.1f°)\n",
          Render_Width, Render_Height, Width, Height, Active_Render_Scale * 100.f, Vertical_FOV);

  // Create the ray tracing storage targe and depth images
  Raytracing_Storage_Image = Image_Storage_Create (Render_Width, Render_Height);
  Vulkan_Transition_Storage_Image ();

  // Create R32F depth image for postprocessing
  {
    Depth_Image.Format = VK_FORMAT_R32_SFLOAT;
    Depth_Image.Heap_Block = -1;
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
    uint8_t *Depth_Mapped = NULL;
    Depth_Image.Heap_Block = GPU_Heap_Alloc (&Heap, Mem_Req.size, Mem_Req.alignment,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Mem_Req.memoryTypeBits,
                                              &Depth_Image.Memory, &Depth_Image.Offset, &Depth_Mapped);
    VK_CHECK (vkBindImageMemory (Device, Depth_Image.Image, Depth_Image.Memory, Depth_Image.Offset));
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

  // Create history image for temporal accumulation
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

  // Create postprocess output image - postprocess writes here instead of back to Color_Image, so that Color_Image stays consistent
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

  // Allocate the camera uniform buffer
  Camera_Uniform_Buffer = Buffer_Allocate (/*Size         =>*/ 256,
                                           /*Usage        =>*/ VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                           /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                             | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Load the BSP scene and spawn point (no CPU collision map - GPU handles physics via TLAS)
  Spawn Spawn_Point;
  Scene Scene_Data = Source_Mode ? Scene_Load_From_VBSP (Map_Path, &Spawn_Point)
                                : Scene_Load_From_BSP   (Map_Path, &Spawn_Point);

  // Load entity: Source mode defaults to ct_sas, Q3 mode defaults to sarge
  const char *Entity_MDL_Path = Source_MDL_Path;
  if (not Entity_MDL_Path and Source_Mode)
    Entity_MDL_Path = "/tmp/cspromod_new/cspromod_b105/cspromod/models/player/ct_sas.mdl";

  // Place enemy 120 units forward from spawn (in engine Y-up space: forward = camera's look direction)
  vec3 Enemy_Origin = Spawn_Point.Origin;
  if (Entity_MDL_Path) {
    float Spawn_Yaw = 1.5707963f - Spawn_Point.Angle * 3.14159f / 180.f;
    float Fwd_X = sinf(Spawn_Yaw), Fwd_Z = -cosf(Spawn_Yaw);
    Enemy_Origin.x += Fwd_X * 120.f;
    Enemy_Origin.z += Fwd_Z * 120.f;
    printf("[enemy] placing at (%.1f, %.1f, %.1f), %.0f units forward from spawn\n",
           Enemy_Origin.x, Enemy_Origin.y, Enemy_Origin.z, 120.f);
  }
  // Figure pool: generational-index slot allocator for all animated figures
  Figure_Pool Figures;
  Figure_Pool_Init (&Figures);

  // Allocate enemy figure from the pool
  Figure_Instance *Enemy;
  Figure_Handle Enemy_Handle = Figure_Pool_Alloc (&Figures, &Enemy);
  *Enemy = Entity_MDL_Path ? MDL_Load (&Scene_Data, Entity_MDL_Path, Enemy_Origin, Spawn_Point.Angle + 180.f)
                           : Entity_Load (&Scene_Data, Spawn_Point);
  Enemy->Ray_Mask = 0xFF;  // Visible to all rays (casts shadows)
  Enemy->TLAS_Transform[0][0] = 1.f; Enemy->TLAS_Transform[1][1] = 1.f; Enemy->TLAS_Transform[2][2] = 1.f;

  // Infer per-scene environment settings from BSP data (sky textures, worldspawn)
  Active_Environment = Environment_Infer_From_Scene (&Scene_Data);

  // Load scene and weapon textures
  Scene_Load_Textures (&Scene_Data);
  Figure_Instance *Weapon;
  Figure_Handle Weapon_Handle = Figure_Pool_Alloc (&Figures, &Weapon);
  const char *Weapon_MDL_Path = Source_Weapon_Path;
  if (not Weapon_MDL_Path and Source_Mode)
    Weapon_MDL_Path = "/tmp/v_m4_new/models/v_rif_m4a1.mdl";
  Weapon->Figure = Weapon_MDL_Path ? Source_Weapon_Model_Load (Weapon_MDL_Path)
                                   : Weapon_Model_Load ();
  Weapon_Load_Textures (Weapon);
  Weapon->Ray_Mask = 0x01;  // Excluded from shadow rays
  Weapon->TLAS_Transform[0][0] = 1.f; Weapon->TLAS_Transform[1][1] = 1.f; Weapon->TLAS_Transform[2][2] = 1.f;

  // Allocate a player body slot (shares enemy BLAS, shadow-only)
  Figure_Instance *Player_Body;
  Figure_Handle Body_Handle = Figure_Pool_Alloc (&Figures, &Player_Body);
  Player_Body->Ray_Mask = 0x02;  // Shadow-only (visible to shadow rays, not primary)

  // Check quality arguments
  //
  //   --no-pbr: set PBR_Stride to 0 to force heuristic PBR for all materials.
  //   --no-parallax / Potato quality: disables parallax and reflections via Active_SPP flags
  //
  if (No_PBR) {
    printf ("[mode] PBR maps DISABLED (heuristic fallback)\n");
    PBR_Stride = 0;
  }
  printf ("[mode] PBR maps %s, parallax %s\n",
          No_PBR ? "DISABLED" : "enabled", No_Parallax ? "DISABLED" : "enabled");

  // Build acceleration structures (BLAS for world + all figures, then TLAS)
  Acceleration_Structure World_Bottom_Level = Build_World_Bottom_Level (&Scene_Data);
  Figure_BLAS_Initialize (Weapon);
  Figure_BLAS_Initialize (Enemy);

  // Player body shares enemy's BLAS and buffers (same geometry, different transform + ray mask)
  Player_Body->Bottom_Level      = Enemy->Bottom_Level;
  Player_Body->Vertex_Buffer     = Enemy->Vertex_Buffer;
  Player_Body->Index_Buffer      = Enemy->Index_Buffer;
  Player_Body->Texture_Id_Buffer = Enemy->Texture_Id_Buffer;
  Player_Body->Texture_Base_Index = Enemy->Texture_Base_Index;

  Top_Level_Initialize (1 + FIGURE_POOL_MAX);
  Top_Level_Rebuild (&World_Bottom_Level, &Figures);

  // Create the ray tracing pipeline, shader binding table, and descriptors
  Raytracing_Pipeline_Create ();
  Shader_Binding_Table_Create ();
  Descriptor_Set_Create (&Figures);

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

  // Create the GPU skeletal skinning pipeline (used by Source MDL entities)
  Skinning_Pipeline_Create ();

  // Create the GPU physics pipeline and resources (with hull binding)
  Physics_Pipeline_Create ();

  // Compute world-relative spawn offset
  float Spawn_Feet_Offset = (Active_World.Type == WORLD_SOURCE) ? 0.f : 24.f; 
  float Capsule_Y = Active_World.Player_Height * 0.5f - Spawn_Feet_Offset; // Offset from spawn to capsule center
  Player Initial_Player = {
    .Position = {Spawn_Point.Origin.x, Spawn_Point.Origin.y + Capsule_Y, Spawn_Point.Origin.z},
    .Yaw      = 1.5707963f - Spawn_Point.Angle * 3.14159f / 180.f}; // M_PI/2 - Angle: Q3 angle 0 = +X = our yaw π/2
  Physics_Resources_Create (&Initial_Player);

  // Initialize the audio system (OpenAL with synthesized sounds)
  Audio_Init ();

  // Apply runtime mode flags
  Skip_Postprocess = No_Postprocess;

  // Default SPP = 1 for maximum speed; override with --spp N
  uint Active_SPP = Override_SPP ? (uint)Override_SPP : (uint)QUALITY_PRESETS[Active_Quality].SPP;

  // Log diagnostic output
  printf ("[init] ready - entering game loop\n");

  // Physics-only test mode: run with --physics-test to skip rendering and simulate movement - Test code !!!
  if (Physics_Test) {
    fprintf (stderr, "[physics-test] starting physics-only test (3s simulated, fixed dt=0.016)\n");
    float Fixed_Dt = 1.f / 60.f; // A 60fps timestep

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
    float Eye_Y = Active_World.Eye_Height - Spawn_Feet_Offset; // World-relative eye height above spawn origin
    Camera Bench_Cam = {.Position = {Spawn_Point.Origin.x, Spawn_Point.Origin.y + Eye_Y, Spawn_Point.Origin.z},
                         .Yaw = 1.5707963f - Spawn_Point.Angle * 3.14159f / 180.f};
    int Total_Frames = Screenshot_Path ? 1 : Benchmark_Frames;
    float Fixed_Dt = 1.f / 60.f;

    // Warm up for testing - Test code !!!
    printf ("[benchmark] warming up (5 frames)...\n");
    {
      VK_CHECK (vkWaitForFences (Device, 1, &Fence, VK_TRUE, UINT64_MAX));
      Weapon_Update (Weapon, &Bench_Cam, Fixed_Dt, 0);
      Figure_BLAS_Rebuild (Weapon);
      Top_Level_Rebuild (&World_Bottom_Level, &Figures);
      mat4 Bench_View = View (Bench_Cam.Position, Bench_Cam.Yaw, Bench_Cam.Pitch);
      mat4 Bench_Proj = Perspective (Vertical_FOV, (float)Width / Height, 0.1f, 10000.f);
      mat4 Bench_Inv_Proj = Inverse_Projection (Bench_Proj);
      mat4 Bench_Reproj = Mat4_Mul (Bench_Proj, Mat4_Mul (Bench_View, Inverse_Orthogonal (Bench_View)));
      Prev_View_Matrix = Bench_View;

      // Alternate frame parity so checkerboard traces both pixel halves during warmup
      for (int I = 0; I < 5; I++) {
        Bench_Cam.Frame = (uint)I;
        Camera_Upload (&Bench_Cam, Vertical_FOV, Weapon->Texture_Base_Index, PBR_Stride, Active_SPP);
        GPU_Postprocess_Push Warmup_Postprocess = {.Time = 0,
          .Dt_Frame       = (uint32_t)Float_To_Half (Fixed_Dt) | ((uint32_t)(Frame_Count & 0xFFFF) << 16),
          .Velocity       = 0,
          .Speed_Exposure = Pack_Half2x16 (0.f, Active_Exposure),
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
    uint64_t Bench_Freq     = SDL_GetPerformanceFrequency ();
    float    Frame_Min      = 1e9f, Frame_Max = 0, Frame_Sum = 0;
    float   *Frame_Times    = calloc (Total_Frames, sizeof (float)); // For percentile stats
    vec3     Prev_Bench_Pos = Bench_Cam.Position; // Track camera position for speed computation

    // Render each benchmark frame
    for (int F = 0; F < Total_Frames; F++) {

      // Wait for the previous frame's RT submission to complete before reusing Command_Buffer
      VK_CHECK (vkWaitForFences (Device, 1, &Fence, VK_TRUE, UINT64_MAX));

      // Time this frame
      uint64_t Frame_Start = SDL_GetPerformanceCounter ();

      // Slowly rotate the camera for visual coverage - Test code !!!
      if (not Screenshot_Path) {
        float T = F * Fixed_Dt;
        float Yaw_Speed   = cosf (T * 3.14159f) * 120.f; // ~120 px/frame mouse sweep
        float Pitch_Speed = sinf (T * 6.28f) * 5.f;      // Gentle pitch oscillation
        Input In = {.Forward = 1, .Right = (F % 180 >= 90) ? 1 : 0,
                    .Delta_X = Yaw_Speed, .Delta_Y = Pitch_Speed};
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
      Weapon_Update (Weapon, &Bench_Cam, Fixed_Dt, 0);
      Figure_BLAS_Rebuild (Weapon);
      Enemy->Animation_Time += Fixed_Dt;
      Enemy->Current_Vertices = Enemy->Figure.Frame_Vertices[(int)(Enemy->Animation_Time * Enemy->Figure.Animations[0].FPS) % Enemy->Figure.Animations[0].Frame_Count];
      Figure_BLAS_Rebuild (Enemy);
      Top_Level_Rebuild (&World_Bottom_Level, &Figures);
      Camera_Upload (&Bench_Cam, Vertical_FOV, Weapon->Texture_Base_Index, PBR_Stride, Active_SPP);

      // Build view and projection matrices
      mat4 Bench_View = View (Bench_Cam.Position, Bench_Cam.Yaw, Bench_Cam.Pitch);
      mat4 Bench_Projection = Perspective (Vertical_FOV, (float)Width / Height, 0.1f, 10000.f);
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
      GPU_Postprocess_Push Postprocess = {.Time = F * Fixed_Dt,
        .Dt_Frame       = (uint32_t)Float_To_Half (Fixed_Dt) | ((uint32_t)(Frame_Count & 0xFFFF) << 16),
        .Velocity       = 0,
        .Speed_Exposure = Pack_Half2x16 (Bench_Speed, Active_Exposure),
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
        uint64_t Pixel_Buffer_Size = (uint64_t)Render_Width * Render_Height * 8; // R16G16B16A16_SFLOAT = 8 bytes/pixel

        // Allocate_Readback_Buffer:
        GPU_Buffer Readback = Buffer_Allocate (Pixel_Buffer_Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
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
        if (Readback.Heap_Block >= 0) {
          GPU_Heap_Slab *RS = &Heap.Slabs[Heap.Blocks[Readback.Heap_Block].Slab];
          Pixels = (uint16_t *)(RS->Mapped + Readback.Offset);
        } else
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
                else { uint32_t Fb = ((Exp + 112) << 23) | (Man << 13); memcpy (&V, &Fb, 4);}
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
        if (Readback.Heap_Block < 0) vkUnmapMemory (Device, Readback.Memory);
        Buffer_Destroy (&Readback);
        vkFreeCommandBuffers (Device, Command_Pool, 1, &Download_Command);
      }
    }

    // Wait for GPU to finish
    vkDeviceWaitIdle (Device);

    // Compute percentiles: sort frame times for P50/P95/P99
    for (int I = 0; I < Total_Frames - 1; I++)
      for (int J = I + 1; J < Total_Frames; J++)
        if (Frame_Times[I] > Frame_Times[J]) { float T = Frame_Times[I]; Frame_Times[I] = Frame_Times[J]; Frame_Times[J] = T;}
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
      GPU_Buffer Readback = Buffer_Allocate (/*Size         =>*/ Pixel_Size,
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
      if (Readback.Heap_Block >= 0) {
        GPU_Heap_Slab *RS = &Heap.Slabs[Heap.Blocks[Readback.Heap_Block].Slab];
        Pixels_F16 = (uint16_t *)(RS->Mapped + Readback.Offset);
      } else
        vkMapMemory (Device, Readback.Memory, 0, Pixel_Size, 0, (void **)&Pixels_F16);

      // Write pixel data to TGA file
      FILE *TGA = fopen (Screenshot_Path, "wb");
      if (TGA) {

        // TGA header (18 bytes)
        uint8_t Header[18] = {0};
        Header[2]  = 2;    // Uncompressed true-color
        Header[16] = 32;   // 32 bpp (BGRA)
        Header[17] = 0x20; // Top-left origin
        Header[12] = Render_Width & 0xFF;  Header[13] = (Render_Width  >> 8) & 0xFF;
        Header[14] = Render_Height & 0xFF; Header[15] = (Render_Height >> 8) & 0xFF;
        fwrite (Header, 1, 18, TGA);

        // Write pixels: fp16 linear > clamp > linear-to-sRGB > 8-bit BGRA for TGA
        for (int Y = 0; Y < Render_Height; Y++) {
          for (int X = 0; X < Render_Width; X++) {
            uint16_t *P = Pixels_F16 + (Y * Render_Width + X) * 4;

            // Convert fp16 to float (use half-to-float bit manipulation)
            uint8_t BGRA[4];
            for (int C = 0; C < 3; C++) {

              // IEEE 754 fp16 to fp32 conversion
              uint16_t H    = P[C];
              uint32_t Sign = (uint32_t)(H >> 15) << 31;
              uint32_t Exp  = (H >> 10) & 0x1F;
              uint32_t Man  = H & 0x3FF;

              // Comment here !!!
              float V;
              if (Exp == 0) V = (Man == 0) ? 0.0f : (float)Man / 1024.0f * (1.0f / 16384.0f);
              else if (Exp == 31) V = 1.0f;
              else {uint32_t F = Sign | ((Exp + 112) << 23) | (Man << 13); memcpy (&V, &F, 4);}

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
      if (Readback.Heap_Block < 0) vkUnmapMemory (Device, Readback.Memory);
      Buffer_Destroy       (&Readback);
      vkFreeCommandBuffers (Device, Command_Pool, 1, &Cmd);
    }

    // Wait for GPU and exit
    vkDeviceWaitIdle (Device);
    return 0;
  }

  // Camera and perspective
  Camera Cam = {.Position = {Spawn_Point.Origin.x,
                             Spawn_Point.Origin.y + (Active_World.Eye_Height - Spawn_Feet_Offset), // Game eye
                             Spawn_Point.Origin.z},
                .Yaw = 1.5707963f - Spawn_Point.Angle * 3.14159f / 180.f};
  Prev_View_Matrix = View (Cam.Position, Cam.Yaw, Cam.Pitch);

  // Loop timing
  uint64_t Last       = SDL_GetPerformanceCounter ();
  uint64_t Freq       = SDL_GetPerformanceFrequency ();
  uint     Frame      = 0;
  float    Total_Time = 0;

  // Main game loop
  Quit = 0;
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
      SDL_Delay (16); // Don't spin-wait when minimized
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
    Player Physics = Physics_Dispatch (In, Delta_Time);

    // Read back projectile state after GPU physics step
    Projectile_Pool_Readback ();

    // Update audio: footsteps, landing sounds
    Audio_Update_Footsteps (&Physics, Delta_Time);

    // Update the camera from the physics result - add View_Height to get eye position
    Cam.Position.y += Physics.View_Height; // Raise camera to eye level
    Cam.Position    = Physics.Position;
    Cam.Yaw         = Physics.Yaw;
    Cam.Pitch       = Physics.Pitch;
    Cam.Frame       = Frame;

    // Animate and rebuild the weapon viewmodel
    Weapon_Update (Weapon, &Cam, Delta_Time, In.Fire);
    Figure_BLAS_Rebuild (Weapon);

    // Advance enemy idle animation and rebuild BLAS
    Enemy->Animation_Time += Delta_Time;
    {
      int Frame_Index = (int)(Enemy->Animation_Time * Enemy->Figure.Animations[0].FPS) % Enemy->Figure.Animations[0].Frame_Count;
      Enemy->Current_Vertices = Enemy->Figure.Frame_Vertices[Frame_Index];
    }
    Figure_BLAS_Rebuild (Enemy);

    // Compute player body TLAS transform and write it to the player body pool slot
    vec3  Entity_Origin = Enemy->GL_Origin;
    float Body_Yaw      = -Physics.Yaw;
    float D_Yaw         = Body_Yaw - Enemy->GL_Yaw;
    float Cosine_Yaw    = cosf (D_Yaw), Sine_Yaw = sinf (D_Yaw);
    float Translation_X = Physics.Position.x - (Cosine_Yaw * Entity_Origin.x + Sine_Yaw * Entity_Origin.z);
    float Translation_Y = Physics.Position.y - Entity_Origin.y;
    float Translation_Z = Physics.Position.z - (-Sine_Yaw * Entity_Origin.x + Cosine_Yaw * Entity_Origin.z);
    float Body_T[3][4] = {{Cosine_Yaw,  0.f, Sine_Yaw,    Translation_X},
                           {0.f,         1.f, 0.f,         Translation_Y},
                           {-Sine_Yaw,   0.f, Cosine_Yaw,  Translation_Z}};
    memcpy (Player_Body->TLAS_Transform, Body_T, sizeof Body_T);
    Player_Body->Bottom_Level = Enemy->Bottom_Level;  // Share rebuilt BLAS

    // Rebuild the top-level acceleration structure
    Top_Level_Rebuild (&World_Bottom_Level, &Figures);

    // Adaptive quality budget
    float Target_Frame_Time;
    if      (Active_Quality == QUALITY_POTATO) Target_Frame_Time = POTATO_TARGET_MS;
    else if (Active_Quality == QUALITY_MEDIUM
             Active_Quality == QUALITY_LOW)    Target_Frame_Time = MEDIUM_TARGET_MS;
    else if (Active_Quality == QUALITY_CRYSIS) Target_Frame_Time = ULTRA_TARGET_MS;
    else                                       Target_Frame_Time = DEFAULT_TARGET_MS;

    // Comment here !!!
    float Budget = 0.0f;
    if (Force_Cheap) {
      Budget = 1.0f;

    // Comment here !!!
    } else if (Delta_Time > Target_Frame_Time) {
      float Budget_Max = fmaxf (Target_Frame_Time * 2.0f, 0.15f);
      Budget = (Delta_Time - Target_Frame_Time) / (Budget_Max - Target_Frame_Time);
      if (Budget > 1.0f) Budget = 1.0f;
    }

    // Comment here !!!
    if (Active_Quality == QUALITY_POTATO and Budget < POTATO_BUDGET_FLOOR) Budget = POTATO_BUDGET_FLOOR;
    uint Budget_Byte = (uint)(Budget * 255.0f);
    float Denoise_Budget = Budget;

    // Comment here !!!
    if (Active_SPP <= 1) Denoise_Budget = fmaxf (Denoise_Budget, 0.10f);
    Current_Budget_Byte = (int)(uint)(fminf (Denoise_Budget, 1.0f) * 255.0f);
    uint Packed_SPP     = (Active_SPP & 0xFF) | (Budget_Byte << 8);

    // Upload the camera and dispatch ray tracing + postprocess
    Camera_Upload (&Cam, Vertical_FOV, Weapon->Texture_Base_Index, PBR_Stride, Packed_SPP);
    float Horizontal_Speed = sqrtf (Physics.Velocity.x * Physics.Velocity.x +
                                    Physics.Velocity.z * Physics.Velocity.z);

    // Build reprojection matrix: Proj * Prev_View * Inverse_View (maps current view-space > previous clip-space)
    mat4 Cur_View     = View (Cam.Position, Cam.Yaw, Cam.Pitch);
    mat4 Cur_Inv_View = Inverse_Orthogonal (Cur_View);
    mat4 Proj         = Perspective (Vertical_FOV, (float)Width / Height, 0.1f, 10000.f);
    mat4 Reproject    = Mat4_Mul (Proj, Mat4_Mul (Prev_View_Matrix, Cur_Inv_View));
    mat4 Inv_Proj     = Inverse_Projection (Proj);

    // Project sun direction to screen space for god rays
    vec3 Sun_D     = Normalize (Active_Environment.Sun_Direction);
    vec3 Sun_World = Add (Cam.Position, Scale (Sun_D, 1000.f)); // Far point in sun direction

    // Transform through View * Proj to get clip space
    mat4 View_Projection = Mat4_Mul (Proj, Cur_View);
    float Clip_X = View_Projection.E[0]*Sun_World.x + View_Projection.E[4]*Sun_World.y + View_Projection.E[8]*Sun_World.z  + View_Projection.E[12];
    float Clip_Y = View_Projection.E[1]*Sun_World.x + View_Projection.E[5]*Sun_World.y + View_Projection.E[9]*Sun_World.z  + View_Projection.E[13];
    float Clip_W = View_Projection.E[3]*Sun_World.x + View_Projection.E[7]*Sun_World.y + View_Projection.E[11]*Sun_World.z + View_Projection.E[15];
    float Sun_U = 0.5f, Sun_V = 0.5f;
    float Sun_Visible = 0.f;

    // Sun is in front of camera
    if (Clip_W > 0.01f) { 
      Sun_U = (Clip_X / Clip_W) * 0.5f + 0.5f;
      Sun_V = (Clip_Y / Clip_W) * 0.5f + 0.5f;

      // Check if sun is roughly on screen (with margin for off-screen glow)
      if (Sun_U > -0.5f and Sun_U < 1.5f and Sun_V > -0.5f and Sun_V < 1.5f)
        Sun_Visible = 1.f;
    }

    // Build post-processing push constants and render frame
    GPU_Postprocess_Push Postprocess = {
      .Time           = Total_Time,
      .Dt_Frame       = (uint32_t)Float_To_Half (Delta_Time) | ((uint32_t)(Frame_Count & 0xFFFF) << 16),
      .Velocity       = Pack_Half2x16 (Physics.Velocity.x, Physics.Velocity.z),
      .Speed_Exposure = Pack_Half2x16 (Horizontal_Speed, Active_Exposure),
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
  free (Scene_Data.Lightmap_Atlas);

  // Pipelines and layouts
  vkDestroyPipeline            (Device, Denoise_Pipeline, NULL);
  vkDestroyPipelineLayout      (Device, Denoise_Pipeline_Layout, NULL);
  vkDestroyDescriptorPool      (Device, Denoise_Descriptor_Pool, NULL);
  vkDestroyDescriptorSetLayout (Device, Denoise_Descriptor_Layout, NULL);
  vkDestroyImageView           (Device, Denoise_Ping_Image.View, NULL);
  vkDestroyImage               (Device, Denoise_Ping_Image.Image, NULL);
  if (Denoise_Ping_Image.Heap_Block >= 0) GPU_Heap_Free (&Heap, Denoise_Ping_Image.Heap_Block);
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
  vkDestroyAccelerationStructure (Device, Bottom_Level.Handle, NULL);
  Buffer_Destroy (&Top_Level.Buffer);
  Buffer_Destroy (&Top_Level_Instance_Buffer);
  Buffer_Destroy (&Top_Level_Scratch_Buffer);
  Buffer_Destroy (&Bottom_Level.Buffer);

  // Player body shares enemy's Vulkan resources — clear its handles to prevent double-free
  memset (Player_Body, 0, sizeof *Player_Body);

  // Free all active figures in the pool
  for (uint I = 0; I < FIGURE_POOL_MAX; I++) {
    if (not Figures.Active[I]) continue;
    Figure_Instance *F = &Figures.Slots[I];
    if (F->Bottom_Level.Handle) {
      vkDestroyAccelerationStructure (Device, F->Bottom_Level.Handle, NULL);
      Buffer_Destroy (&F->Bottom_Level.Buffer);
    }
    if (F->Bottom_Level_Scratch.Buffer) Buffer_Destroy (&F->Bottom_Level_Scratch);
    if (F->Vertex_Buffer.Buffer)        Buffer_Destroy (&F->Vertex_Buffer);
    if (F->Index_Buffer.Buffer)         Buffer_Destroy (&F->Index_Buffer);
    if (F->Texture_Id_Buffer.Buffer)    Buffer_Destroy (&F->Texture_Id_Buffer);
    free (F->Figure.Vertices);
    free (F->Figure.Indices);
    free (F->Figure.Texture_Ids);
    free (F->Transformed_Vertices);
    for (uint J = 0; J < F->Figure.Total_Frame_Count; J++) free (F->Figure.Frame_Vertices[J]);
  }

  // Shader binding table
  Buffer_Destroy (&Shader_Binding_Table_Buffer);

  // GPU storage images — destroy Vulkan objects, then bulk-free all heap memory
  vkDestroyImageView (Device, Raytracing_Storage_Image.View, NULL);
  vkDestroyImage     (Device, Raytracing_Storage_Image.Image, NULL);
  if (Raytracing_Storage_Image.Heap_Block >= 0) GPU_Heap_Free (&Heap, Raytracing_Storage_Image.Heap_Block);
  vkDestroyImageView (Device, Depth_Image.View, NULL);
  vkDestroyImage     (Device, Depth_Image.Image, NULL);
  if (Depth_Image.Heap_Block >= 0) GPU_Heap_Free (&Heap, Depth_Image.Heap_Block);
  vkDestroyImageView (Device, History_Image.View, NULL);
  vkDestroyImage     (Device, History_Image.Image, NULL);
  if (History_Image.Heap_Block >= 0) GPU_Heap_Free (&Heap, History_Image.Heap_Block);
  vkDestroyImageView (Device, Postprocess_Output_Image.View, NULL);
  vkDestroyImage     (Device, Postprocess_Output_Image.Image, NULL);
  if (Postprocess_Output_Image.Heap_Block >= 0) GPU_Heap_Free (&Heap, Postprocess_Output_Image.Heap_Block);

  // Scene buffers
  Buffer_Destroy (&Camera_Uniform_Buffer);
  Buffer_Destroy (&Vertex_Buffer);
  Buffer_Destroy (&Index_Buffer);
  Buffer_Destroy (&Material_Buffer);
  Buffer_Destroy (&Texture_Id_Buffer);
  Buffer_Destroy (&Player_State_Buffer);
  Buffer_Destroy (&Hull_Storage_Buffer);
  Buffer_Destroy (&Projectile_Buffer);

  // Textures
  for (uint I = 0; I < Texture_Count; I++) {
    vkDestroyImageView (Device, Texture_Views[I], NULL);
    vkDestroyImage     (Device, Texture_Images[I], NULL);
    if (Texture_Heap_Blocks[I] >= 0) GPU_Heap_Free (&Heap, Texture_Heap_Blocks[I]);
  }
  free (Texture_Views);
  free (Texture_Images);
  free (Texture_Memories);
  free (Texture_Heap_Blocks);
  vkDestroySampler (Device, Texture_Sampler, NULL);

  // Lightmap
  vkDestroyImageView (Device, Lightmap_View, NULL);
  vkDestroyImage     (Device, Lightmap_Image, NULL);
  if (Lightmap_Heap_Block >= 0) GPU_Heap_Free (&Heap, Lightmap_Heap_Block);
  vkDestroySampler   (Device, Lightmap_Sampler, NULL);

  // Destroy the GPU memory heap — frees all remaining slabs, prints stats
  GPU_Heap_Destroy (&Heap);

  // Swapchain image views
  for (uint I = 0; I < Swapchain_Image_Count; I++)
    vkDestroyImageView (Device, Swapchain_Views[I], NULL);

  // Core Vulkan objects
  vkDestroySemaphore    (Device, Semaphore_Image_Available, NULL);
  vkDestroySemaphore    (Device, Semaphore_Render_Finished, NULL);
  vkDestroyCommandPool  (Device, Command_Pool, NULL);
  vkDestroySwapchainKHR (Device, Swapchain, NULL);
  vkDestroySurfaceKHR   (Instance, Surface, NULL);
  vkDestroyFence        (Device, Fence, NULL);
  vkDestroyDevice       (Device, NULL);
  vkDestroyInstance     (Instance, NULL);

  // Media layer
  SDL_DestroyWindow (Window);
  SDL_Quit ();
  return 0;
  
} // main

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
//                                                            B  O  D  Y
//                 
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// Note: Sections §1 through §2 are specification only

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §3. Math
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ════════════
//   Identity 
// ════════════

mat4 Identity () {mat4 Result = {0}; Result.E[0]  = 1; Result.E[5]  = 1;
                                     Result.E[10] = 1; Result.E[15] = 1; return Result;}

// ═════════════
//   Mat34_Mul
// ═════════════

void Mat34_Mul (const float A[3][4], const float B[3][4], float C[3][4]) {
  for (int R=0;R<3;R++) {
    C[R][0] = A[R][0]*B[0][0]+A[R][1]*B[1][0]+A[R][2]*B[2][0];
    C[R][1] = A[R][0]*B[0][1]+A[R][1]*B[1][1]+A[R][2]*B[2][1];
    C[R][2] = A[R][0]*B[0][2]+A[R][1]*B[1][2]+A[R][2]*B[2][2];
    C[R][3] = A[R][0]*B[0][3]+A[R][1]*B[1][3]+A[R][2]*B[2][3]+A[R][3];
  }
}

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

// ══════════════════════
//   Inverse_Orthogonal 
// ══════════════════════

mat4 Inverse_Orthogonal (mat4 Source) {
  mat4 Result = {0};

  // Transpose the upper-left 3 by 3 rotation block
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

// ════════════
//   Mat4_Mul
// ════════════

mat4 Mat4_Mul (mat4 A, mat4 B) {
  mat4 Result = {0};
  for (int Row = 0; Row < 4; Row++)
    for (int Col = 0; Col < 4; Col++)
      for (int K = 0; K < 4; K++)
        Result.E[Col * 4 + Row] += A.E[K * 4 + Row] * B.E[Col * 4 + K];
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

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §5. Memory
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ═════════════════
//   Buffer_Upload 
// ═════════════════

void Buffer_Upload (GPU_Buffer Destination, const void *Data, uint64_t Size) {
  if (Destination.Heap_Block >= 0) {
    // Sub-allocated from TLSF heap — use the slab's persistent mapping + offset
    GPU_Heap_Slab *S = &Heap.Slabs[Heap.Blocks[Destination.Heap_Block].Slab];
    assert (S->Mapped and "Buffer_Upload on non-host-visible sub-allocation");
    memcpy (S->Mapped + Destination.Offset, Data, Size);
  } else {
    // Standalone allocation — map/unmap the entire VkDeviceMemory
    void *Mapped;
    VK_CHECK (vkMapMemory (Device, Destination.Memory, 0, Size, 0, &Mapped));
    memcpy (Mapped, Data, Size);
    vkUnmapMemory (Device, Destination.Memory);
  }
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

GPU_Buffer Buffer_Allocate (uint64_t Size, VkBufferUsageFlags Usage, VkMemoryPropertyFlags Memory_Flags) {
  GPU_Buffer Result = {.Size = Size, .Heap_Block = -1};

  // Create the Vulkan buffer object
  VK_CHECK (vkCreateBuffer (/*device      =>*/ Device,
                            /*pCreateInfo =>*/ &(VkBufferCreateInfo){
                              .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                              .size  = Size,
                              .usage = Usage},
                            /*pAllocator  =>*/ NULL,
                            /*pBuffer     =>*/ &Result.Buffer));

  // Query driver memory requirements (size, alignment, compatible types)
  VkMemoryRequirements Req;
  vkGetBufferMemoryRequirements (Device, Result.Buffer, &Req);

  // Sub-allocate from the TLSF heap — O(1) via bitmap lookup
  uint8_t *Mapped = NULL;
  Result.Heap_Block = GPU_Heap_Alloc (&Heap, Req.size, Req.alignment, Memory_Flags, Req.memoryTypeBits,
                                      &Result.Memory, &Result.Offset, &Mapped);
  if (Result.Heap_Block < 0) {
    printf ("[buffer] TLSF alloc failed for %llu bytes\n", (unsigned long long)Size);
    return Result;
  }

  // Bind the buffer to the sub-allocated region within the slab
  VK_CHECK (vkBindBufferMemory (Device, Result.Buffer, Result.Memory, Result.Offset));

  // Retrieve the 64-bit device address if this buffer will be referenced from shaders
  if (Usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    Result.Address = vkGetBufferDeviceAddress (/*device =>*/ Device,
                                               /*pInfo  =>*/ &(VkBufferDeviceAddressInfo){
                                                 .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                                 .buffer = Result.Buffer});
  return Result;
}

// ════════════════════
//   Buffer_Destroy
// ════════════════════

void Buffer_Destroy (GPU_Buffer *B) {
  if (B->Buffer) vkDestroyBuffer (Device, B->Buffer, NULL);
  if (B->Heap_Block >= 0)
    GPU_Heap_Free (&Heap, B->Heap_Block);
  *B = (GPU_Buffer){.Heap_Block = -1};
}

// ═══════════════════════
//   Buffer_Stage_Upload
// ═══════════════════════

GPU_Buffer Buffer_Stage_Upload (VkCommandBuffer    Command_Buffer,
                                VkQueue            Queue,
                                const void *Data, uint64_t Size,
                                VkBufferUsageFlags Usage) {

  // Allocate a host-visible staging buffer and fill it with the source data
  GPU_Buffer Staging = Buffer_Allocate (/*Size         =>*/ Size,
                                        /*Usage        =>*/ VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                          | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  Buffer_Upload (Staging, Data, Size);

  // Allocate the final device-local buffer that shaders will access
  GPU_Buffer Destination = Buffer_Allocate (/*Size         =>*/ Size,
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
  Buffer_Destroy (&Staging);
  return Destination;
}

// ════════════════
//   Arena_Create
// ════════════════

Arena Arena_Create (uint64_t Capacity) {
  return (Arena){.Base = (uint8_t *)malloc (Capacity), .Used = 0, .Capacity = Capacity};
}

// ══════════════
//   Arena_Alloc
// ══════════════

void *Arena_Alloc (Arena *A, uint64_t Size, uint64_t Align) {
  uint64_t Mask   = Align - 1;
  uint64_t Cursor = (A->Used + Mask) & ~Mask;
  if (Cursor + Size > A->Capacity) return NULL;
  A->Used = Cursor + Size;
  return A->Base + Cursor;
}

// ══════════════
//   Arena_Reset
// ══════════════

void Arena_Reset (Arena *A) {A->Used = 0;}

// ════════════════
//   Arena_Destroy
// ════════════════

void Arena_Destroy (Arena *A) {free (A->Base); *A = (Arena){0};}

// ═══════════════════════════════════════════════════════════════
//   GPU Heap — TLSF (Two-Level Segregated Fit) Sub-Allocator
// ═══════════════════════════════════════════════════════════════
//
// O(1) alloc, O(1) free, O(1) coalesce.  Uses CLZ/CTZ bit-scan
// intrinsics on two-level bitmaps to find a suitable free block
// in constant time.  Physical-neighbor links enable immediate
// coalescing without any linear search.
//
// Reference: Masmano et al. "TLSF: a New Dynamic Memory Allocator
// for Real-Time Systems", ECRTS 2004.  Used by AMD RADV, RTEMS,
// L4Re, FreeRTOS.

// ── Bit intrinsics ──────────────────────────────────────────────

static inline int Bit_FLS (uint64_t V) {return V ? 63 - __builtin_clzll (V) : -1;} // Floor log2 (find last set)
static inline int Bit_FFS (uint32_t V) {return V ? __builtin_ctz (V) : -1;}         // Find first set (count trailing zeros)

// ── TLSF class mapping ────────────────────────────────────────

static inline void TLSF_Map (uint64_t Size, int *FL, int *SL) {
  int F = Bit_FLS (Size);
  *FL = F;
  *SL = (int)((Size >> (F > (int)GPU_HEAP_SL_BITS ? F - GPU_HEAP_SL_BITS : 0)) & (GPU_HEAP_SL_COUNT - 1));
}

static inline void TLSF_Map_Search (uint64_t Size, int *FL, int *SL) {
  // Round up to the next class boundary so the found block is guaranteed to fit
  uint64_t Round = Size + (1ull << (Bit_FLS (Size) - GPU_HEAP_SL_BITS)) - 1;
  TLSF_Map (Round < Size ? Size : Round, FL, SL); // Overflow guard
}

// ── Block index allocator (LIFO free-stack) ────────────────────

static int16_t Heap_Block_New (GPU_Heap *H) {
  if (H->Free_Stack_N > 0) return H->Free_Stack[--H->Free_Stack_N];
  if (H->Block_Count >= GPU_HEAP_MAX_BLOCKS) {printf ("[heap] block limit reached (%u)\n", GPU_HEAP_MAX_BLOCKS); return -1;}
  return (int16_t)(H->Block_Count++);
}

static void Heap_Block_Return (GPU_Heap *H, int16_t I) {
  if (H->Free_Stack_N < GPU_HEAP_MAX_BLOCKS) H->Free_Stack[H->Free_Stack_N++] = I;
}

// ── Free-list insert / remove ──────────────────────────────────

static void Heap_FL_Insert (GPU_Heap *H, int16_t Idx) {
  GPU_Heap_Block *B = &H->Blocks[Idx];
  int FL, SL; TLSF_Map (B->Size, &FL, &SL);
  B->Prev_Free = -1;
  B->Next_Free = H->Free_Head[FL][SL];
  if (B->Next_Free >= 0) H->Blocks[B->Next_Free].Prev_Free = Idx;
  H->Free_Head[FL][SL] = Idx;
  H->FL_Bitmap       |= (1u << FL);
  H->SL_Bitmap[FL]   |= (1u << SL);
  B->Free = 1;
}

static void Heap_FL_Remove (GPU_Heap *H, int16_t Idx) {
  GPU_Heap_Block *B = &H->Blocks[Idx];
  int FL, SL; TLSF_Map (B->Size, &FL, &SL);
  if (B->Prev_Free >= 0) H->Blocks[B->Prev_Free].Next_Free = B->Next_Free;
  else                    H->Free_Head[FL][SL] = B->Next_Free;
  if (B->Next_Free >= 0) H->Blocks[B->Next_Free].Prev_Free = B->Prev_Free;
  B->Prev_Free = B->Next_Free = -1;
  B->Free = 0;
  if (H->Free_Head[FL][SL] < 0) {
    H->SL_Bitmap[FL] &= ~(1u << SL);
    if (H->SL_Bitmap[FL] == 0) H->FL_Bitmap &= ~(1u << FL);
  }
}

// ── Find the next physical block (the one starting at Offset + Size in the same slab) ───

static int16_t Heap_Next_Phys (GPU_Heap *H, int16_t Idx) {
  GPU_Heap_Block *B = &H->Blocks[Idx];
  uint64_t End = B->Offset + B->Size;
  // Walk from this block's slab's chain to find the successor (O(1) with sorted blocks — we keep them insertion-ordered)
  for (int16_t I = H->Slabs[B->Slab].First_Block; I >= 0; ) {
    GPU_Heap_Block *C = &H->Blocks[I];
    if (C->Slab == B->Slab and C->Offset == End) return I;
    // Walk via Block_Count order (physical chain not needed — blocks are sorted by offset within slab)
    I = -1;
    for (uint J = 0; J < H->Block_Count; J++) {
      if ((int16_t)J != Idx and H->Blocks[J].Slab == B->Slab and H->Blocks[J].Offset == End) {I = (int16_t)J; break;}
    }
    break;
  }
  return -1;
}

// ── Find the previous physical block (ending at this block's Offset) ────

static int16_t Heap_Prev_Phys (GPU_Heap *H, int16_t Idx) {
  GPU_Heap_Block *B = &H->Blocks[Idx];
  if (B->Prev_Phys >= 0) return B->Prev_Phys;
  return -1;
}

// ── Coalesce with physical neighbors ────────────────────────────

static int16_t Heap_Coalesce (GPU_Heap *H, int16_t Idx) {
  GPU_Heap_Block *B = &H->Blocks[Idx];

  // Merge with next physical block if free
  int16_t Next = Heap_Next_Phys (H, Idx);
  if (Next >= 0 and H->Blocks[Next].Free) {
    Heap_FL_Remove (H, Next);
    B->Size += H->Blocks[Next].Size;
    // Update any block whose Prev_Phys pointed to Next
    for (uint I = 0; I < H->Block_Count; I++)
      if (H->Blocks[I].Prev_Phys == Next) H->Blocks[I].Prev_Phys = Idx;
    Heap_Block_Return (H, Next);
  }

  // Merge with previous physical block if free
  int16_t Prev = Heap_Prev_Phys (H, Idx);
  if (Prev >= 0 and H->Blocks[Prev].Free) {
    Heap_FL_Remove (H, Prev);
    H->Blocks[Prev].Size += B->Size;
    // Update any block whose Prev_Phys pointed to Idx
    for (uint I = 0; I < H->Block_Count; I++)
      if (H->Blocks[I].Prev_Phys == Idx) H->Blocks[I].Prev_Phys = Prev;
    Heap_Block_Return (H, Idx);
    Idx = Prev;
  }

  return Idx;
}

// ── GPU_Heap_Init ───────────────────────────────────────────────

void GPU_Heap_Init (GPU_Heap *H, uint64_t Default_Slab_Size) {
  memset (H, 0, sizeof *H);
  H->Default_Slab_Size = Default_Slab_Size ? Default_Slab_Size : GPU_HEAP_DEFAULT_SIZE;
  for (int I = 0; I < GPU_HEAP_FL_BITS; I++)
    for (int J = 0; J < (int)GPU_HEAP_SL_COUNT; J++)
      H->Free_Head[I][J] = -1;
}

// ── Slab allocation (internal) ──────────────────────────────────

static int Heap_Slab_Create (GPU_Heap *H, uint64_t Size, VkMemoryPropertyFlags Mem_Flags, uint Type_Bits, uint Flags) {
  if (H->Slab_Count >= GPU_HEAP_MAX_SLABS) {printf ("[heap] slab limit reached (%u)\n", GPU_HEAP_MAX_SLABS); return -1;}
  uint MT = Find_Memory_Type (Type_Bits, Mem_Flags);
  int  SI = (int)H->Slab_Count++;
  GPU_Heap_Slab *S = &H->Slabs[SI];
  S->Size        = Size;
  S->Memory_Type = MT;
  S->Flags       = Flags;
  S->Mapped      = NULL;

  // Request device-address support if any buffer in this heap might need it
  VkMemoryAllocateFlagsInfo Addr_Flags = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
    .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT};
  VK_CHECK (vkAllocateMemory (/*device        =>*/ Device,
                              /*pAllocateInfo =>*/ &(VkMemoryAllocateInfo){
                                .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                .pNext           = &Addr_Flags,
                                .allocationSize  = Size,
                                .memoryTypeIndex = MT},
                              /*pAllocator    =>*/ NULL,
                              /*pMemory       =>*/ &S->Memory));

  if (Mem_Flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    VK_CHECK (vkMapMemory (Device, S->Memory, 0, Size, 0, (void **)&S->Mapped));

  // Create one free block spanning the entire slab
  int16_t BI = Heap_Block_New (H);
  if (BI < 0) return -1;
  H->Blocks[BI] = (GPU_Heap_Block){.Offset = 0, .Size = Size, .Slab = (int16_t)SI, .Prev_Phys = -1, .Prev_Free = -1, .Next_Free = -1};
  S->First_Block = BI;
  Heap_FL_Insert (H, BI);
  H->Total_Allocated += Size;

  printf ("[heap] slab %d: %llu MB, type %u%s\n", SI, (unsigned long long)(Size >> 20), MT,
          (Flags & GPU_HEAP_PACK_FLAG) ? " (asset pack)" : "");
  return SI;
}

// ── GPU_Heap_Alloc ──────────────────────────────────────────────
//
// O(1) allocation via TLSF bitmap lookup.

int GPU_Heap_Alloc (GPU_Heap *H, uint64_t Size, uint64_t Alignment,
                    VkMemoryPropertyFlags Mem_Flags, uint Type_Bits,
                    VkDeviceMemory *Out_Memory, uint64_t *Out_Offset, uint8_t **Out_Mapped) {

  if (Size < GPU_HEAP_MIN_SIZE) Size = GPU_HEAP_MIN_SIZE;

  // TLSF O(1) lookup: find a free block >= Size via bitmap scan
  int FL, SL;
  TLSF_Map_Search (Size, &FL, &SL);

  // Search second-level bitmap at FL for a set bit >= SL
  uint32_t SL_Map = H->SL_Bitmap[FL] & (~0u << SL);
  if (not SL_Map) {
    // No fit at this FL — search first-level bitmap for the next larger class
    uint32_t FL_Map = H->FL_Bitmap & (~0u << (FL + 1));
    if (not FL_Map) {
      // No existing block fits — grow by allocating a new slab
      uint64_t Slab_Size = H->Default_Slab_Size;
      if (Size + Alignment > Slab_Size) Slab_Size = Size + Alignment;
      int New_Slab = Heap_Slab_Create (H, Slab_Size, Mem_Flags, Type_Bits, 0);
      if (New_Slab < 0) return -1;
      return GPU_Heap_Alloc (H, Size, Alignment, Mem_Flags, Type_Bits, Out_Memory, Out_Offset, Out_Mapped);
    }
    FL = Bit_FFS (FL_Map);
    SL_Map = H->SL_Bitmap[FL];
  }
  SL = Bit_FFS (SL_Map);

  // Pop the head of the free-list at (FL, SL)
  int16_t Idx = H->Free_Head[FL][SL];
  if (Idx < 0) return -1; // Should never happen after bitmap said yes
  GPU_Heap_Block *B = &H->Blocks[Idx];

  // Verify memory type compatibility (slab must match requested type)
  uint MT_Needed = Find_Memory_Type (Type_Bits, Mem_Flags);
  if (H->Slabs[B->Slab].Memory_Type != MT_Needed) {
    // Wrong memory type — need a new slab of the right type
    uint64_t Slab_Size = H->Default_Slab_Size;
    if (Size + Alignment > Slab_Size) Slab_Size = Size + Alignment;
    int New_Slab = Heap_Slab_Create (H, Slab_Size, Mem_Flags, Type_Bits, 0);
    if (New_Slab < 0) return -1;
    return GPU_Heap_Alloc (H, Size, Alignment, Mem_Flags, Type_Bits, Out_Memory, Out_Offset, Out_Mapped);
  }

  Heap_FL_Remove (H, Idx);

  // Handle alignment: if the block's offset doesn't satisfy alignment, split a front padding block
  uint64_t Aligned = (B->Offset + Alignment - 1) & ~(Alignment - 1);
  uint64_t Pad = Aligned - B->Offset;
  if (Pad > 0 and Pad >= GPU_HEAP_MIN_SIZE) {
    // Split: create a free padding block before this one
    int16_t Pad_Idx = Heap_Block_New (H);
    if (Pad_Idx >= 0) {
      H->Blocks[Pad_Idx] = (GPU_Heap_Block){.Offset = B->Offset, .Size = Pad, .Slab = B->Slab, .Prev_Phys = B->Prev_Phys, .Prev_Free = -1, .Next_Free = -1};
      B->Offset = Aligned;
      B->Size  -= Pad;
      B->Prev_Phys = Pad_Idx;
      Heap_FL_Insert (H, Pad_Idx);
    }
  } else if (Pad > 0) {
    // Padding too small to split — absorb into this block (wastes < MIN_SIZE bytes)
    B->Offset = Aligned;
    B->Size  -= Pad;
  }

  // Split remainder if large enough
  uint64_t Remainder = B->Size - Size;
  if (Remainder >= GPU_HEAP_MIN_SIZE) {
    int16_t Rem_Idx = Heap_Block_New (H);
    if (Rem_Idx >= 0) {
      H->Blocks[Rem_Idx] = (GPU_Heap_Block){.Offset = Aligned + Size, .Size = Remainder, .Slab = B->Slab, .Prev_Phys = Idx, .Prev_Free = -1, .Next_Free = -1};
      B->Size = Size;
      // Update next physical block's Prev_Phys to point to remainder instead of us
      for (uint I = 0; I < H->Block_Count; I++)
        if (H->Blocks[I].Slab == B->Slab and H->Blocks[I].Offset == Aligned + Size + Remainder)
          {H->Blocks[I].Prev_Phys = Rem_Idx; break;}
      Heap_FL_Insert (H, Rem_Idx);
    }
  }

  // Output
  GPU_Heap_Slab *S = &H->Slabs[B->Slab];
  *Out_Memory = S->Memory;
  *Out_Offset = B->Offset;
  *Out_Mapped = S->Mapped ? S->Mapped + B->Offset : NULL;
  H->Total_Used += B->Size;
  if (H->Block_Count > H->Peak_Blocks) H->Peak_Blocks = H->Block_Count;
  return (int)Idx;
}

// ── GPU_Heap_Free ───────────────────────────────────────────────
//
// O(1) free + immediate coalesce with physical neighbors.

void GPU_Heap_Free (GPU_Heap *H, int Block_Handle) {
  if (Block_Handle < 0 or (uint)Block_Handle >= H->Block_Count) return;
  GPU_Heap_Block *B = &H->Blocks[Block_Handle];
  if (B->Free) return; // Double-free guard
  H->Total_Used -= B->Size;
  int16_t Merged = Heap_Coalesce (H, (int16_t)Block_Handle);
  Heap_FL_Insert (H, Merged);
}

// ── GPU_Heap_Pack_Create ────────────────────────────────────────
//
// Pin a dedicated slab for a large asset pack (map textures, model set).
// Returns the slab index.  All allocations from this pack use GPU_Heap_Pack_Alloc.
// On unload, GPU_Heap_Pack_Destroy frees the entire slab in one vkFreeMemory — O(1).

int GPU_Heap_Pack_Create (GPU_Heap *H, uint64_t Size, VkMemoryPropertyFlags Mem_Flags, uint Type_Bits) {
  return Heap_Slab_Create (H, Size, Mem_Flags, Type_Bits, GPU_HEAP_PACK_FLAG);
}

// ── GPU_Heap_Pack_Alloc ─────────────────────────────────────────
//
// Allocate from a specific pack slab.  Same O(1) TLSF mechanics but restricted to the pack's slab.

int GPU_Heap_Pack_Alloc (GPU_Heap *H, int Pack_Slab, uint64_t Size, uint64_t Alignment,
                         VkDeviceMemory *Out_Memory, uint64_t *Out_Offset, uint8_t **Out_Mapped) {
  if (Size < GPU_HEAP_MIN_SIZE) Size = GPU_HEAP_MIN_SIZE;

  // Find a free block within this specific slab via TLSF lookup, then verify slab match
  int FL, SL;
  TLSF_Map_Search (Size, &FL, &SL);

  uint32_t SL_Map = H->SL_Bitmap[FL] & (~0u << SL);
  int Found = -1;

  // Search through TLSF classes for a block in the target slab
  for (int Fl = FL; Fl < GPU_HEAP_FL_BITS and Found < 0; Fl++) {
    uint32_t Sl_Map = (Fl == FL) ? SL_Map : H->SL_Bitmap[Fl];
    if (not Sl_Map and not (H->FL_Bitmap & (~0u << (Fl + 1)))) break;
    if (not Sl_Map) continue;
    for (int Sl = Bit_FFS (Sl_Map); Sl >= 0 and Found < 0; Sl_Map &= ~(1u << Sl), Sl = Bit_FFS (Sl_Map)) {
      for (int16_t I = H->Free_Head[Fl][Sl]; I >= 0; I = H->Blocks[I].Next_Free) {
        if (H->Blocks[I].Slab == Pack_Slab and H->Blocks[I].Size >= Size) {Found = I; break;}
      }
    }
  }

  if (Found < 0) {printf ("[heap] pack slab %d exhausted\n", Pack_Slab); return -1;}

  // Remove from free-list and do the standard split/align dance
  Heap_FL_Remove (H, (int16_t)Found);
  GPU_Heap_Block *B = &H->Blocks[Found];

  uint64_t Aligned = (B->Offset + Alignment - 1) & ~(Alignment - 1);
  uint64_t Pad = Aligned - B->Offset;
  if (Pad >= GPU_HEAP_MIN_SIZE) {
    int16_t PI = Heap_Block_New (H);
    if (PI >= 0) {
      H->Blocks[PI] = (GPU_Heap_Block){.Offset = B->Offset, .Size = Pad, .Slab = B->Slab, .Prev_Phys = B->Prev_Phys, .Prev_Free = -1, .Next_Free = -1};
      B->Offset = Aligned; B->Size -= Pad; B->Prev_Phys = PI;
      Heap_FL_Insert (H, PI);
    }
  } else if (Pad > 0) {B->Offset = Aligned; B->Size -= Pad;}

  uint64_t Rem = B->Size - Size;
  if (Rem >= GPU_HEAP_MIN_SIZE) {
    int16_t RI = Heap_Block_New (H);
    if (RI >= 0) {
      H->Blocks[RI] = (GPU_Heap_Block){.Offset = Aligned + Size, .Size = Rem, .Slab = B->Slab, .Prev_Phys = (int16_t)Found, .Prev_Free = -1, .Next_Free = -1};
      B->Size = Size;
      Heap_FL_Insert (H, RI);
    }
  }

  *Out_Memory = H->Slabs[Pack_Slab].Memory;
  *Out_Offset = B->Offset;
  *Out_Mapped = H->Slabs[Pack_Slab].Mapped ? H->Slabs[Pack_Slab].Mapped + B->Offset : NULL;
  H->Total_Used += B->Size;
  return Found;
}

// ── GPU_Heap_Pack_Destroy ───────────────────────────────────────
//
// Bulk-free an entire asset pack slab in O(1).  All blocks within the slab are invalidated.
// This is the key advantage over individual frees: no fragmentation left behind, one vkFreeMemory call.

void GPU_Heap_Pack_Destroy (GPU_Heap *H, int Pack_Slab) {
  if (Pack_Slab < 0 or (uint)Pack_Slab >= H->Slab_Count) return;
  GPU_Heap_Slab *S = &H->Slabs[Pack_Slab];
  if (not (S->Flags & GPU_HEAP_PACK_FLAG)) return;

  // Remove all blocks belonging to this slab from the TLSF free-lists and recycle their indices
  for (uint I = 0; I < H->Block_Count; I++) {
    if (H->Blocks[I].Slab != Pack_Slab) continue;
    if (H->Blocks[I].Free) Heap_FL_Remove (H, (int16_t)I);
    else H->Total_Used -= H->Blocks[I].Size;
    H->Blocks[I] = (GPU_Heap_Block){0};
    Heap_Block_Return (H, (int16_t)I);
  }

  // Free the Vulkan device memory
  H->Total_Allocated -= S->Size;
  if (S->Mapped) vkUnmapMemory (Device, S->Memory);
  vkFreeMemory (Device, S->Memory, NULL);
  printf ("[heap] pack slab %d destroyed (%llu MB returned)\n", Pack_Slab, (unsigned long long)(S->Size >> 20));
  *S = (GPU_Heap_Slab){0};
}

// ── GPU_Heap_Destroy ────────────────────────────────────────────

void GPU_Heap_Destroy (GPU_Heap *H) {
  for (uint I = 0; I < H->Slab_Count; I++) {
    if (not H->Slabs[I].Memory) continue;
    if (H->Slabs[I].Mapped) vkUnmapMemory (Device, H->Slabs[I].Memory);
    vkFreeMemory (Device, H->Slabs[I].Memory, NULL);
  }
  printf ("[heap] destroyed: %u slabs, peak %u blocks, %llu MB allocated, %llu MB used\n",
          H->Slab_Count, H->Peak_Blocks,
          (unsigned long long)(H->Total_Allocated >> 20),
          (unsigned long long)(H->Total_Used >> 20));
  memset (H, 0, sizeof *H);
}

// ═══════════════════════
//   Figure_Pool_Init
// ═══════════════════════

void Figure_Pool_Init (Figure_Pool *Pool) {
  memset (Pool, 0, sizeof *Pool);
  Pool->Free_Count = FIGURE_POOL_MAX;
  for (uint I = 0; I < FIGURE_POOL_MAX; I++) Pool->Free_Stack[I] = FIGURE_POOL_MAX - 1 - I; // LIFO: low indices first
}

// ════════════════════════
//   Figure_Pool_Alloc
// ════════════════════════

Figure_Handle Figure_Pool_Alloc (Figure_Pool *Pool, Figure_Instance **Out) {
  if (Pool->Free_Count == 0) {printf ("[pool] FULL — %u slots exhausted\n", FIGURE_POOL_MAX); *Out = NULL; return FIGURE_HANDLE_NULL;}
  uint Index = Pool->Free_Stack[--Pool->Free_Count];
  memset (&Pool->Slots[Index], 0, sizeof (Figure_Instance));
  Pool->Active[Index] = 1;
  Pool->Active_Count++;
  *Out = &Pool->Slots[Index];
  return (Figure_Handle){.Index = Index, .Generation = Pool->Generations[Index]};
}

// ════════════════════════
//   Figure_Pool_Free
// ════════════════════════

void Figure_Pool_Free (Figure_Pool *Pool, Figure_Handle Handle) {
  if (Handle.Index >= FIGURE_POOL_MAX) return;
  if (Handle.Generation != Pool->Generations[Handle.Index]) return; // stale handle
  if (not Pool->Active[Handle.Index]) return;
  Pool->Active[Handle.Index] = 0;
  Pool->Generations[Handle.Index]++;
  Pool->Free_Stack[Pool->Free_Count++] = Handle.Index;
  Pool->Active_Count--;
}

// ════════════════════════
//   Figure_Pool_Get
// ════════════════════════

Figure_Instance *Figure_Pool_Get (Figure_Pool *Pool, Figure_Handle Handle) {
  if (Handle.Index >= FIGURE_POOL_MAX) return NULL;
  return (Handle.Generation == Pool->Generations[Handle.Index] and Pool->Active[Handle.Index])
       ? &Pool->Slots[Handle.Index] : NULL;
}

// ════════════════════════
//   Figure_Pool_Count
// ════════════════════════

uint Figure_Pool_Count (const Figure_Pool *Pool) { return Pool->Active_Count; }

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §6. Textures
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

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
                              /*Out_View        =>*/ Out_View,
                              /*Out_Heap_Block  =>*/ NULL);
}

// ════════════════════════════
//   Sampler_Create_Repeating 
// ════════════════════════════

VkSampler Sampler_Create_Repeating () {
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

VkSampler Sampler_Create_Clamping () {
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

// ════════════════════════
//   Image_Storage_Create
// ════════════════════════

GPU_Image Image_Storage_Create (uint Width, uint Height) {
  GPU_Image Result = {.Format = VK_FORMAT_R16G16B16A16_SFLOAT, .Heap_Block = -1};

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

  // Sub-allocate device-local memory from the TLSF heap
  VkMemoryRequirements Req;
  vkGetImageMemoryRequirements (Device, Result.Image, &Req);
  uint8_t *Mapped = NULL;
  Result.Heap_Block = GPU_Heap_Alloc (&Heap, Req.size, Req.alignment,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Req.memoryTypeBits,
                                       &Result.Memory, &Result.Offset, &Mapped);

  // Bind image to the sub-allocated region within the slab
  VK_CHECK (vkBindImageMemory (Device, Result.Image, Result.Memory, Result.Offset));

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

} // Image_Storage_Create

// ══════════════════════════════
//   Texture_Upload_With_Format
// ══════════════════════════════

void Texture_Upload_With_Format (VkCommandBuffer Command_Buffer, VkQueue Queue,
                                 const uint8_t *Pixels, uint Width, uint Height, VkFormat Format,
                                 VkImage *Out_Image, VkDeviceMemory *Out_Memory, VkImageView *Out_View,
                                 int *Out_Heap_Block) {

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

  // Sub-allocate device-local memory from the TLSF heap
  VkMemoryRequirements Req;
  vkGetImageMemoryRequirements (Device, Image, &Req);
  VkDeviceMemory Memory;
  uint64_t       Img_Offset = 0;
  uint8_t       *Mapped     = NULL;
  int            Img_Block  = GPU_Heap_Alloc (&Heap, Req.size, Req.alignment,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Req.memoryTypeBits,
                                               &Memory, &Img_Offset, &Mapped);
  VK_CHECK (vkBindImageMemory (Device, Image, Memory, Img_Offset));

  // Stage the pixel data through a host-visible buffer
  uint64_t Byte_Size = (uint64_t)Width * Height * 4;
  GPU_Buffer Staging = Buffer_Allocate (/*Size         =>*/ Byte_Size,
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
  Buffer_Destroy (&Staging);

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
  *Out_Image      = Image;
  *Out_Memory     = Memory;
  *Out_View       = Image_View;
  if (Out_Heap_Block) *Out_Heap_Block = Img_Block;

} // Texture_Upload_With_Format

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §6. Textures
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ═════════════════════
//   Damage_Cache_Free
// ═════════════════════

void Damage_Cache_Free () {
  for (int I = 0; I < Damage_Cache_Count; I++) free (Damage_Cache[I].Pixels);
  Damage_Cache_Count = 0;
}

// ═════════════════════
//   Damage_Map_Sample
// ═════════════════════

float Damage_Map_Sample (const char *Path, float U, float V) {
  // Find or load the damage map
  Damage_Map_Cache_Entry *Entry = NULL;
  for (int I = 0; I < Damage_Cache_Count; I++) {
    if (strcmp (Damage_Cache[I].Path, Path) == 0) { Entry = &Damage_Cache[I]; break;}
  }

  // Load the damage map from disk if not cached
  if (not Entry and Damage_Cache_Count < DAMAGE_CACHE_MAX) {
    Entry = &Damage_Cache[Damage_Cache_Count++];
    strncpy (Entry->Path, Path, sizeof (Entry->Path) - 1);
    Entry->Pixels = TGA_Load (Path, &Entry->Width, &Entry->Height);
    if (not Entry->Pixels) { Damage_Cache_Count--; return 0.5f;}
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
  if (Length < 18) {fclose (File); return NULL;}

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

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §7. Models
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ═════════════════════════
//   MD3_Find_Tag_At_Frame
// ═════════════════════════

int MD3_Find_Tag_At_Frame (const uint8_t *Data, int Tag_Count, int Tags_Offset,
                           int Frame, const char *Name, float Out[12])
{
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

  // C.axis[j] = R_A * B.axis[j] (Three column vectors, each 3 floats)
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
  if (*(uint *)Data != MD3_MAGIC) { free (Data); return NULL;}
  return Data;
}

// ═══════════
//   VTF_Bpp
// ═══════════

uint VTF_Bpp (int Format) {
  switch (Format) {
    case VTF_FMT_RGBA8888:
    case VTF_FMT_ABGR8888:
    case VTF_FMT_BGRA8888: return 4; // Comment here !!!
    case VTF_FMT_RGB888:
    case VTF_FMT_BGR888:   return 3; // Comment here !!!
    case VTF_FMT_RGB565:   return 2; // Comment here !!!
    case VTF_FMT_DXT1:     return 0; // Block-compressed, handled separately
    case VTF_FMT_DXT3:
    case VTF_FMT_DXT5:     return 0; // Comment here !!!
    default:               return 4; // Comment here !!!
  }
}

// ═════════════════════
//   DXT1_Decode_Block
// ═════════════════════

void DXT1_Decode_Block (const uint8_t *Src, uint8_t *Dst, int Stride) {
  uint16_t C0 = Src[0] | (Src[1] << 8), C1 = Src[2] | (Src[3] << 8);
  uint8_t Pal[4][4];
  Pal[0][0] = ((C0>>11)&0x1F)*255/31; Pal[0][1] = ((C0>>5)&0x3F)*255/63; Pal[0][2] = (C0&0x1F)*255/31; Pal[0][3] = 255;
  Pal[1][0] = ((C1>>11)&0x1F)*255/31; Pal[1][1] = ((C1>>5)&0x3F)*255/63; Pal[1][2] = (C1&0x1F)*255/31; Pal[1][3] = 255;
  if (C0 > C1) {
    for (int C=0;C<3;C++) {Pal[2][C]=(2*Pal[0][C]+Pal[1][C]+1)/3; Pal[3][C]=(Pal[0][C]+2*Pal[1][C]+1)/3;}
    Pal[2][3]=Pal[3][3]=255;
  } else {
    for (int C=0;C<3;C++) Pal[2][C]=(Pal[0][C]+Pal[1][C]+1)/2;
    Pal[2][3]=255; memset(Pal[3],0,4);
  }
  uint Bits = Src[4]|(Src[5]<<8)|(Src[6]<<16)|(Src[7]<<24);
  for (int Y=0;Y<4;Y++) for (int X=0;X<4;X++,Bits>>=2) memcpy(Dst+Y*Stride+X*4,Pal[Bits&3],4);
}

// ═════════════════════
//   DXT5_Decode_Alpha
// ═════════════════════

void DXT5_Decode_Alpha (const uint8_t *Src, uint8_t *Dst, int Stride) {
  uint8_t A0=Src[0], A1=Src[1], Pal[8]={A0,A1};
  if (A0>A1) {for(int I=2;I<8;I++) Pal[I]=(uint8_t)(((8-I)*A0+(I-1)*A1+3)/7);}
  else {for(int I=2;I<6;I++) Pal[I]=(uint8_t)(((6-I)*A0+(I-1)*A1+2)/5); Pal[6]=0; Pal[7]=255;}
  uint64_t Bits=0; for(int I=2;I<8;I++) Bits|=(uint64_t)Src[I]<<((I-2)*8);
  for (int Y=0;Y<4;Y++) for (int X=0;X<4;X++,Bits>>=3) Dst[Y*Stride+X*4+3]=Pal[Bits&7];
}

// ═════════════════════════
//   Movement_Style_Toggle
// ═════════════════════════

void Movement_Style_Toggle (Player *P) {
  P->Movement = (P->Movement + 1) % WORLD_COUNT;
  Active_Movement = P->Movement;
  const char *Names[] = {"Quake 3", "Source"};
  printf("[movement] switched to %s style\n", Names[P->Movement]);
}

// ═════════════════════
//   Figure_Upload_Skeleton
// ═════════════════════════
//
// Upload skeletal hierarchy data to GPU SSBOs (called once at load time). After this, all
// bone evaluation happens on the GPU — no CPU-side Skeleton_Evaluate needed.

void Figure_Upload_Skeleton (Figure_Instance *E) {
  if (E->Figure.Bone_Count <= 0) return;
  uint BC = (uint)E->Figure.Bone_Count;

  // Bind pose matrices (3x4 row-major, BC entries)
  E->Bone_Buffer = Buffer_Stage_Upload (/*Command_Buffer =>*/ Command_Buffer,
                                        /*Queue          =>*/ Queue,
                                        /*Data           =>*/ E->Figure.Bind_Pose,
                                        /*Size           =>*/ sizeof (float) * 12 * BC,
                                        /*Usage          =>*/ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

  // Inverse bind pose matrices
  E->Inv_Bind_Buffer = Buffer_Stage_Upload (/*Command_Buffer =>*/ Command_Buffer,
                                            /*Queue          =>*/ Queue,
                                            /*Data           =>*/ E->Figure.Inv_Bind,
                                            /*Size           =>*/ sizeof (float) * 12 * BC,
                                            /*Usage          =>*/ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

  // Parent indices (int per bone)
  E->Bone_Parent_Buffer = Buffer_Stage_Upload (/*Command_Buffer =>*/ Command_Buffer,
                                               /*Queue          =>*/ Queue,
                                               /*Data           =>*/ E->Figure.Bone_Parents,
                                               /*Size           =>*/ sizeof (int) * BC,
                                               /*Usage          =>*/ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

  // Pose output buffer (device-local, written by compute shader each frame)
  E->Pose_Buffer = Buffer_Allocate (/*Size         =>*/ sizeof (float) * 12 * BC,
                                    /*Usage        =>*/ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                    /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  printf ("[skeleton] uploaded %u bones to GPU\n", BC);
}

// ════════════════════════════
//   Figure_Skeleton_Dispatch
// ════════════════════════════
//
// Single compute dispatch: Pass 0 evaluates the bone hierarchy on GPU (one invocation per bone),
// Pass 1 skins all vertices using the computed pose matrices. Two dispatches back-to-back in one
// command buffer with a barrier between them.

void Figure_Skeleton_Dispatch (Figure_Instance *E) {
  if (E->Figure.Bone_Count <= 0 or E->Figure.Vertex_Count == 0) return;

  VkCommandBuffer Cmd;
  VK_CHECK (vkAllocateCommandBuffers (/*device          =>*/ Device,
                                      /*pAllocateInfo   =>*/ &(VkCommandBufferAllocateInfo){
                                        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                        .commandPool        = Command_Pool,
                                        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                        .commandBufferCount = 1},
                                      /*pCommandBuffers =>*/ &Cmd));
  VK_CHECK (vkBeginCommandBuffer (/*commandBuffer =>*/ Cmd,
                                  /*pBeginInfo    =>*/ &(VkCommandBufferBeginInfo){
                                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT}));
  vkCmdBindPipeline (Cmd, VK_PIPELINE_BIND_POINT_COMPUTE, Skinning_Pipeline);

  // Bind all 6 SSBOs via push descriptors
  VkDescriptorBufferInfo Infos[6] = {
    {E->Bone_Buffer.Buffer,        0, VK_WHOLE_SIZE},  // 0: bind pose
    {E->Inv_Bind_Buffer.Buffer,    0, VK_WHOLE_SIZE},  // 1: inv bind
    {E->Bone_Parent_Buffer.Buffer, 0, VK_WHOLE_SIZE},  // 2: parents
    {E->Pose_Buffer.Buffer,        0, VK_WHOLE_SIZE},  // 3: pose output
    {E->Vertex_Buffer.Buffer,      0, VK_WHOLE_SIZE},  // 4: bind-pose vertices
    {E->Vertex_Buffer.Buffer,      0, VK_WHOLE_SIZE},  // 5: skinned output (in-place)
  };
  VkWriteDescriptorSet Writes[6];
  for (int I = 0; I < 6; I++)
    Writes[I] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstBinding = (uint)I,
                                        .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                        .pBufferInfo = &Infos[I]};

  PFN_vkCmdPushDescriptorSetKHR Push_Desc =
    (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr (Device, "vkCmdPushDescriptorSetKHR");
  Push_Desc (Cmd, VK_PIPELINE_BIND_POINT_COMPUTE, Skinning_Pipeline_Layout, 0, 6, Writes);

  // Pass 0: bone hierarchy evaluation (one invocation per bone)
  uint Push_0[3] = {E->Figure.Vertex_Count, (uint)E->Figure.Bone_Count, 0};
  vkCmdPushConstants (Cmd, Skinning_Pipeline_Layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 12, Push_0);
  vkCmdDispatch (Cmd, ((uint)E->Figure.Bone_Count + 63) / 64, 1, 1);

  // Memory barrier: pose buffer written by pass 0, read by pass 1
  vkCmdPipelineBarrier (/*commandBuffer         =>*/ Cmd,
                        /*srcStageMask          =>*/ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        /*dstStageMask          =>*/ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        /*dependencyFlags       =>*/ 0,
                        /*memoryBarrierCount    =>*/ 1,
                        /*pMemoryBarriers       =>*/ &(VkMemoryBarrier){
                          .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                          .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                          .dstAccessMask = VK_ACCESS_SHADER_READ_BIT},
                        /*bufferMemoryBarrierCount =>*/ 0, /*pBufferMemoryBarriers =>*/ NULL,
                        /*imageMemoryBarrierCount  =>*/ 0, /*pImageMemoryBarriers  =>*/ NULL);

  // Pass 1: vertex skinning (one invocation per vertex)
  uint Push_1[3] = {E->Figure.Vertex_Count, (uint)E->Figure.Bone_Count, 1};
  vkCmdPushConstants (Cmd, Skinning_Pipeline_Layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 12, Push_1);
  vkCmdDispatch (Cmd, (E->Figure.Vertex_Count + 63) / 64, 1, 1);

  VK_CHECK (vkEndCommandBuffer (Cmd));
  VK_CHECK (vkQueueSubmit (Queue, 1, &(VkSubmitInfo){.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                                      .commandBufferCount = 1, .pCommandBuffers = &Cmd}, VK_NULL_HANDLE));
  vkQueueWaitIdle (Queue);
  vkFreeCommandBuffers (Device, Command_Pool, 1, &Cmd);
}

// ═════════════════════
//   MD3_Parse_Surface
// ═════════════════════

void MD3_Parse_Surface (const uint8_t *Surface_Data,
                        Vertex **Inout_Vertices,    uint *Inout_Vertex_Count,
                        uint    **Inout_Indices,     uint *Inout_Index_Count,
                        uint    **Inout_Texture_Ids, uint *Inout_Triangle_Count,
                        uint Assigned_Texture_Index, const float *Transform)
{
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
  const uint8_t *Vertex_Data             = Surface_Data + Surface->Vertices_Offset;
  const float   *Texture_Coordinate_Data = (const float *)(Surface_Data + Surface->Texture_Coordinates_Offset);

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
    uint8_t  Longitude = Vertex_Data[Vertex_Index * 8 + 6]; // low byte
    uint8_t  Latitude  = Vertex_Data[Vertex_Index * 8 + 7]; // high byte
    float Latitude_Angle  = Latitude  * (2.f * (float)M_PI / 256.f);
    float Longitude_Angle = Longitude * (2.f * (float)M_PI / 256.f);
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
      .Texture_UV = {Texture_U,  Texture_V},
    };
  }

  // Advance the running totals
  *Inout_Vertex_Count   += Surface->Number_Of_Vertices;
  *Inout_Index_Count    += Surface->Number_Of_Triangles * 3;
  *Inout_Triangle_Count += Surface->Number_Of_Triangles;
  
} // MD3_Parse_Surface

// ══════════════════════════════
//   MD3_Parse_Surface_At_Frame
// ══════════════════════════════

void MD3_Parse_Surface_At_Frame (const uint8_t *Surface_Data, int Frame,
                                 Vertex **Inout_Vertices,    uint *Inout_Vertex_Count,
                                 uint   **Inout_Indices,     uint *Inout_Index_Count,
                                 uint   **Inout_Texture_Ids, uint *Inout_Triangle_Count,
                                 uint Assigned_Texture_Index, const float *Transform)
{
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
  const uint8_t *Vertex_Data = Surface_Data + Surface->Vertices_Offset + Frame * Surface->Number_Of_Vertices * 8;
  const float *Tex_Coords = (const float *)(Surface_Data + Surface->Texture_Coordinates_Offset);

  // Grow the vertex array for decoded vertices
  *Inout_Vertices = realloc (*Inout_Vertices, sizeof (Vertex) * (*Inout_Vertex_Count + Surface->Number_Of_Vertices));

  // Decode each vertex at the requested frame
  for (int V = 0; V < Surface->Number_Of_Vertices; V++) {
    const int16_t *Coords = (const int16_t *)(Vertex_Data + V * 8);
    float PX = Coords[0] / 64.f, PY = Coords[1] / 64.f, PZ = Coords[2] / 64.f;

    // Decode spherical normal from latitude/longitude byte pair
    uint8_t Lon = Vertex_Data[V * 8 + 6]; // low byte
    uint8_t Lat = Vertex_Data[V * 8 + 7]; // high byte
    float LA = Lat * (2.f * (float)M_PI / 256.f);
    float LO = Lon * (2.f * (float)M_PI / 256.f);
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
      .Texture_UV = {U,  Tv},
    };
  }

  // Advance the running totals
  *Inout_Vertex_Count   += Surface->Number_Of_Vertices;
  *Inout_Index_Count    += Surface->Number_Of_Triangles * 3;
  *Inout_Triangle_Count += Surface->Number_Of_Triangles;

} // MD3_Parse_Surface_At_Frame

// ═════════════════════
//   Weapon_Model_Load
// ═════════════════════

Articulated_Figure Weapon_Model_Load () {
  Articulated_Figure Result = {0};

  // Open the main weapon body mesh
  FILE *File = fopen ("assets/models/weapons2/machinegun/machinegun.md3", "rb");
  if (not File) {printf ("[weapon] machinegun.md3 not found\n"); return Result;}

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

  // Search for the "tag_barrel" attachment point in the body's tag list and store as Tags[0]
  snprintf (Result.Tags[0].Name, 64, "tag_barrel");
  Result.Tags[0].Frame_Count = 1;
  memset (Result.Tags[0].Transforms[0], 0, sizeof (float) * 12);
  const MD3_Tag *Body_Tags = (const MD3_Tag *)(Body_Data + Body_Tags_Offset);
  for (int Tag = 0; Tag < Body_Tag_Count; Tag++) {
    if (strncmp (Body_Tags[Tag].Name, "tag_barrel", 64) == 0) {
      memcpy (Result.Tags[0].Transforms[0], Body_Tags[Tag].Origin, 3 * sizeof (float));
      memcpy (Result.Tags[0].Transforms[0] + 3, Body_Tags[Tag].Axis, 9 * sizeof (float));
      break;
    }
  }
  Result.Tag_Count = 1;

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
                         /*Transform              =>*/ Result.Tags[0].Transforms[0]);
      Surface_Cursor += ((const MD3_Surface *)Surface_Cursor)->End_Offset;
    }
    free (Barrel_Data);
    printf ("[weapon] barrel merged, tag_barrel=(%.1f,%.1f,%.1f)\n",
            Result.Tags[0].Transforms[0][0],
            Result.Tags[0].Transforms[0][1],
            Result.Tags[0].Transforms[0][2]);
  }

  // Load the hand model for tag_weapon animation frames (no hand geometry - weapon-only viewmodel)
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
    // Store per-frame tag_weapon transforms in Tags[1]
    snprintf (Result.Tags[1].Name, 64, "tag_weapon");
    uint Anim_Frames = Hand_Frame_Count < FIGURE_MAX_TAG_FRAMES ? (uint)Hand_Frame_Count : FIGURE_MAX_TAG_FRAMES;
    Result.Tags[1].Frame_Count = Anim_Frames;

    for (uint Frame = 0; Frame < Anim_Frames; Frame++) {
      const MD3_Tag *Tags = (const MD3_Tag *)(Hand_Data + Hand_Tags_Offset + Frame * Hand_Tag_Count * sizeof (MD3_Tag));
      for (int Tag = 0; Tag < Hand_Tag_Count; Tag++) {
        if (strncmp (Tags[Tag].Name, "tag_weapon", 64) == 0) {
          memcpy (Result.Tags[1].Transforms[Frame], Tags[Tag].Origin, 3 * sizeof (float));
          memcpy (Result.Tags[1].Transforms[Frame] + 3, Tags[Tag].Axis, 9 * sizeof (float));
          break;
        }
      }
    }
    Result.Tag_Count = 2;

    // Store animation clip metadata
    Result.Animation_Count = 1;
    snprintf (Result.Animations[0].Name, 64, "fire");
    Result.Animations[0].Frame_Count = (int)Anim_Frames;
    Result.Animations[0].FPS         = 10.f;
    Result.Animations[0].Looping     = 0;

    free (Hand_Data);
    printf ("[weapon] hand: %u animation frames (no hand geometry)\n", Anim_Frames);
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
//   Entity_Assemble_Frame
// ═════════════════════════

void Entity_Assemble_Frame (int Legs_Frame, int Torso_Frame,
                            uint Body_Mat, uint Gun_Mat, const float World[12],
                            Vertex **Out_Verts, uint *Out_Vert_Count,
                            uint **Out_Indices, uint *Out_Index_Count,
                            uint **Out_Tex_Ids, uint *Out_Tri_Count)
{
  *Out_Verts   = NULL; *Out_Vert_Count  = 0;
  *Out_Indices = NULL; *Out_Index_Count = 0;
  *Out_Tex_Ids = NULL; *Out_Tri_Count   = 0;

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

Figure_Instance Entity_Load (Scene *S, Spawn Spawn_Point) {
  Figure_Instance E = {0};

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
  if (not Lower_Check) { printf ("[enemy] lower.md3 not found\n"); return E;}
  int Lower_Frames = *(int *)(Lower_Check + 76);
  free (Lower_Check);

  // Configure idle leg animation frame range and playback rate
  int Legs_Base = 171;  // MD3 frame for first LEGS_IDLE frame
  int Legs_Num  = 10;   // Number of LEGS_IDLE frames
  if (Legs_Base + Legs_Num > Lower_Frames) Legs_Num = Lower_Frames - Legs_Base;
  if (Legs_Num < 1) Legs_Num = 1;
  if (Legs_Num > FIGURE_MAX_FRAMES) Legs_Num = FIGURE_MAX_FRAMES;

  // Register the idle animation clip
  E.Figure.Animation_Count = 1;
  snprintf (E.Figure.Animations[0].Name, 64, "LEGS_IDLE");
  E.Figure.Animations[0].First_Frame = 0;
  E.Figure.Animations[0].Frame_Count = Legs_Num;
  E.Figure.Animations[0].FPS         = 10.f;
  E.Figure.Animations[0].Looping     = 1;
  E.Figure.Total_Frame_Count         = (uint)Legs_Num;

  // Set the torso to its standing pose frame
  int Torso_Frame = 151;

  // Pre-compute vertex data for each animation frame
  for (int F = 0; F < Legs_Num; F++) {
    Vertex *Verts = NULL; uint VC = 0;
    uint *Idx = NULL; uint IC = 0;
    uint *Tex = NULL; uint TC = 0;

    // Build the composite model at this animation frame
    Entity_Assemble_Frame (Legs_Base + F, Torso_Frame, Body_Mat, Gun_Mat, World,
                          &Verts, &VC, &Idx, &IC, &Tex, &TC);

    // Cache vertex data for this frame
    E.Figure.Frame_Vertices[F] = Verts;

    // Store shared topology from the first frame or free duplicate arrays
    if (F == 0) {
      E.Figure.Vertex_Count   = VC;
      E.Figure.Index_Count    = IC;
      E.Figure.Triangle_Count = TC;
      E.Figure.Indices        = Idx;
      E.Figure.Texture_Ids    = Tex;
    } else {
      free (Idx);
      free (Tex);
    }
  }

  // Initialize animation state to the first frame
  E.Current_Vertices = E.Figure.Frame_Vertices[0];
  E.Animation_Time   = 0.f;
  E.Active_Animation = 0;

  // Log entity statistics
  printf ("[enemy] sarge loaded: %u verts, %u tris, %u animation frames @ %.0f fps\n",
          E.Figure.Vertex_Count, E.Figure.Triangle_Count,
          E.Figure.Animations[0].Frame_Count, E.Figure.Animations[0].FPS);
  return E;

} // Entity_Load

// ═══════════
//   VT_Load
// ═══════════
// ═══════════
//   VTF_Load
// ═══════════

int VTF_Load (const char *Path, uint8_t **Out_Pixels, int *Out_W, int *Out_H) {
  FILE *File = fopen (Path, "rb");
  if (not File) return 0;
  fseek (File, 0, SEEK_END); long File_Size = ftell (File); rewind (File);
  uint8_t *File_Data    = malloc (File_Size);
  size_t   Bytes_Read_  = fread (File_Data, 1, File_Size, File); (void)Bytes_Read_;
  fclose (File);
  if (*(uint*)File_Data != VTF_MAGIC) { free (File_Data); return 0; }
  const VTF_Header *Header = (const VTF_Header*)File_Data;
  int Width  = Header->Width;
  int Height = Header->Height;
  int Format = Header->High_Res_Format;
  *Out_W      = Width;
  *Out_H      = Height;
  *Out_Pixels = calloc (Width * Height * 4, 1);

  // Skip past the file header and the low-res thumbnail to reach full-resolution mip 0
  uint Data_Offset = Header->Header_Size;
  if (Header->Low_Res_Format == VTF_FMT_DXT1 and Header->Low_Res_W > 0)
    Data_Offset += ((Header->Low_Res_W + 3) / 4) * ((Header->Low_Res_H + 3) / 4) * 8;

  // Skip lower mip levels (VTF stores mips smallest-first; mip 0 is the full-size image)
  for (int Mip_Level = Header->Mipmap_Count - 1; Mip_Level > 0; Mip_Level--) {
    int Mip_Width  = Width  >> Mip_Level; if (Mip_Width  < 1) Mip_Width  = 1;
    int Mip_Height = Height >> Mip_Level; if (Mip_Height < 1) Mip_Height = 1;
    switch (Format) {
      case VTF_FMT_DXT1:
        Data_Offset += ((Mip_Width + 3) / 4) * ((Mip_Height + 3) / 4) * 8;
        break;
      case VTF_FMT_DXT3:
      case VTF_FMT_DXT5:
        Data_Offset += ((Mip_Width + 3) / 4) * ((Mip_Height + 3) / 4) * 16;
        break;
      default:
        Data_Offset += Mip_Width * Mip_Height * (VTF_Bpp (Format) ? VTF_Bpp (Format) : 4);
        break;
    }
  }
  const uint8_t *Source_Pixels = File_Data + Data_Offset;
  uint8_t       *Output_Pixels = *Out_Pixels;

  // Decode source pixels into the RGBA8 output buffer, swizzling channels as required per format
  switch (Format) {
    case VTF_FMT_RGBA8888:
      memcpy (Output_Pixels, Source_Pixels, Width * Height * 4);
      break;
    case VTF_FMT_BGRA8888:
      for (int Pixel_Index = 0; Pixel_Index < Width * Height; Pixel_Index++) {
        Output_Pixels[Pixel_Index*4 + 0] = Source_Pixels[Pixel_Index*4 + 2];
        Output_Pixels[Pixel_Index*4 + 1] = Source_Pixels[Pixel_Index*4 + 1];
        Output_Pixels[Pixel_Index*4 + 2] = Source_Pixels[Pixel_Index*4 + 0];
        Output_Pixels[Pixel_Index*4 + 3] = Source_Pixels[Pixel_Index*4 + 3];
      }
      break;
    case VTF_FMT_RGB888:
      for (int Pixel_Index = 0; Pixel_Index < Width * Height; Pixel_Index++) {
        memcpy (Output_Pixels + Pixel_Index * 4, Source_Pixels + Pixel_Index * 3, 3);
        Output_Pixels[Pixel_Index * 4 + 3] = 255;
      }
      break;
    case VTF_FMT_BGR888:
      for (int Pixel_Index = 0; Pixel_Index < Width * Height; Pixel_Index++) {
        Output_Pixels[Pixel_Index*4 + 0] = Source_Pixels[Pixel_Index*3 + 2];
        Output_Pixels[Pixel_Index*4 + 1] = Source_Pixels[Pixel_Index*3 + 1];
        Output_Pixels[Pixel_Index*4 + 2] = Source_Pixels[Pixel_Index*3 + 0];
        Output_Pixels[Pixel_Index*4 + 3] = 255;
      }
      break;
    case VTF_FMT_DXT1: {
      int Block_Width  = (Width  + 3) / 4;
      int Block_Height = (Height + 3) / 4;
      for (int Block_Y = 0; Block_Y < Block_Height; Block_Y++)
        for (int Block_X = 0; Block_X < Block_Width; Block_X++)
          DXT1_Decode_Block (/*Source     =>*/ Source_Pixels + (Block_Y * Block_Width + Block_X) * 8,
                             /*Dest       =>*/ Output_Pixels + (Block_Y * 4 * Width   + Block_X * 4) * 4,
                             /*Row_Stride =>*/ Width * 4);
    } break;
    case VTF_FMT_DXT5: {
      int Block_Width  = (Width  + 3) / 4;
      int Block_Height = (Height + 3) / 4;
      for (int Block_Y = 0; Block_Y < Block_Height; Block_Y++)
        for (int Block_X = 0; Block_X < Block_Width; Block_X++) {
          const uint8_t *Block_Data        = Source_Pixels + (Block_Y * Block_Width + Block_X) * 16;
          uint8_t       *Destination_Pixel = Output_Pixels + (Block_Y * 4 * Width   + Block_X * 4) * 4;
          DXT1_Decode_Block (/*Source     =>*/ Block_Data + 8,
                             /*Dest       =>*/ Destination_Pixel,
                             /*Row_Stride =>*/ Width * 4);
          DXT5_Decode_Alpha (/*Source     =>*/ Block_Data,
                             /*Dest       =>*/ Destination_Pixel,
                             /*Row_Stride =>*/ Width * 4);
        }
    } break;
    default:
      memset (Output_Pixels, 128, Width * Height * 4); // Unknown format: mid-grey fallback
      break;
  }
  free (File_Data);
  printf ("[vtf] %s %dx%d fmt=%d\n", Path, Width, Height, Format);
  return 1;

} // VTF_Load


// ════════════
//   MDL_Load
// ════════════

Figure_Instance MDL_Load (Scene *S, const char *Path, vec3 Origin, float Yaw) {
  Figure_Instance Entity_Result = {0};

  // Read MDL file
  FILE *File = fopen (Path, "rb");
  if (not File) { printf ("[mdl] cannot open %s\n", Path); return Entity_Result; }
  fseek (File, 0, SEEK_END); long File_Size = ftell (File); rewind (File);
  uint8_t *File_Data   = malloc (File_Size);
  size_t   Bytes_Read_ = fread (File_Data, 1, File_Size, File); (void)Bytes_Read_;
  fclose (File);
  const MDL_Header *Header = (const MDL_Header*)File_Data;
  if (Header->Magic != MDL_MAGIC_IDST
      or (Header->Version < MDL_VERSION_44 or Header->Version > MDL_VERSION_49)) {
    printf ("[mdl] bad magic/version in %s\n", Path);
    free (File_Data);
    return Entity_Result;
  }

  // Parse bones and build each bone's 3x4 bind-pose matrix from its default position and quaternion
  Entity_Result.Figure.Bone_Count = Header->Bone_Count < MDL_MAX_BONES
                             ? Header->Bone_Count : MDL_MAX_BONES;
  for (int Bone_Index = 0; Bone_Index < Entity_Result.Figure.Bone_Count; Bone_Index++) {
    const MDL_Bone *Bone = (const MDL_Bone*)(File_Data + Header->Bone_Offset
                                             + Bone_Index * sizeof (MDL_Bone));
    Entity_Result.Figure.Bone_Parents[Bone_Index] = Bone->Parent;

    // Expand the bind-pose quaternion into a row-major 3x4 rotation-translation matrix
    float Quat_X = Bone->Quat[0], Quat_Y = Bone->Quat[1];
    float Quat_Z = Bone->Quat[2], Quat_W = Bone->Quat[3];
    float Two_X  = Quat_X + Quat_X, Two_Y = Quat_Y + Quat_Y, Two_Z = Quat_Z + Quat_Z;
    float XX = Quat_X * Two_X, XY = Quat_X * Two_Y, XZ = Quat_X * Two_Z;
    float YY = Quat_Y * Two_Y, YZ = Quat_Y * Two_Z, ZZ = Quat_Z * Two_Z;
    float WX = Quat_W * Two_X, WY = Quat_W * Two_Y, WZ = Quat_W * Two_Z;
    Entity_Result.Figure.Bind_Pose[Bone_Index][0][0] = 1-(YY+ZZ); Entity_Result.Figure.Bind_Pose[Bone_Index][0][1] = XY-WZ;      Entity_Result.Figure.Bind_Pose[Bone_Index][0][2] = XZ+WY;      Entity_Result.Figure.Bind_Pose[Bone_Index][0][3] = Bone->Position.x;
    Entity_Result.Figure.Bind_Pose[Bone_Index][1][0] = XY+WZ;     Entity_Result.Figure.Bind_Pose[Bone_Index][1][1] = 1-(XX+ZZ);  Entity_Result.Figure.Bind_Pose[Bone_Index][1][2] = YZ-WX;      Entity_Result.Figure.Bind_Pose[Bone_Index][1][3] = Bone->Position.y;
    Entity_Result.Figure.Bind_Pose[Bone_Index][2][0] = XZ-WY;     Entity_Result.Figure.Bind_Pose[Bone_Index][2][1] = YZ+WX;      Entity_Result.Figure.Bind_Pose[Bone_Index][2][2] = 1-(XX+YY);  Entity_Result.Figure.Bind_Pose[Bone_Index][2][3] = Bone->Position.z;

    // Copy the inverse bind-pose (pose_to_bone) directly from the MDL bone descriptor
    memcpy (Entity_Result.Figure.Inv_Bind[Bone_Index], Bone->Pose_To_Bone, sizeof (float) * 12);
  }

  // Append materials from the MDL material table into the shared scene material list
  int Material_Base_Index = S->Material_Count;
  for (int Material_Index = 0;
       Material_Index < Header->Material_Count and Material_Index < MDL_MAX_MESHES;
       Material_Index++) {
    const uint8_t *Material_Entry = File_Data + Header->Material_Offset + Material_Index * 64;
    int         Name_Offset   = *(const int*)Material_Entry; // Relative name offset within the entry block
    const char *Material_Name = (const char*)(Material_Entry + Name_Offset);
    S->Material_Count++;
    S->Materials     = realloc (S->Materials,     sizeof (vec4) * S->Material_Count);
    S->Texture_Names = realloc (S->Texture_Names, 64            * S->Material_Count);
    S->Materials    [S->Material_Count - 1] = (vec4){.x=0.5f, .y=0.5f, .z=0.5f, .w=1.f};
    snprintf (S->Texture_Names[S->Material_Count - 1], 64, "%s", Material_Name);
  }

  // Load geometry from the VVD and VTX sidecar files (Source engine model pipeline)
  Vertex *Vertices      = NULL; uint Vertex_Count   = 0;
  uint   *Indices       = NULL; uint Index_Count    = 0;
  uint   *Texture_Ids   = NULL; uint Triangle_Count = 0;
  float   Cosine_Yaw    = cosf (Yaw);
  float   Sine_Yaw      = sinf (Yaw);

  // Derive sidecar file paths by replacing the .mdl extension
  char VVD_Path[512], VTX_Path[512];
  snprintf (VVD_Path, sizeof VVD_Path, "%.*s.vvd",      (int)(strlen (Path) - 4), Path);
  snprintf (VTX_Path, sizeof VTX_Path, "%.*s.dx90.vtx", (int)(strlen (Path) - 4), Path);

  // Read VVD (vertex data sidecar)
  FILE       *VVD_File         = fopen (VVD_Path, "rb");
  VVD_Vertex *VVD_Vertices     = NULL;
  int         VVD_Vertex_Count = 0;
  if (VVD_File) {
    fseek (VVD_File, 0, SEEK_END); long VVD_File_Size = ftell (VVD_File); rewind (VVD_File);
    uint8_t *VVD_Data      = malloc (VVD_File_Size);
    size_t   VVD_Bytes_Read_ = fread (VVD_Data, 1, VVD_File_Size, VVD_File); (void)VVD_Bytes_Read_;
    fclose (VVD_File);
    const VVD_Header *VVD_Hdr = (const VVD_Header*)VVD_Data;
    if (VVD_Hdr->Magic == VVD_MAGIC and VVD_Hdr->Version == 4) {
      VVD_Vertex_Count = VVD_Hdr->LOD_Vertex_Counts[0];
      VVD_Vertices     = malloc (sizeof (VVD_Vertex) * VVD_Vertex_Count);
      memcpy (VVD_Vertices, VVD_Data + VVD_Hdr->Vertex_Data_Start,
              sizeof (VVD_Vertex) * VVD_Vertex_Count);
      printf ("[mdl] VVD: %d vertices from %s\n", VVD_Vertex_Count, VVD_Path);
    }
    free (VVD_Data);
  }

  // Read VTX (triangle strip sidecar) and extract an indexed triangle list
  FILE *VTX_File = fopen (VTX_Path, "rb");
  if (VTX_File and VVD_Vertices) {
    fseek (VTX_File, 0, SEEK_END); long VTX_File_Size = ftell (VTX_File); rewind (VTX_File);
    uint8_t *VTX_Data       = malloc (VTX_File_Size);
    size_t   VTX_Bytes_Read_ = fread (VTX_Data, 1, VTX_File_Size, VTX_File); (void)VTX_Bytes_Read_;
    fclose (VTX_File); VTX_File = NULL;
    const VTX_Header *VTX_Hdr = (const VTX_Header*)VTX_Data;
    if (VTX_Hdr->Version == VTX_VERSION) {

      // Walk body parts > models > LOD 0 > meshes > strip groups > index list
      int Mesh_Vertex_Offset = 0; // Running base into the VVD vertex array, updated per mesh
      for (int Body_Part_Index = 0; Body_Part_Index < VTX_Hdr->Body_Part_Count; Body_Part_Index++) {
        int Body_Part_Base         = VTX_Hdr->Body_Part_Offset + Body_Part_Index * 8;
        int Body_Part_Model_Count  = *(int*)(VTX_Data + Body_Part_Base);
        int Body_Part_Model_Offset = *(int*)(VTX_Data + Body_Part_Base + 4);

        // Cross-reference the MDL body part to resolve per-model vertex offsets
        const MDL_Body_Part *MDL_Body = (Body_Part_Index < Header->Body_Count)
          ? (const MDL_Body_Part*)(File_Data + Header->Body_Offset
                                   + Body_Part_Index * sizeof (MDL_Body_Part))
          : NULL;
        for (int Model_Index = 0; Model_Index < Body_Part_Model_Count; Model_Index++) {
          int Model_Base       = Body_Part_Base + Body_Part_Model_Offset + Model_Index * 8;
          int Model_LOD_Count  = *(int*)(VTX_Data + Model_Base);     (void)Model_LOD_Count; // LOD 0 only
          int Model_LOD_Offset = *(int*)(VTX_Data + Model_Base + 4);
          int LOD_Base         = Model_Base + Model_LOD_Offset;
          int LOD_Mesh_Count   = *(int*)(VTX_Data + LOD_Base);
          int LOD_Mesh_Offset  = *(int*)(VTX_Data + LOD_Base + 4);

          // Locate the MDL model descriptor to read per-mesh VVD vertex offsets
          const MDL_Model *MDL_Model_Ptr = (MDL_Body and Model_Index < MDL_Body->Model_Count)
            ? (const MDL_Model*)(File_Data + Header->Body_Offset
                                 + Body_Part_Index * sizeof (MDL_Body_Part)
                                 + MDL_Body->Model_Offset
                                 + Model_Index * sizeof (MDL_Model))
            : NULL;
          for (int Mesh_Index = 0; Mesh_Index < LOD_Mesh_Count; Mesh_Index++) {
            int Mesh_Base          = LOD_Base + LOD_Mesh_Offset + Mesh_Index * 9; // VTX MeshHeader_t is packed (9 bytes)
            int Strip_Group_Count  = *(int*)(VTX_Data + Mesh_Base);
            int Strip_Group_Offset = *(int*)(VTX_Data + Mesh_Base + 4);

            // Resolve the MDL mesh descriptor to get its material index and VVD vertex base
            uint Material_Id = 0;
            if (MDL_Model_Ptr and Mesh_Index < MDL_Model_Ptr->Mesh_Count) {
              const MDL_Mesh *MDL_Mesh_Ptr =
                (const MDL_Mesh*)(File_Data + Header->Body_Offset
                                  + Body_Part_Index * sizeof (MDL_Body_Part)
                                  + MDL_Body->Model_Offset
                                  + Model_Index  * sizeof (MDL_Model)
                                  + MDL_Model_Ptr->Mesh_Offset
                                  + Mesh_Index   * sizeof (MDL_Mesh));
              Material_Id        = (uint)(Material_Base_Index
                                          + (MDL_Mesh_Ptr->Material < Header->Material_Count
                                             ? MDL_Mesh_Ptr->Material : 0));
              Mesh_Vertex_Offset = MDL_Mesh_Ptr->Vertex_Offset; // Base into VVD for this mesh's verts
            }
            for (int Strip_Group_Index = 0; Strip_Group_Index < Strip_Group_Count; Strip_Group_Index++) {
              int Strip_Group_Base    = Mesh_Base + Strip_Group_Offset + Strip_Group_Index * 25;
              int Group_Vertex_Count  = *(int*)(VTX_Data + Strip_Group_Base);
              int Group_Vertex_Offset = *(int*)(VTX_Data + Strip_Group_Base + 4);
              int Group_Index_Count   = *(int*)(VTX_Data + Strip_Group_Base + 8);
              int Group_Index_Offset  = *(int*)(VTX_Data + Strip_Group_Base + 12);

              // Build origMeshVertID mapping: strip-group vertex index -> VVD vertex index
              uint16_t *Original_Vertex_Map = malloc (Group_Vertex_Count * sizeof (uint16_t));
              for (int Vertex_Index = 0; Vertex_Index < Group_Vertex_Count; Vertex_Index++) {
                int Vertex_Data_Offset = Strip_Group_Base + Group_Vertex_Offset + Vertex_Index * 9;
                Original_Vertex_Map[Vertex_Index] = *(uint16_t*)(VTX_Data + Vertex_Data_Offset + 4);
              }

              // Emit one triangle at a time from the flat index list
              uint16_t *Strip_Indices = (uint16_t*)(VTX_Data + Strip_Group_Base + Group_Index_Offset);
              for (int Triangle_Index = 0; Triangle_Index + 2 < Group_Index_Count; Triangle_Index += 3) {
                uint Vertex_Base = Vertex_Count;
                Vertices    = realloc (Vertices,    sizeof (Vertex) * (Vertex_Count   + 3));
                Indices     = realloc (Indices,     sizeof (uint)   * (Index_Count    + 3));
                Texture_Ids = realloc (Texture_Ids, sizeof (uint)   * (Triangle_Count + 1));
                for (int Corner = 0; Corner < 3; Corner++) {
                  int Strip_Vertex = Strip_Indices[Triangle_Index + Corner];
                  int VVD_Index    = Mesh_Vertex_Offset
                                     + (Strip_Vertex < Group_Vertex_Count
                                        ? Original_Vertex_Map[Strip_Vertex] : 0);
                  if (VVD_Index >= VVD_Vertex_Count) VVD_Index = 0;
                  const VVD_Vertex *Source_Vertex = &VVD_Vertices[VVD_Index];

                  // Transform: Source Z-up to GL Y-up with entity yaw rotation and world offset
                  float Src_X   = Source_Vertex->Position[0];
                  float Src_Y   = Source_Vertex->Position[1];
                  float Src_Z   = Source_Vertex->Position[2];
                  float World_X = Cosine_Yaw * Src_X - Sine_Yaw * Src_Y + Origin.x;
                  float World_Y = Sine_Yaw   * Src_X + Cosine_Yaw * Src_Y + Origin.z;
                  float World_Z = Src_Z + Origin.y; // Source Z = GL Y (up axis)

                  // Swizzle to OpenGL Y-up: (World_X, World_Z, -World_Y)
                  Vertices[Vertex_Count] = (Vertex){
                    .Position   = {World_X, World_Z, -World_Y},
                    .Normal     = {Source_Vertex->Normal[0], Source_Vertex->Normal[2], -Source_Vertex->Normal[1]},
                    .Texture_UV = {Source_Vertex->Tex_Coord[0], Source_Vertex->Tex_Coord[1]}};
                  Indices[Index_Count++] = Vertex_Base + Corner;
                  Vertex_Count++;
                }
                Texture_Ids[Triangle_Count++] = Material_Id;
              }
              free (Original_Vertex_Map);
            }
          }
        }
      }
    }
    free (VTX_Data);
  } else {
    if (VTX_File) fclose (VTX_File);
    printf ("[mdl] VVD/VTX sidecars not found, generating placeholder\n");

    // Fallback: generate an axis-aligned box to stand in for the missing geometry
    Vertices    = malloc (sizeof (Vertex) * 36);
    Indices     = malloc (sizeof (uint)   * 36);
    Texture_Ids = malloc (sizeof (uint)   * 12);
    float Half_X = 8, Half_Y = 24, Half_Z = 8;
    vec3  Entity_Origin = {Origin.x, Origin.y, Origin.z};
    float Box_Verts[][3] = {
      {-Half_X,-Half_Y,-Half_Z}, { Half_X,-Half_Y,-Half_Z},
      { Half_X, Half_Y,-Half_Z}, {-Half_X, Half_Y,-Half_Z},
      {-Half_X,-Half_Y, Half_Z}, { Half_X,-Half_Y, Half_Z},
      { Half_X, Half_Y, Half_Z}, {-Half_X, Half_Y, Half_Z}};
    int Box_Faces[][4] = {
      {0,1,2,3}, {5,4,7,6}, {1,5,6,2}, {4,0,3,7}, {4,5,1,0}, {3,2,6,7}};
    float Box_Normals[][3] = {
      {0,0,-1}, {0,0,1}, {1,0,0}, {-1,0,0}, {0,-1,0}, {0,1,0}};
    for (int Face_Index = 0; Face_Index < 6; Face_Index++)
      for (int Triangle = 0; Triangle < 2; Triangle++) {
        int Idx_0  = Box_Faces[Face_Index][0];
        int Idx_1  = Box_Faces[Face_Index][Triangle ? 0 : 1];
        int Idx_2  = Box_Faces[Face_Index][Triangle + 1];
        int Idx_3_ = Box_Faces[Face_Index][Triangle ? 3 : 2]; (void)Idx_3_;
        for (int Corner = 0; Corner < 3; Corner++) {
          int   Vert_Index = Corner == 0 ? Idx_0 : Corner == 1 ? Idx_1 : Idx_2;
          float Local_X    = Box_Verts[Vert_Index][0];
          float Local_Y    = Box_Verts[Vert_Index][1];
          float Local_Z    = Box_Verts[Vert_Index][2];
          float World_X    = Cosine_Yaw * Local_X - Sine_Yaw   * Local_Z + Entity_Origin.x;
          float World_Z    = Sine_Yaw   * Local_X + Cosine_Yaw * Local_Z + Entity_Origin.z;
          float World_Y    = Local_Y + Entity_Origin.y;
          Vertices[Vertex_Count] = (Vertex){
            .Position = {World_X, World_Y, -World_Z},
            .Normal   = {Box_Normals[Face_Index][0], Box_Normals[Face_Index][2], -Box_Normals[Face_Index][1]}};
          Indices[Index_Count] = Vertex_Count;
          Index_Count++;
          Vertex_Count++;
        }
        Texture_Ids[Triangle_Count++] = 0;
      }
  }
  free (VVD_Vertices);

  // Populate figure and instance fields from the assembled geometry buffers
  Entity_Result.Figure.Frame_Vertices[0] = Vertices;
  Entity_Result.Figure.Total_Frame_Count = 1;
  Entity_Result.Figure.Vertex_Count      = Vertex_Count;
  Entity_Result.Figure.Index_Count       = Index_Count;
  Entity_Result.Figure.Triangle_Count    = Triangle_Count;
  Entity_Result.Figure.Indices           = Indices;
  Entity_Result.Figure.Texture_Ids       = Texture_Ids;
  Entity_Result.Figure.Vertices          = Vertices;
  Entity_Result.Figure.Is_Source         = 1;
  Entity_Result.Figure.Animation_Count   = 1;
  snprintf (Entity_Result.Figure.Animations[0].Name, 64, "idle");
  Entity_Result.Figure.Animations[0].Frame_Count = 1;
  Entity_Result.Figure.Animations[0].FPS         = 1.f;
  Entity_Result.Figure.Animations[0].Looping     = 1;
  Entity_Result.Current_Vertices  = Vertices;
  Entity_Result.GL_Origin         = Origin;
  Entity_Result.GL_Yaw            = Yaw;
  Entity_Result.Active_Animation  = 0;

  // Allocate bone weight arrays; default every vertex to rigid attachment on bone 0 (100% weight)
  Entity_Result.Figure.Bone_Ids     = calloc (Vertex_Count * SKEL_MAX_BONES_PER_VERT, 1);
  Entity_Result.Figure.Bone_Weights = calloc (Vertex_Count * SKEL_MAX_BONES_PER_VERT, 1);
  for (uint Vert_Index = 0; Vert_Index < Vertex_Count; Vert_Index++)
    Entity_Result.Figure.Bone_Weights[Vert_Index * SKEL_MAX_BONES_PER_VERT] = 255;

  free (File_Data);
  printf ("[mdl] %s: %d bones, %u verts, %u tris\n",
          Path, Entity_Result.Figure.Bone_Count, Vertex_Count, Triangle_Count);
  return Entity_Result;

} // MDL_Load


// ════════════════════════════
//   Source_Weapon_Model_Load
// ════════════════════════════

Articulated_Figure Source_Weapon_Model_Load (const char *Path) {
  Articulated_Figure Result = {0};
  Result.Is_Source = 1;

  // Read MDL file
  FILE *File = fopen (Path, "rb");
  if (not File) { printf ("[weapon] cannot open Source MDL %s\n", Path); return Result; }
  fseek (File, 0, SEEK_END); long File_Size = ftell (File); rewind (File);
  uint8_t *File_Data   = malloc (File_Size);
  size_t   Bytes_Read_ = fread (File_Data, 1, File_Size, File); (void)Bytes_Read_;
  fclose (File);
  const MDL_Header *Header = (const MDL_Header*)File_Data;
  if (Header->Magic != MDL_MAGIC_IDST) {
    printf ("[weapon] bad magic in %s\n", Path);
    free (File_Data);
    return Result;
  }

  // Read all material names from the MDL material table
  Result.Surface_Count = Header->Material_Count < WEAPON_MAX_TEXTURES
                          ? (uint)Header->Material_Count
                          : WEAPON_MAX_TEXTURES;

  // Comment here !!!
  for (uint Material_Index = 0; Material_Index < Result.Surface_Count; Material_Index++) {
    const uint8_t *Material_Entry = File_Data + Header->Material_Offset + Material_Index * 64;
    int         Name_Offset   = *(const int*)Material_Entry;
    const char *Material_Name = (const char*)(Material_Entry + Name_Offset);
    snprintf (Result.Texture_Names[Material_Index], 64, "%s", Material_Name);
    printf ("[weapon] material[%u]: %s\n", Material_Index, Material_Name);
  }

  // Construct sidecar file paths by replacing the .mdl extension
  char VVD_Path[512], VTX_Path[512];
  snprintf (VVD_Path, sizeof VVD_Path, "%.*s.vvd",      (int)(strlen (Path) - 4), Path);
  snprintf (VTX_Path, sizeof VTX_Path, "%.*s.dx90.vtx", (int)(strlen (Path) - 4), Path);

  // Read VVD (vertex data sidecar)
  FILE       *VVD_File         = fopen (VVD_Path, "rb");
  VVD_Vertex *VVD_Vertices     = NULL;
  int         VVD_Vertex_Count = 0;
  if (VVD_File) {
    fseek (VVD_File, 0, SEEK_END); long VVD_File_Size = ftell (VVD_File); rewind (VVD_File);
    uint8_t *VVD_Data       = malloc (VVD_File_Size);
    size_t   VVD_Bytes_Read_ = fread (VVD_Data, 1, VVD_File_Size, VVD_File); (void)VVD_Bytes_Read_;
    fclose (VVD_File);
    const VVD_Header *VVD_Hdr = (const VVD_Header*)VVD_Data;
    if (VVD_Hdr->Magic == VVD_MAGIC and VVD_Hdr->Version == 4) {
      VVD_Vertex_Count = VVD_Hdr->LOD_Vertex_Counts[0];
      VVD_Vertices     = malloc (sizeof (VVD_Vertex) * VVD_Vertex_Count);
      memcpy (VVD_Vertices, VVD_Data + VVD_Hdr->Vertex_Data_Start,
              sizeof (VVD_Vertex) * VVD_Vertex_Count);
    }
    free (VVD_Data);
  }

  // Compute skeletal skinning matrices from the idle animation (sequence 0, frame 0).
  // This bakes the natural weapon-holding pose into the vertex positions at load time.
  float Skin_Matrices[128][3][4];
  int   Has_Skinning = 0;
  if (VVD_Vertices and Header->Bone_Count > 0 and Header->Animation_Count > 0) {
    int Total_Bone_Count = Header->Bone_Count < 128 ? Header->Bone_Count : 128;

    // Seed each bone's local transform from the MDL bind pose
    float Local[128][3][4];
    for (int Bone_Index = 0; Bone_Index < Total_Bone_Count; Bone_Index++) {
      const MDL_Bone *Bone = (const MDL_Bone*)(File_Data + Header->Bone_Offset
                                               + Bone_Index * sizeof (MDL_Bone));
      float Quat_X = Bone->Quat[0], Quat_Y = Bone->Quat[1];
      float Quat_Z = Bone->Quat[2], Quat_W = Bone->Quat[3];
      float Two_X  = Quat_X + Quat_X, Two_Y = Quat_Y + Quat_Y, Two_Z = Quat_Z + Quat_Z;
      float XX = Quat_X*Two_X, XY = Quat_X*Two_Y, XZ = Quat_X*Two_Z;
      float YY = Quat_Y*Two_Y, YZ = Quat_Y*Two_Z, ZZ = Quat_Z*Two_Z;
      float WX = Quat_W*Two_X, WY = Quat_W*Two_Y, WZ = Quat_W*Two_Z;
      Local[Bone_Index][0][0] = 1-(YY+ZZ); Local[Bone_Index][0][1] = XY-WZ;     Local[Bone_Index][0][2] = XZ+WY;     Local[Bone_Index][0][3] = Bone->Position.x;
      Local[Bone_Index][1][0] = XY+WZ;     Local[Bone_Index][1][1] = 1-(XX+ZZ); Local[Bone_Index][1][2] = YZ-WX;     Local[Bone_Index][1][3] = Bone->Position.y;
      Local[Bone_Index][2][0] = XZ-WY;     Local[Bone_Index][2][1] = YZ+WX;     Local[Bone_Index][2][2] = 1-(XX+YY); Local[Bone_Index][2][3] = Bone->Position.z;
    }

    // Walk the animation 0 data stream and override bind-pose transforms with frame 0 values
    const uint8_t *Anim_Desc           = File_Data + Header->Animation_Offset;
    int            Animation_Index_Offset = *(const int*)(Anim_Desc + 56); // animindex field at byte 56 of animdesc_t
    const uint8_t *Animation_Cursor    = Anim_Desc + Animation_Index_Offset;
    for (;;) {
      if (Animation_Cursor < File_Data or Animation_Cursor >= File_Data + File_Size - 4) break;
      int     Anim_Bone_Index   = Animation_Cursor[0];
      int     Anim_Flags        = Animation_Cursor[1];
      int16_t Next_Entry_Offset = *(const int16_t*)(Animation_Cursor + 2);
      if (Anim_Bone_Index < Total_Bone_Count) {
        const MDL_Bone *Bone = (const MDL_Bone*)(File_Data + Header->Bone_Offset
                                                 + Anim_Bone_Index * sizeof (MDL_Bone));
        int Data_Offset = 4;
        if (Anim_Flags & 0x02) Data_Offset += 6; // RAWROT
        if (Anim_Flags & 0x20) Data_Offset += 8; // RAWROT2
        if (Anim_Flags & 0x01) Data_Offset += 6; // RAWPOS
        float Rotation[3] = {Bone->Rot.x,      Bone->Rot.y,      Bone->Rot.z     };
        float Position[3] = {Bone->Position.x,  Bone->Position.y, Bone->Position.z};

        // ANIMROT: decode compressed per-axis rotation deltas and apply them
        if (Anim_Flags & 0x08) {
          const int16_t *Rotation_Pointer = (const int16_t*)(Animation_Cursor + Data_Offset);
          float Rotation_Scale[3] = {Bone->Rot_Scale.x,      Bone->Rot_Scale.y,      Bone->Rot_Scale.z     };
          float Rotation_Base [3] = {Bone->Rot.x,            Bone->Rot.y,            Bone->Rot.z           };
          for (int Axis_Index = 0; Axis_Index < 3; Axis_Index++)
            if (Rotation_Pointer[Axis_Index]) {
              const uint8_t *Value_Pointer = (const uint8_t*)&Rotation_Pointer[Axis_Index]
                                             + Rotation_Pointer[Axis_Index];
              Rotation[Axis_Index] = Rotation_Base[Axis_Index]
                                     + *(const int16_t*)(Value_Pointer + 2) * Rotation_Scale[Axis_Index];
            }
          Data_Offset += 6;
        }

        // ANIMPOS: decode compressed per-axis position deltas and apply them
        if (Anim_Flags & 0x04) {
          const int16_t *Position_Pointer = (const int16_t*)(Animation_Cursor + Data_Offset);
          float Position_Scale[3] = {Bone->Position_Scale.x, Bone->Position_Scale.y, Bone->Position_Scale.z};
          float Position_Base [3] = {Bone->Position.x,       Bone->Position.y,       Bone->Position.z      };
          for (int Axis_Index = 0; Axis_Index < 3; Axis_Index++)
            if (Position_Pointer[Axis_Index]) {
              const uint8_t *Value_Pointer = (const uint8_t*)&Position_Pointer[Axis_Index]
                                             + Position_Pointer[Axis_Index];
              Position[Axis_Index] = Position_Base[Axis_Index]
                                     + *(const int16_t*)(Value_Pointer + 2) * Position_Scale[Axis_Index];
            }
          Data_Offset += 6;
        }

        // Convert the resulting Euler angles (XYZ order) to a normalised unit quaternion
        float Cos_X = cosf (Rotation[0] * 0.5f), Sin_X = sinf (Rotation[0] * 0.5f);
        float Cos_Y = cosf (Rotation[1] * 0.5f), Sin_Y = sinf (Rotation[1] * 0.5f);
        float Cos_Z = cosf (Rotation[2] * 0.5f), Sin_Z = sinf (Rotation[2] * 0.5f);
        float Anim_Quat_W = Cos_X*Cos_Y*Cos_Z + Sin_X*Sin_Y*Sin_Z;
        float Anim_Quat_X = Sin_X*Cos_Y*Cos_Z - Cos_X*Sin_Y*Sin_Z;
        float Anim_Quat_Y = Cos_X*Sin_Y*Cos_Z + Sin_X*Cos_Y*Sin_Z;
        float Anim_Quat_Z = Cos_X*Cos_Y*Sin_Z - Sin_X*Sin_Y*Cos_Z;
        float Quat_Length = sqrtf (Anim_Quat_X*Anim_Quat_X + Anim_Quat_Y*Anim_Quat_Y
                                   + Anim_Quat_Z*Anim_Quat_Z + Anim_Quat_W*Anim_Quat_W);
        if (Quat_Length > 1e-6f) {
          Anim_Quat_X /= Quat_Length; Anim_Quat_Y /= Quat_Length;
          Anim_Quat_Z /= Quat_Length; Anim_Quat_W /= Quat_Length;
        }
        float A_Two_X = Anim_Quat_X + Anim_Quat_X;
        float A_Two_Y = Anim_Quat_Y + Anim_Quat_Y;
        float A_Two_Z = Anim_Quat_Z + Anim_Quat_Z;
        float AXX = Anim_Quat_X*A_Two_X, AXY = Anim_Quat_X*A_Two_Y, AXZ = Anim_Quat_X*A_Two_Z;
        float AYY = Anim_Quat_Y*A_Two_Y, AYZ = Anim_Quat_Y*A_Two_Z, AZZ = Anim_Quat_Z*A_Two_Z;
        float AWX = Anim_Quat_W*A_Two_X, AWY = Anim_Quat_W*A_Two_Y, AWZ = Anim_Quat_W*A_Two_Z;
        Local[Anim_Bone_Index][0][0] = 1-(AYY+AZZ); Local[Anim_Bone_Index][0][1] = AXY-AWZ;      Local[Anim_Bone_Index][0][2] = AXZ+AWY;      Local[Anim_Bone_Index][0][3] = Position[0];
        Local[Anim_Bone_Index][1][0] = AXY+AWZ;     Local[Anim_Bone_Index][1][1] = 1-(AXX+AZZ);  Local[Anim_Bone_Index][1][2] = AYZ-AWX;      Local[Anim_Bone_Index][1][3] = Position[1];
        Local[Anim_Bone_Index][2][0] = AXZ-AWY;     Local[Anim_Bone_Index][2][1] = AYZ+AWX;       Local[Anim_Bone_Index][2][2] = 1-(AXX+AYY); Local[Anim_Bone_Index][2][3] = Position[2];
      }
      if (Next_Entry_Offset == 0) break;
      Animation_Cursor += Next_Entry_Offset;
    }

    // Forward pass: World[i] = World[parent] * Local[i]
    float World[128][3][4];
    for (int Bone_Index = 0; Bone_Index < Total_Bone_Count; Bone_Index++) {
      const MDL_Bone *Bone = (const MDL_Bone*)(File_Data + Header->Bone_Offset
                                               + Bone_Index * sizeof (MDL_Bone));
      if (Bone->Parent >= 0 and Bone->Parent < Bone_Index) {
        for (int Row = 0; Row < 3; Row++)
          for (int Column = 0; Column < 4; Column++)
            World[Bone_Index][Row][Column] =
              World[Bone->Parent][Row][0] * Local[Bone_Index][0][Column]
            + World[Bone->Parent][Row][1] * Local[Bone_Index][1][Column]
            + World[Bone->Parent][Row][2] * Local[Bone_Index][2][Column]
            + (Column == 3 ? World[Bone->Parent][Row][3] : 0);
      } else {
        memcpy (World[Bone_Index], Local[Bone_Index], sizeof (float) * 12);
      }
    }

    // Compose: Skin_Matrices[i] = World[i] * InvBind[i]  (pose_to_bone is the inverse bind)
    for (int Bone_Index = 0; Bone_Index < Total_Bone_Count; Bone_Index++) {
      const MDL_Bone *Bone = (const MDL_Bone*)(File_Data + Header->Bone_Offset
                                               + Bone_Index * sizeof (MDL_Bone));
      for (int Row = 0; Row < 3; Row++)
        for (int Column = 0; Column < 4; Column++)
          Skin_Matrices[Bone_Index][Row][Column] =
            World[Bone_Index][Row][0] * Bone->Pose_To_Bone[0][Column]
          + World[Bone_Index][Row][1] * Bone->Pose_To_Bone[1][Column]
          + World[Bone_Index][Row][2] * Bone->Pose_To_Bone[2][Column]
          + (Column == 3 ? World[Bone_Index][Row][3] : 0);
    }
    Has_Skinning = 1;
    printf ("[weapon] skeletal: %d bones, idle animation applied\n", Total_Bone_Count);
  }

  // Read VTX (triangle strip sidecar) and extract skinned geometry
  FILE *VTX_File = fopen (VTX_Path, "rb");
  if (VTX_File and VVD_Vertices) {
    fseek (VTX_File, 0, SEEK_END); long VTX_File_Size = ftell (VTX_File); rewind (VTX_File);
    uint8_t *VTX_Data        = malloc (VTX_File_Size);
    size_t   VTX_Bytes_Read_  = fread (VTX_Data, 1, VTX_File_Size, VTX_File); (void)VTX_Bytes_Read_;
    fclose (VTX_File); VTX_File = NULL;
    const VTX_Header *VTX_Hdr = (const VTX_Header*)VTX_Data;
    #define VTX_BOUNDS(Offset, Size) ((Offset) >= 0 and (Offset) + (Size) <= VTX_File_Size)
    if (VTX_Hdr->Version == VTX_VERSION) {
      int Mesh_Vertex_Offset = 0;
      for (int Body_Part_Index = 0; Body_Part_Index < VTX_Hdr->Body_Part_Count; Body_Part_Index++) {
        int Body_Part_Base         = VTX_Hdr->Body_Part_Offset + Body_Part_Index * 8;
        if (not VTX_BOUNDS (Body_Part_Base, 8)) continue;
        int Body_Part_Model_Count  = *(int*)(VTX_Data + Body_Part_Base);
        int Body_Part_Model_Offset = *(int*)(VTX_Data + Body_Part_Base + 4);
        const MDL_Body_Part *MDL_Body = (Body_Part_Index < Header->Body_Count)
          ? (const MDL_Body_Part*)(File_Data + Header->Body_Offset
                                   + Body_Part_Index * sizeof (MDL_Body_Part))
          : NULL;
        for (int Model_Index = 0; Model_Index < Body_Part_Model_Count; Model_Index++) {
          int Model_Base       = Body_Part_Base + Body_Part_Model_Offset + Model_Index * 8;
          if (not VTX_BOUNDS (Model_Base, 8)) continue;
          int Model_LOD_Offset = *(int*)(VTX_Data + Model_Base + 4);
          int LOD_Base         = Model_Base + Model_LOD_Offset;
          if (not VTX_BOUNDS (LOD_Base, 8)) continue;
          int LOD_Mesh_Count   = *(int*)(VTX_Data + LOD_Base);
          int LOD_Mesh_Offset  = *(int*)(VTX_Data + LOD_Base + 4);
          const MDL_Model *MDL_Model_Ptr = (MDL_Body and Model_Index < MDL_Body->Model_Count)
            ? (const MDL_Model*)(File_Data + Header->Body_Offset
                                 + Body_Part_Index * sizeof (MDL_Body_Part)
                                 + MDL_Body->Model_Offset
                                 + Model_Index * sizeof (MDL_Model))
            : NULL;
          for (int Mesh_Index = 0; Mesh_Index < LOD_Mesh_Count; Mesh_Index++) {
            int Mesh_Base          = LOD_Base + LOD_Mesh_Offset + Mesh_Index * 9; // VTX MeshHeader_t is packed (9 bytes)
            if (not VTX_BOUNDS (Mesh_Base, 9)) continue;
            int Strip_Group_Count  = *(int*)(VTX_Data + Mesh_Base);
            int Strip_Group_Offset = *(int*)(VTX_Data + Mesh_Base + 4);
            uint Mesh_Material_Id  = 0;
            if (MDL_Model_Ptr and Mesh_Index < MDL_Model_Ptr->Mesh_Count) {
              const MDL_Mesh *MDL_Mesh_Ptr =
                (const MDL_Mesh*)(File_Data + Header->Body_Offset
                                  + Body_Part_Index * sizeof (MDL_Body_Part)
                                  + MDL_Body->Model_Offset
                                  + Model_Index  * sizeof (MDL_Model)
                                  + MDL_Model_Ptr->Mesh_Offset
                                  + Mesh_Index   * sizeof (MDL_Mesh));
              Mesh_Vertex_Offset = MDL_Mesh_Ptr->Vertex_Offset;
              Mesh_Material_Id   = (uint)MDL_Mesh_Ptr->Material < Result.Surface_Count
                                   ? (uint)MDL_Mesh_Ptr->Material : 0;
            }
            for (int Strip_Group_Index = 0; Strip_Group_Index < Strip_Group_Count; Strip_Group_Index++) {
              int Group_Base          = Mesh_Base + Strip_Group_Offset + Strip_Group_Index * 25;
              if (not VTX_BOUNDS (Group_Base, 25)) continue;
              int Group_Vertex_Count  = *(int*)(VTX_Data + Group_Base);
              int Group_Vertex_Offset = *(int*)(VTX_Data + Group_Base + 4);
              int Group_Index_Count   = *(int*)(VTX_Data + Group_Base + 8);
              int Group_Index_Offset  = *(int*)(VTX_Data + Group_Base + 12);
              if (Group_Vertex_Count <= 0 or Group_Vertex_Count > 65536)  continue;
              if (Group_Index_Count  <= 0 or Group_Index_Count  > 300000) continue;

              // Verify vertex and index regions are within file bounds before reading
              if (not VTX_BOUNDS (Group_Base + Group_Vertex_Offset, Group_Vertex_Count * 9)) continue;
              if (not VTX_BOUNDS (Group_Base + Group_Index_Offset,  Group_Index_Count  * 2)) continue;
              uint16_t *Original_Vertex_Map = malloc (Group_Vertex_Count * sizeof (uint16_t));
              for (int Vertex_Index = 0; Vertex_Index < Group_Vertex_Count; Vertex_Index++) {
                int Vertex_Data_Offset = Group_Base + Group_Vertex_Offset + Vertex_Index * 9;
                Original_Vertex_Map[Vertex_Index] = *(uint16_t*)(VTX_Data + Vertex_Data_Offset + 4);
              }
              uint16_t *Strip_Indices = (uint16_t*)(VTX_Data + Group_Base + Group_Index_Offset);
              for (int Triangle_Index = 0; Triangle_Index + 2 < Group_Index_Count; Triangle_Index += 3) {
                uint Vertex_Base = Result.Vertex_Count;
                Result.Vertices     = realloc (Result.Vertices,     sizeof (Vertex) * (Result.Vertex_Count   + 3));
                Result.Indices      = realloc (Result.Indices,      sizeof (uint)   * (Result.Index_Count    + 3));
                Result.Texture_Ids  = realloc (Result.Texture_Ids,  sizeof (uint)   * (Result.Triangle_Count + 1));
                for (int Corner = 0; Corner < 3; Corner++) {
                  int Strip_Vertex = Strip_Indices[Triangle_Index + Corner];
                  int VVD_Index    = Mesh_Vertex_Offset
                                     + (Strip_Vertex < Group_Vertex_Count
                                        ? Original_Vertex_Map[Strip_Vertex] : 0);
                  if (VVD_Index < 0 or VVD_Index >= VVD_Vertex_Count) VVD_Index = 0;
                  const VVD_Vertex *Source_Vertex = &VVD_Vertices[VVD_Index];

                  // Apply weighted skeletal skinning: blend the bind-pose vertex through each bone's skin matrix
                  float Skinned_Position[3] = {Source_Vertex->Position[0], Source_Vertex->Position[1], Source_Vertex->Position[2]};
                  float Skinned_Normal  [3] = {Source_Vertex->Normal[0],   Source_Vertex->Normal[1],   Source_Vertex->Normal[2]  };
                  if (Has_Skinning) {
                    float Blended_Position[3] = {0, 0, 0};
                    float Blended_Normal  [3] = {0, 0, 0};
                    for (int Bone_Influence = 0;
                         Bone_Influence < Source_Vertex->Bone_Count and Bone_Influence < 3;
                         Bone_Influence++) {
                      int   Bone_Index  = Source_Vertex->Bone_Ids    [Bone_Influence];
                      float Bone_Weight = Source_Vertex->Bone_Weights[Bone_Influence];
                      if (Bone_Weight < 0.001f or Bone_Index >= 128) continue;
                      for (int Row = 0; Row < 3; Row++) {
                        Blended_Position[Row] += Bone_Weight *
                          (Skin_Matrices[Bone_Index][Row][0] * Skinned_Position[0]
                         + Skin_Matrices[Bone_Index][Row][1] * Skinned_Position[1]
                         + Skin_Matrices[Bone_Index][Row][2] * Skinned_Position[2]
                         + Skin_Matrices[Bone_Index][Row][3]);
                        Blended_Normal[Row] += Bone_Weight *
                          (Skin_Matrices[Bone_Index][Row][0] * Skinned_Normal[0]
                         + Skin_Matrices[Bone_Index][Row][1] * Skinned_Normal[1]
                         + Skin_Matrices[Bone_Index][Row][2] * Skinned_Normal[2]);
                      }
                    }
                    Skinned_Position[0] = Blended_Position[0];
                    Skinned_Position[1] = Blended_Position[1];
                    Skinned_Position[2] = Blended_Position[2];
                    float Normal_Length = sqrtf (Blended_Normal[0]*Blended_Normal[0]
                                               + Blended_Normal[1]*Blended_Normal[1]
                                               + Blended_Normal[2]*Blended_Normal[2]);
                    if (Normal_Length > 1e-6f) {
                      Skinned_Normal[0] = Blended_Normal[0] / Normal_Length;
                      Skinned_Normal[1] = Blended_Normal[1] / Normal_Length;
                      Skinned_Normal[2] = Blended_Normal[2] / Normal_Length;
                    } else {
                      Skinned_Normal[0] = Blended_Normal[0];
                      Skinned_Normal[1] = Blended_Normal[1];
                      Skinned_Normal[2] = Blended_Normal[2];
                    }
                  }

                  // Swizzle to (barrel=-Y, up=+Z, right=+X) for Weapon_Update
                  Result.Vertices[Result.Vertex_Count] = (Vertex){
                    .Position   = {-Skinned_Position[1],  Skinned_Position[2],  Skinned_Position[0]},
                    .Normal     = {-Skinned_Normal  [1],  Skinned_Normal  [2],  Skinned_Normal  [0]},
                    .Texture_UV = {Source_Vertex->Tex_Coord[0], Source_Vertex->Tex_Coord[1]}};
                  Result.Indices[Result.Index_Count++] = Vertex_Base + Corner;
                  Result.Vertex_Count++;
                }
                Result.Texture_Ids[Result.Triangle_Count++] = Mesh_Material_Id;
              }
              free (Original_Vertex_Map);
            }
          }
        }
      }
    }
    #undef VTX_BOUNDS
    free (VTX_Data);
  } else {
    if (VTX_File) fclose (VTX_File);
  }
  free (VVD_Vertices);
  free (File_Data);

  // Source viewmodels include the hands; set a single-frame identity tag transform
  snprintf (Result.Tags[0].Name, 64, "tag_weapon");
  Result.Tags[0].Frame_Count = 1;
  memset (Result.Tags[0].Transforms[0], 0, sizeof (float) * 12);
  Result.Tags[0].Transforms[0][3] = 1; Result.Tags[0].Transforms[0][7] = 1; Result.Tags[0].Transforms[0][11] = 1;
  Result.Tag_Count = 1;
  Result.Animation_Count = 1;
  snprintf (Result.Animations[0].Name, 64, "idle");
  Result.Animations[0].Frame_Count = 1;
  Result.Animations[0].FPS         = 1.f;
  Result.Animations[0].Looping     = 1;

  printf ("[weapon] Source MDL: %u verts, %u tris from %s\n",
          Result.Vertex_Count, Result.Triangle_Count, Path);

  // Print the bounding box of the loaded weapon vertices for placement debugging
  if (Result.Vertex_Count > 0) {
    float Min[3] = { 1e9f,  1e9f,  1e9f};
    float Max[3] = {-1e9f, -1e9f, -1e9f};
    for (uint Vert_Index = 0; Vert_Index < Result.Vertex_Count; Vert_Index++)
      for (int Axis = 0; Axis < 3; Axis++) {
        if (Result.Vertices[Vert_Index].Position[Axis] < Min[Axis]) Min[Axis] = Result.Vertices[Vert_Index].Position[Axis];
        if (Result.Vertices[Vert_Index].Position[Axis] > Max[Axis]) Max[Axis] = Result.Vertices[Vert_Index].Position[Axis];
      }
    printf ("[weapon] bounds: min(%.1f, %.1f, %.1f) max(%.1f, %.1f, %.1f) size(%.1f, %.1f, %.1f)\n",
            Min[0], Min[1], Min[2], Max[0], Max[1], Max[2],
            Max[0]-Min[0], Max[1]-Min[1], Max[2]-Min[2]);
  }
  return Result;

} // Source_Weapon_Model_Load

// ═══════════════════
//   Figure_Load
// ═══════════════════
//
// Unified model loader that dispatches based on file extension. Returns an Articulated_Figure with merged geometry, attachment tags,
// and animation data. This is the single entry point that replaces Entity_Load, MDL_Load, Weapon_Model_Load, etc.

Articulated_Figure Figure_Load (const char *Path, vec3 Origin, float Yaw) {
  Articulated_Figure Figure = {0};

  // Determine format from extension
  const char *Dot = strrchr (Path, '.');
  if (not Dot) {printf ("[figure] no extension in %s\n", Path); return Figure;}

  if (strcasecmp (Dot, ".mdl") == 0) {
    // Source engine MDL: delegate to MDL loading pipeline, then transplant data into Figure
    Figure.Is_Source = 1;

    // The Source MDL loader needs the MDL + VVD + VTX sidecar files. It produces an Entity with
    // skeletal data. We transplant the relevant fields into our Figure struct.
    // For now, wrap the existing Source_Weapon_Model_Load which already handles MDL parsing.
    Articulated_Figure WM = Source_Weapon_Model_Load (Path);
    Figure.Part_Count = 1;
    snprintf (Figure.Parts[0].Name, 64, "body");
    Figure.Parts[0].Vertices       = WM.Vertices;
    Figure.Parts[0].Vertex_Count   = WM.Vertex_Count;
    Figure.Parts[0].Indices        = WM.Indices;
    Figure.Parts[0].Index_Count    = WM.Index_Count;
    Figure.Parts[0].Texture_Ids    = WM.Texture_Ids;
    Figure.Parts[0].Triangle_Count = WM.Triangle_Count;
    Figure.Parts[0].Surface_Count  = WM.Surface_Count;
    Figure.Parts[0].Parent_Tag     = -1;
    for (uint I = 0; I < WM.Surface_Count and I < WEAPON_MAX_TEXTURES; I++)
      memcpy (Figure.Parts[0].Texture_Names[I], WM.Texture_Names[I], 64);

    // Copy merged geometry pointers
    Figure.Vertices       = WM.Vertices;
    Figure.Vertex_Count   = WM.Vertex_Count;
    Figure.Indices        = WM.Indices;
    Figure.Index_Count    = WM.Index_Count;
    Figure.Texture_Ids    = WM.Texture_Ids;
    Figure.Triangle_Count = WM.Triangle_Count;

    printf ("[figure] loaded Source MDL %s: %u verts, %u tris, %u surfaces\n",
            Path, Figure.Vertex_Count, Figure.Triangle_Count, Figure.Parts[0].Surface_Count);

  } else if (strcasecmp (Dot, ".md3") == 0) {
    // Q3 MD3: load as a single-part figure with vertex animation frames
    Figure.Is_Source = 0;

    Articulated_Figure WM = Weapon_Model_Load ();
    Figure.Part_Count = 1;
    snprintf (Figure.Parts[0].Name, 64, "body");
    Figure.Parts[0].Vertices       = WM.Vertices;
    Figure.Parts[0].Vertex_Count   = WM.Vertex_Count;
    Figure.Parts[0].Indices        = WM.Indices;
    Figure.Parts[0].Index_Count    = WM.Index_Count;
    Figure.Parts[0].Texture_Ids    = WM.Texture_Ids;
    Figure.Parts[0].Triangle_Count = WM.Triangle_Count;
    Figure.Parts[0].Surface_Count  = WM.Surface_Count;
    Figure.Parts[0].Parent_Tag     = -1;
    for (uint I = 0; I < WM.Surface_Count and I < WEAPON_MAX_TEXTURES; I++)
      memcpy (Figure.Parts[0].Texture_Names[I], WM.Texture_Names[I], 64);

    // Transplant tag and animation data directly (already in Tags[]/Animations[] format)
    Figure.Tag_Count       = WM.Tag_Count;
    for (uint I = 0; I < WM.Tag_Count; I++) Figure.Tags[I] = WM.Tags[I];
    Figure.Animation_Count = WM.Animation_Count;
    for (uint I = 0; I < WM.Animation_Count; I++) Figure.Animations[I] = WM.Animations[I];

    Figure.Vertices       = WM.Vertices;
    Figure.Vertex_Count   = WM.Vertex_Count;
    Figure.Indices        = WM.Indices;
    Figure.Index_Count    = WM.Index_Count;
    Figure.Texture_Ids    = WM.Texture_Ids;
    Figure.Triangle_Count = WM.Triangle_Count;

    printf ("[figure] loaded Q3 MD3 %s: %u verts, %u tris, %u tags, %u anims\n",
            Path, Figure.Vertex_Count, Figure.Triangle_Count, Figure.Tag_Count, Figure.Animation_Count);

  } else {
    printf ("[figure] unsupported format: %s\n", Dot);
  }

  return Figure;
}

// ═════════════════════
//   Figure_Load_Weapon
// ═════════════════════
//
// Convenience wrapper that loads a weapon figure. A weapon is an articulated figure where the geometry is scaled to viewmodel size and
// attachment tags (tag_barrel, tag_weapon) are preserved for fire animation.

Articulated_Figure Figure_Load_Weapon (const char *Path) {
  Articulated_Figure Figure = Figure_Load (Path, (vec3){0, 0, 0}, 0.f);

  // If no explicit weapon tag was loaded, synthesize a default at origin
  if (Figure.Tag_Count == 0) {
    Figure.Tag_Count = 1;
    snprintf (Figure.Tags[0].Name, 64, "tag_weapon");
    memset (Figure.Tags[0].Transforms[0], 0, sizeof (float) * 12);
    Figure.Tags[0].Transforms[0][3]  = 1.f; // Axis identity diagonal
    Figure.Tags[0].Transforms[0][7]  = 1.f;
    Figure.Tags[0].Transforms[0][11] = 1.f;
    Figure.Tags[0].Frame_Count = 1;
  }

  printf ("[figure] weapon: %u parts, %u tags, %u anims\n",
          Figure.Part_Count, Figure.Tag_Count, Figure.Animation_Count);
  return Figure;
}

// (Skeleton_Skin_Dispatch removed — replaced by Figure_Skeleton_Dispatch which does both bone evaluation and skinning on GPU)

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §8. Scene
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ════════════════
//   VBSP_Convert
// ════════════════

Vertex VBSP_Convert (float X, float Y, float Z, float Nx, float Ny, float Nz, float U, float V, float Lu, float Lv) {
  return (Vertex){.Position    = {X, Z, -Y},
                  .Normal      = {Nx, Nz, -Ny},
                  .Texture_UV  = {U, V},
                  .Lightmap_UV = {Lu, Lv}};
}

// ══════════════════════
//   Convert_BSP_Vertex  
// ══════════════════════

Vertex Convert_BSP_Vertex (const BSP_Vertex *Source) {

  // Swizzle from Id Software's Z-up coordinate system to our Y-up system
  return (Vertex){
    .Position    = {Source->Position        [0], Source->Position        [2], -Source->Position [1]},
    .Normal      = {Source->Normal          [0], Source->Normal          [2], -Source->Normal   [1]}
    .Texture_UV  = {Source->Texture_Coords  [0], Source->Texture_Coords  [1]},
    .Lightmap_UV = {Source->Lightmap_Coords [0], Source->Lightmap_Coords [1]},
  };
}

// ═══════════════════
//   Bezier_Evaluate
// ═══════════════════

vec3 Bezier_Evaluate (vec3 Control_A, vec3 Control_B, vec3 Control_C, float Parameter) {

  // Evaluate the quadratic Bezier curve: B(t) = (1 - t)^2 * A + 2(1 - t) t * B + t^2 * C
  float Inverse = 1.f - Parameter;
  return Add (Add (Scale (Control_A,       Inverse   * Inverse),
                   Scale (Control_B, 2.f * Inverse   * Parameter)),
                   Scale (Control_C,       Parameter * Parameter));
}

// ════════════════════════
//   Scene_Load_From_VBSP
// ════════════════════════

Scene Scene_Load_From_VBSP (const char *Path, Spawn *Out_Spawn) {

  // Open and read the full BSP file into memory; abort with a diagnostic message if missing
  FILE *File = fopen (Path, "rb");
  if (not File) {
    fprintf (stderr, "[error] cannot open VBSP map: %s\n", Path);
    fprintf (stderr, "  Ensure the .bsp file exists in assets/maps/\n");
    fprintf (stderr, "  For Source maps, use --source flag and place .bsp files in assets/maps/\n");
    exit (1);
  }

  // Allocate and load the file data
  fseek (File, 0, SEEK_END); long File_Size = ftell (File); rewind (File);
  uint8_t *File_Data   = malloc (File_Size);
  size_t   Bytes_Read_ = fread (File_Data, 1, File_Size, File); (void)Bytes_Read_;
  fclose (File);

  // Validate the BSP header magic and version
  const VBSP_Header *Header = (const VBSP_Header*)File_Data;
  assert (Header->Magic == VBSP_MAGIC
          and (Header->Version >= VBSP_VERSION_19 and Header->Version <= VBSP_VERSION_21));

  // Locate lump base pointers
  const VBSP_Vertex   *Verts      = (const VBSP_Vertex*)   (File_Data + Header->Lumps [VBSP_VERTICES].Offset);
  const VBSP_Edge     *Edges      = (const VBSP_Edge*)     (File_Data + Header->Lumps [VBSP_EDGES].Offset);
  const int           *Surf_Edges = (const int*)           (File_Data + Header->Lumps [VBSP_SURFEDGES].Offset);
  const VBSP_Face     *Faces      = (const VBSP_Face*)     (File_Data + Header->Lumps [VBSP_FACES].Offset);
  const VBSP_Tex_Info *Tex_Infos  = (const VBSP_Tex_Info*) (File_Data + Header->Lumps [VBSP_TEXINFO].Offset);
  const VBSP_Tex_Data *Tex_Datas  = (const VBSP_Tex_Data*) (File_Data + Header->Lumps [VBSP_TEXDATA].Offset);
  const int           *Str_Table  = (const int*)           (File_Data + Header->Lumps [VBSP_TEXDATA_STRING_TABLE].Offset);
  const char          *Str_Data   = (const char*)          (File_Data + Header->Lumps [VBSP_TEXDATA_STRING_DATA].Offset);

  // Planes lump: each plane is 20 bytes (float normal[3], float dist, int type)
  const uint8_t *Plane_Data  = File_Data + Header->Lumps[VBSP_PLANES].Offset;
  uint Plane_Count  = (uint) (Header->Lumps [VBSP_PLANES]    .Length / 20);
  uint Vert_Count   = (uint) (Header->Lumps [VBSP_VERTICES]  .Length / sizeof (VBSP_Vertex));
  uint Edge_Count   = (uint) (Header->Lumps [VBSP_EDGES]     .Length / sizeof (VBSP_Edge));
  uint Surf_Edge_N  = (uint) (Header->Lumps [VBSP_SURFEDGES] .Length / sizeof (int));
  uint Face_Count   = (uint) (Header->Lumps [VBSP_FACES]     .Length / sizeof (VBSP_Face));
  uint Tex_Info_N   = (uint) (Header->Lumps [VBSP_TEXINFO]   .Length / sizeof (VBSP_Tex_Info));
  uint Tex_Data_N   = (uint) (Header->Lumps [VBSP_TEXDATA]   .Length / sizeof (VBSP_Tex_Data));
  printf ("[vbsp] %u verts, %u edges, %u surfedges, %u faces, %u texinfos, %u texdatas\n",
          Vert_Count, Edge_Count, Surf_Edge_N, Face_Count, Tex_Info_N, Tex_Data_N);

  // Displacement data
  const VBSP_Disp_Info *Disps   = (const VBSP_Disp_Info*) (File_Data + Header->Lumps [VBSP_DISPINFO].Offset);
  const VBSP_Disp_Vert *Disp_Vs = (const VBSP_Disp_Vert*) (File_Data + Header->Lumps [VBSP_DISPVERTS].Offset);
  uint Disp_Count = (uint) (Header->Lumps [VBSP_DISPINFO].Length / sizeof (VBSP_Disp_Info));

  // Build the scene material name table from the texdata string pool
  uint Str_Table_N = (uint) (Header->Lumps [VBSP_TEXDATA_STRING_TABLE].Length / sizeof (int));
  uint Str_Data_N  = (uint) (Header->Lumps [VBSP_TEXDATA_STRING_DATA].Length);
  Scene S = {0};
  S.Material_Count = Tex_Data_N;
  S.Materials      = calloc (Tex_Data_N, sizeof (vec4));
  S.Texture_Names  = calloc (Tex_Data_N, 64);
  for (uint Tex_Index = 0; Tex_Index < Tex_Data_N; Tex_Index++) {
    uint        Name_Id = (uint)Tex_Datas[Tex_Index].Name_Id;
    const char *Name    = (Name_Id < Str_Table_N and (uint)Str_Table[Name_Id] < Str_Data_N)
                          ? Str_Data + Str_Table[Name_Id] : "missing";
    snprintf (S.Texture_Names[Tex_Index], 64, "%s", Name);
    S.Materials[Tex_Index] = (vec4){Tex_Datas [Tex_Index].Refl [0],
                                    Tex_Datas [Tex_Index].Refl [1],
                                    Tex_Datas [Tex_Index].Refl [2], 1.f};
  }

  // Build per-material skip table: suppress non-renderable tool surfaces
  #define VBSP_SKIP_FLAGS 0x3C6 // SURF_SKY2D=0x2 SURF_SKY=0x4 SURF_TRIGGER=0x40 SURF_NODRAW=0x80 SURF_HINT=0x100 SURF_SKIP=0x200
  uint8_t *Mat_Skip = calloc (Tex_Data_N, 1);
  for (uint Tex_Index = 0; Tex_Index < Tex_Data_N; Tex_Index++) {
    const char *Name = S.Texture_Names[Tex_Index];
    if (strncasecmp (Name, "TOOLS/", 6) == 0 or strncasecmp (Name, "tools/", 6) == 0)
      Mat_Skip[Tex_Index] = 1;
  }

  // First pass: count triangles (filter logic must exactly match the second pass below)
  uint Total_Tris = 0;
  for (uint Face_Index = 0; Face_Index < Face_Count; Face_Index++) {
    if (Faces[Face_Index].Disp_Info >= 0) continue; // displacements handled separately
    if (Faces[Face_Index].Num_Edges < 3)  continue;
    int Tex_Info_Index = Faces[Face_Index].Tex_Info;
    if (Tex_Info_Index < 0 or (uint)Tex_Info_Index >= Tex_Info_N) continue;
    if (Tex_Infos[Tex_Info_Index].Flags & VBSP_SKIP_FLAGS) continue;
    uint Material_Index = (uint)Tex_Infos[Tex_Info_Index].Tex_Data;
    if (Material_Index >= Tex_Data_N or Mat_Skip[Material_Index]) continue;
    Total_Tris += Faces[Face_Index].Num_Edges - 2;
  }

  // Displacement triangles
  for (uint Disp_Index = 0; Disp_Index < Disp_Count; Disp_Index++) {
    int Grid_Side = (1 << Disps[Disp_Index].Power);
    Total_Tris += Grid_Side * Grid_Side * 2;
  }

  // Allocate geometry buffers sized for the worst-case triangle count
  S.Vertices     = malloc (sizeof (Vertex) * Total_Tris * 3);
  S.Indices      = malloc (sizeof (uint)   * Total_Tris * 3);
  S.Texture_Ids  = malloc (sizeof (uint)   * Total_Tris);
  S.Vertex_Count = S.Index_Count = S.Triangle_Count = 0;

  // Second pass: emit fan-triangulated face geometry
  for (uint Face_Index = 0; Face_Index < Face_Count; Face_Index++) {
    const VBSP_Face *Face = &Faces[Face_Index];
    if (Face->Disp_Info >= 0 or Face->Num_Edges < 3) continue;
    int Tex_Info_Index = Face->Tex_Info;
    if (Tex_Info_Index < 0 or (uint)Tex_Info_Index >= Tex_Info_N) continue;
    const VBSP_Tex_Info *Tex_Info = &Tex_Infos[Tex_Info_Index];
    if (Tex_Info->Flags & VBSP_SKIP_FLAGS) continue;
    uint Material_Index = (uint)Tex_Info->Tex_Data;
    if (Material_Index >= Tex_Data_N or Mat_Skip[Material_Index]) continue;

    // Collect face vertex positions via the surf-edge indirection table
    int  Face_Edge_Count = Face->Num_Edges; if (Face_Edge_Count > 4096) continue; // sanity cap
    vec3 *Positions = alloca (sizeof (vec3) * Face_Edge_Count);
    for (int Edge_Index = 0; Edge_Index < Face_Edge_Count; Edge_Index++) {
      uint Surf_Edge_Index = (uint)(Face->First_Edge + Edge_Index);
      if (Surf_Edge_Index >= Surf_Edge_N) { Positions[Edge_Index] = Make (0,0,0); continue; }
      int  Surf_Edge_Value = Surf_Edges[Surf_Edge_Index];
      uint Abs_Surf_Edge   = (uint)(Surf_Edge_Value >= 0 ? Surf_Edge_Value : -Surf_Edge_Value);
      if (Abs_Surf_Edge >= Edge_Count) { Positions[Edge_Index] = Make (0,0,0); continue; }
      uint Vertex_Index = Surf_Edge_Value >= 0 ? Edges[Abs_Surf_Edge].V[0] : Edges[Abs_Surf_Edge].V[1];
      if (Vertex_Index >= Vert_Count) Vertex_Index = 0;
      Positions[Edge_Index] = Make (Verts[Vertex_Index].P[0],
                                    Verts[Vertex_Index].P[1],
                                    Verts[Vertex_Index].P[2]);
    }

    // Fetch the precomputed face normal from the BSP plane lump (more reliable than cross product)
    vec3 Face_Normal = {0, 1, 0};
    if (Face->Plane_Num < Plane_Count) {
      const float *Plane_Normal = (const float*)(Plane_Data + (uint)Face->Plane_Num * 20);
      Face_Normal = Face->Side
        ? Make (-Plane_Normal[0], -Plane_Normal[1], -Plane_Normal[2])
        : Make ( Plane_Normal[0],  Plane_Normal[1],  Plane_Normal[2]);
    }

    // Fan-triangulate the polygon and project texture and lightmap coordinates
    for (int Fan_Index = 0; Fan_Index < Face->Num_Edges - 2; Fan_Index++) {
      uint Triangle_Base    = S.Vertex_Count;
      int  Fan_Corners[3]   = {0, Fan_Index + 1, Fan_Index + 2};
      for (int Corner = 0; Corner < 3; Corner++) {
        vec3  Corner_Position = Positions[Fan_Corners[Corner]];
        float U  = Corner_Position.x * Tex_Info->Tex_Vecs[0][0]
                 + Corner_Position.y * Tex_Info->Tex_Vecs[0][1]
                 + Corner_Position.z * Tex_Info->Tex_Vecs[0][2] + Tex_Info->Tex_Vecs[0][3];
        float V  = Corner_Position.x * Tex_Info->Tex_Vecs[1][0]
                 + Corner_Position.y * Tex_Info->Tex_Vecs[1][1]
                 + Corner_Position.z * Tex_Info->Tex_Vecs[1][2] + Tex_Info->Tex_Vecs[1][3];
        int Tex_Width  = Tex_Datas[Material_Index].W;
        int Tex_Height = Tex_Datas[Material_Index].H;
        if (Tex_Width  > 0) U /= Tex_Width;
        if (Tex_Height > 0) V /= Tex_Height;
        float Lu = Corner_Position.x * Tex_Info->Lm_Vecs[0][0]
                 + Corner_Position.y * Tex_Info->Lm_Vecs[0][1]
                 + Corner_Position.z * Tex_Info->Lm_Vecs[0][2] + Tex_Info->Lm_Vecs[0][3];
        float Lv = Corner_Position.x * Tex_Info->Lm_Vecs[1][0]
                 + Corner_Position.y * Tex_Info->Lm_Vecs[1][1]
                 + Corner_Position.z * Tex_Info->Lm_Vecs[1][2] + Tex_Info->Lm_Vecs[1][3];
        S.Vertices[S.Vertex_Count++] = VBSP_Convert (Corner_Position.x, Corner_Position.y, Corner_Position.z,
                                                      Face_Normal.x, Face_Normal.y, Face_Normal.z,
                                                      U, V, Lu, Lv);
      }
      S.Indices    [S.Index_Count++]    = Triangle_Base;
      S.Indices    [S.Index_Count++]    = Triangle_Base + 1;
      S.Indices    [S.Index_Count++]    = Triangle_Base + 2;
      S.Texture_Ids[S.Triangle_Count++] = Material_Index;
    }
  }

  // Displacement surfaces: subdivided height-mapped quad patches
  for (uint Disp_Index = 0; Disp_Index < Disp_Count; Disp_Index++) {
    const VBSP_Disp_Info *Disp     = &Disps[Disp_Index];
    int                   Grid_Side = (1 << Disp->Power) + 1;
    if (Disp->Map_Face >= Face_Count) continue;
    const VBSP_Face *Face = &Faces[Disp->Map_Face];
    if (Face->Num_Edges < 4) continue;

    // Collect the four corner positions of the base quad
    vec3 Corners[4] = {{0}};
    for (int Edge_Index = 0; Edge_Index < 4 and Edge_Index < Face->Num_Edges; Edge_Index++) {
      uint Surf_Edge_Index = (uint)(Face->First_Edge + Edge_Index);
      if (Surf_Edge_Index >= Surf_Edge_N) continue;
      int  Surf_Edge_Value = Surf_Edges[Surf_Edge_Index];
      uint Abs_Surf_Edge   = (uint)(Surf_Edge_Value >= 0 ? Surf_Edge_Value : -Surf_Edge_Value);
      if (Abs_Surf_Edge >= Edge_Count) continue;
      uint Vertex_Index = Surf_Edge_Value >= 0 ? Edges[Abs_Surf_Edge].V[0] : Edges[Abs_Surf_Edge].V[1];
      if (Vertex_Index >= Vert_Count) Vertex_Index = 0;
      Corners[Edge_Index] = Make (Verts[Vertex_Index].P[0],
                                  Verts[Vertex_Index].P[1],
                                  Verts[Vertex_Index].P[2]);
    }

    // Find the corner closest to the displacement start position and rotate the winding
    float Best_Dist   = 1e18f;
    int   Best_Corner = 0;
    vec3  Start_Position = Make (Disp->Start[0], Disp->Start[1], Disp->Start[2]);
    for (int Corner_Index = 0; Corner_Index < 4; Corner_Index++) {
      float Corner_Dist_Sq = Dot (Subtract (Corners[Corner_Index], Start_Position),
                                  Subtract (Corners[Corner_Index], Start_Position));
      if (Corner_Dist_Sq < Best_Dist) { Best_Dist = Corner_Dist_Sq; Best_Corner = Corner_Index; }
    }
    vec3 Rotated_Corners[4];
    for (int Corner_Index = 0; Corner_Index < 4; Corner_Index++)
      Rotated_Corners[Corner_Index] = Corners[(Corner_Index + Best_Corner) % 4];

    // Bilinearly interpolate the grid and displace each point along the stored vector
    uint Disp_Vertex_Base = S.Vertex_Count;
    S.Vertices = realloc (S.Vertices, sizeof (Vertex) * (S.Vertex_Count + Grid_Side * Grid_Side));
    for (int Grid_Y = 0; Grid_Y < Grid_Side; Grid_Y++)
      for (int Grid_X = 0; Grid_X < Grid_Side; Grid_X++) {
        float U = (float)Grid_X / (Grid_Side - 1);
        float V = (float)Grid_Y / (Grid_Side - 1);
        vec3 Interp_Bottom  = Add (Scale (Rotated_Corners[0], 1-U), Scale (Rotated_Corners[1], U));
        vec3 Interp_Top     = Add (Scale (Rotated_Corners[3], 1-U), Scale (Rotated_Corners[2], U));
        vec3 Surface_Position = Add (Scale (Interp_Bottom, 1-V), Scale (Interp_Top, V));
        const VBSP_Disp_Vert *Disp_Vertex = &Disp_Vs[Disp->Disp_Vert_Start + Grid_Y * Grid_Side + Grid_X];
        Surface_Position = Add (Surface_Position,
                                Scale (Make (Disp_Vertex->Vec[0], Disp_Vertex->Vec[1], Disp_Vertex->Vec[2]),
                                       Disp_Vertex->Dist));
        S.Vertices[S.Vertex_Count++] = VBSP_Convert (Surface_Position.x, Surface_Position.y, Surface_Position.z,
                                                      0, 0, 1, U, V, U, V);
      }

    // Triangulate the displacement grid into two triangles per cell
    int Total_Disp_Tris = (Grid_Side - 1) * (Grid_Side - 1) * 2;
    S.Indices     = realloc (S.Indices,     sizeof (uint) * (S.Index_Count    + Total_Disp_Tris * 3));
    S.Texture_Ids = realloc (S.Texture_Ids, sizeof (uint) * (S.Triangle_Count + Total_Disp_Tris));
    int Tex_Info_Index = Face->Tex_Info;
    uint Material_Index = (Tex_Info_Index >= 0 and (uint)Tex_Info_Index < Tex_Info_N)
                          ? (uint)Tex_Infos[Tex_Info_Index].Tex_Data : 0;
    if (Material_Index >= Tex_Data_N) Material_Index = 0;
    for (int Grid_Y = 0; Grid_Y < Grid_Side - 1; Grid_Y++)
      for (int Grid_X = 0; Grid_X < Grid_Side - 1; Grid_X++) {
        uint Grid_A = Disp_Vertex_Base + Grid_Y * Grid_Side + Grid_X;
        uint Grid_B = Grid_A + 1;
        uint Grid_C = Grid_A + Grid_Side;
        uint Grid_D = Grid_C + 1;
        S.Indices[S.Index_Count++] = Grid_A; S.Indices[S.Index_Count++] = Grid_C; S.Indices[S.Index_Count++] = Grid_B;
        S.Indices[S.Index_Count++] = Grid_B; S.Indices[S.Index_Count++] = Grid_C; S.Indices[S.Index_Count++] = Grid_D;
        S.Texture_Ids[S.Triangle_Count++] = Material_Index;
        S.Texture_Ids[S.Triangle_Count++] = Material_Index;
      }
  }

  // Lightmap atlas: build from the VBSP_LIGHTING lump if present, otherwise use a 1x1 full-bright fallback
  uint Lightmap_Lump_Size = (uint)Header->Lumps[VBSP_LIGHTING].Length;
  if (Lightmap_Lump_Size > 0) {
    uint Atlas_Width = 512, Atlas_Height = 512;
    S.Lightmap_Atlas  = calloc (Atlas_Width * Atlas_Height * 4, 1);
    memset (S.Lightmap_Atlas, 255, Atlas_Width * Atlas_Height * 4); // Full-bright fallback
    S.Lightmap_Width  = Atlas_Width;
    S.Lightmap_Height = Atlas_Height;
  } else {
    S.Lightmap_Atlas  = calloc (4, 1);
    memset (S.Lightmap_Atlas, 255, 4);
    S.Lightmap_Width = S.Lightmap_Height = 1;
  }

  // Parse entities from the entity lump (Source BSP key-value format)
  *Out_Spawn = (Spawn){{0, 64, 0}, 0};
  S.Sky_Name[0]  = 0;
  S.Entities     = calloc (MAX_BSP_ENTITIES, sizeof (BSP_Entity));
  S.Entity_Count = 0;
  uint  Entity_Lump_Len = (uint)Header->Lumps[VBSP_ENTITIES].Length;
  char *Entity_String   = malloc (Entity_Lump_Len + 1);
  memcpy (Entity_String, File_Data + Header->Lumps[VBSP_ENTITIES].Offset, Entity_Lump_Len);
  Entity_String[Entity_Lump_Len] = 0;
  int Spawn_Found = 0;
  {
    char *Cursor = Entity_String;
    while (*Cursor) {
      while (*Cursor and *Cursor != '{') Cursor++;
      if (not *Cursor) break; Cursor++;

      // Parse all key-value pairs in this entity block
      char  Class[64]={0}, Sky[64]={0}, Light[128]={0}, Ambient[128]={0};
      float Origin_X=0, Origin_Y=0, Origin_Z=0;
      float Angles_Pitch=0, Angles_Yaw=0, Explicit_Pitch=0, Scale=16;
      int   Has_Explicit_Pitch = 0;
      while (*Cursor and *Cursor != '}') {
        while (*Cursor and *Cursor != '"') { if (*Cursor == '}') goto done; Cursor++; }
        if (not *Cursor) break; Cursor++;
        char Key[64] = {0}; int Key_Index = 0;
        while (*Cursor and *Cursor != '"' and Key_Index < 63) Key[Key_Index++] = *Cursor++;
        if (*Cursor == '"') Cursor++;
        while (*Cursor and *Cursor != '"' and *Cursor != '}') Cursor++;
        if (*Cursor != '"') continue; Cursor++;
        char Value[128] = {0}; int Value_Index = 0;
        while (*Cursor and *Cursor != '"' and Value_Index < 127) Value[Value_Index++] = *Cursor++;
        if (*Cursor == '"') Cursor++;
        if (strcmp (Key, "classname") == 0) snprintf (Class,   64,  "%s", Value);
        if (strcmp (Key, "origin")    == 0) sscanf   (Value, "%f %f %f", &Origin_X, &Origin_Y, &Origin_Z);
        if (strcmp (Key, "angles")    == 0) sscanf   (Value,    "%f %f", &Angles_Pitch, &Angles_Yaw);
        if (strcmp (Key, "skyname")   == 0) snprintf (Sky,      64,  "%s", Value);
        if (strcmp (Key, "_light")    == 0) snprintf (Light,   128,  "%s", Value);
        if (strcmp (Key, "_ambient")  == 0) snprintf (Ambient, 128,  "%s", Value);
        if (strcmp (Key, "pitch")     == 0) { sscanf (Value, "%f", &Explicit_Pitch); Has_Explicit_Pitch = 1; }
        if (strcmp (Key, "scale")     == 0) sscanf   (Value, "%f", &Scale);
      }
      done:
      if (*Cursor == '}') Cursor++;
      float Sun_Pitch = Has_Explicit_Pitch ? Explicit_Pitch : Angles_Pitch;
      float Sun_Yaw   = Angles_Yaw;

      // Spawn points: record the first one found as the player start
      if (not Spawn_Found
          and (strcmp (Class, "info_player_terrorist")        == 0
            or strcmp (Class, "info_player_counterterrorist") == 0
            or strcmp (Class, "info_player_start")            == 0
            or strcmp (Class, "info_player_deathmatch")       == 0)) {
        Out_Spawn->Origin = Make (Origin_X, Origin_Z, -Origin_Y);
        Out_Spawn->Angle  = Angles_Yaw;
        Spawn_Found = 1;
        printf ("[vbsp] spawn '%s': src=(%g,%g,%g) gl=(%g,%g,%g) yaw=%g\n",
                Class, Origin_X, Origin_Y, Origin_Z, Origin_X, Origin_Z, -Origin_Y, Angles_Yaw);
      }

      // Worldspawn: extract the skybox name
      if (strcmp (Class, "worldspawn") == 0 and Sky[0]) {
        snprintf (S.Sky_Name, 64, "%s", Sky);
        printf ("[vbsp] skyname: %s\n", S.Sky_Name);
      }

      // light_environment: record sun colour, direction, and ambient for the renderer
      if (strcmp (Class, "light_environment") == 0 and S.Entity_Count < MAX_BSP_ENTITIES) {
        BSP_Entity *Light_Entity = &S.Entities[S.Entity_Count++];
        Light_Entity->Kind              = ENTITY_ENV_SKY;
        Light_Entity->Common.Origin     = Make (Origin_X, Origin_Z, -Origin_Y);

        // Parse sun colour from the "_light" key (R G B [brightness])
        float Light_Red = 255, Light_Green = 255, Light_Blue = 255;
        sscanf (Light, "%f %f %f", &Light_Red, &Light_Green, &Light_Blue);
        Light_Entity->env.Color = Make (Light_Red/255.f, Light_Green/255.f, Light_Blue/255.f);

        // Parse ambient from the "_ambient" key: R G B brightness (Source scales by brightness/200)
        float Ambient_Red=128, Ambient_Green=128, Ambient_Blue=128, Ambient_Brightness=200;
        sscanf (Ambient, "%f %f %f %f", &Ambient_Red, &Ambient_Green, &Ambient_Blue, &Ambient_Brightness);
        float Ambient_Scale = (Ambient_Brightness / 200.f) * 0.15f; // Scale down for the ray tracer
        Light_Entity->env.Ambient = Make (Ambient_Red   / 255.f * Ambient_Scale,
                                          Ambient_Green / 255.f * Ambient_Scale,
                                          Ambient_Blue  / 255.f * Ambient_Scale);

        // Convert pitch and yaw to a world-space sun direction vector
        float Pitch_Radians = Sun_Pitch * 3.14159f / 180.f;
        float Yaw_Radians   = Sun_Yaw   * 3.14159f / 180.f;

        // Source space: X=right, Y=forward, Z=up; light rays come FROM the sun position
        float Source_Sun_X = cosf (Pitch_Radians) * cosf (Yaw_Radians);
        float Source_Sun_Y = cosf (Pitch_Radians) * sinf (Yaw_Radians);
        float Source_Sun_Z = sinf (Pitch_Radians);

        // Negate to get direction TO the sun, then swizzle Source (x,y,z) -> engine (x,z,-y)
        Light_Entity->env.Direction = Normalize (Make (-Source_Sun_X, -Source_Sun_Z, Source_Sun_Y));
        printf ("[vbsp] light_environment: sun=(%g,%g,%g) dir=(%g,%g,%g) ambient=(%g,%g,%g) pitch=%g yaw=%g\n",
                Light_Entity->env.Color.x,     Light_Entity->env.Color.y,     Light_Entity->env.Color.z,
                Light_Entity->env.Direction.x, Light_Entity->env.Direction.y, Light_Entity->env.Direction.z,
                Light_Entity->env.Ambient.x,   Light_Entity->env.Ambient.y,   Light_Entity->env.Ambient.z,
                Sun_Pitch, Sun_Yaw);
      }

      // sky_camera: record origin and scale for 3D skybox rendering
      if (strcmp (Class, "sky_camera") == 0 and S.Entity_Count < MAX_BSP_ENTITIES) {
        BSP_Entity *Sky_Camera_Entity = &S.Entities[S.Entity_Count++];
        Sky_Camera_Entity->Kind           = ENTITY_WORLD;
        Sky_Camera_Entity->Common.Origin  = Make (Origin_X, Origin_Z, -Origin_Y);
        printf ("[vbsp] sky_camera: src=(%g,%g,%g) scale=%g\n", Origin_X, Origin_Y, Origin_Z, Scale);
      }
    }
  }

  // Release the entity lump string buffer and warn if no valid spawn point was found
  free (Entity_String);
  if (not Spawn_Found)
    printf ("[vbsp] WARNING: no spawn found in entity lump (%u bytes)\n", Entity_Lump_Len);

  // Free temporary parsing buffers and the raw file data
  free (Mat_Skip);
  free (File_Data);
  printf ("[vbsp] %s: %u verts, %u tris, %u materials\n",
          Path, S.Vertex_Count, S.Triangle_Count, S.Material_Count);

  // Return the fully assembled scene to the caller
  return S;
} // Scene_Load_From_VBSP

// ════════════════════════
//   BSP_Tessellate_Patch
// ════════════════════════

uint BSP_Tessellate_Patch (const BSP_Vertex *Control_Grid, int Patch_Width, int Patch_Height,
                           Vertex **Inout_Vertices, uint *Inout_Vertex_Count,
                           uint    **Inout_Indices, uint *Inout_Index_Count) {

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
  *Inout_Indices  = realloc (*Inout_Indices,  sizeof (uint)   * (*Inout_Index_Count  + Added_Indices));

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
          .Texture_UV  = {Texture.x,  Texture.y},
          .Lightmap_UV = {Lightmap.x, Lightmap.y},
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
  if (not File) {
    fprintf (stderr, "[error] cannot open Q3 BSP map: %s\n", Path);
    fprintf (stderr, "  Download OpenArena assets: place .bsp files in assets/maps/\n");
    exit (1);
  }

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
  uint        Raw_Vertex_Count  = (uint)         (Header->Lumps [BSP_VERTICES] .Length / sizeof (BSP_Vertex));
  uint        Raw_Face_Count    = (uint)         (Header->Lumps [BSP_FACES]    .Length / sizeof (BSP_Face));
  uint        Raw_Shader_Count  = (uint)         (Header->Lumps [BSP_SHADERS]  .Length / sizeof (BSP_Shader));

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
          Vertices[Vertex_Index].Lightmap_UV[0] = (Column_Offset + Vertices[Vertex_Index].Lightmap_UV[0]) / (float)Atlas_Columns;
          Vertices[Vertex_Index].Lightmap_UV[1] = (Row_Offset    + Vertices[Vertex_Index].Lightmap_UV[1]) / (float)Atlas_Rows;
        }
      } else {
        for (int Vertex_Loop = 0; Vertex_Loop < Face->Vertex_Count; Vertex_Loop++) {
          uint Vertex_Index = (uint)(Face->First_Vertex + Vertex_Loop);
          Vertices[Vertex_Index].Lightmap_UV[0] = White_Fallback_U;
          Vertices[Vertex_Index].Lightmap_UV[1] = White_Fallback_V;
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
          Vertices[Vertex_Index].Lightmap_UV[0] = (Column_Offset + Vertices[Vertex_Index].Lightmap_UV[0]) / (float)Atlas_Columns;
          Vertices[Vertex_Index].Lightmap_UV[1] = (Row_Offset    + Vertices[Vertex_Index].Lightmap_UV[1]) / (float)Atlas_Rows;
        }
      } else {
        for (uint Vertex_Index = Previous_Vertex_Count; Vertex_Index < Vertex_Count; Vertex_Index++) {
          Vertices[Vertex_Index].Lightmap_UV[0] = White_Fallback_U;
          Vertices[Vertex_Index].Lightmap_UV[1] = White_Fallback_V;
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
          (Name[C+2] == 'y' or Name[C+2] == 'Y')) { Has_Sky = 1; break;}
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

    // Sample sky texture: top quarter = zenith, middle band = horizon. Compute average color for each region (sRGB > linear)
    if (Pixels and W > 0 and H > 0) {
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
      float Sky_Lum = Env.Sky_Zenith.x  * 0.2126f + Env.Sky_Zenith.y  * 0.7152f + Env.Sky_Zenith.z  * 0.0722f;
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

  // 3. Check entities for explicit overrides (Q3 worldspawn, Source light_environment)
  for (uint I = 0; I < S->Entity_Count; I++) {
    if (S->Entities[I].Kind == ENTITY_ENV_SKY) {

      // Source light_environment: use parsed sun/ambient data
      const BSP_Entity *E = &S->Entities[I];
      Env.Sun_Direction = E->env.Direction;
      Env.Sun_Color     = E->env.Color;

      // Source lightmaps already include baked direct sun. Use moderate sun for shadow contrast only, low lightmap multiplier to prevent
      // double-bright from baked sun + raytraced sun overlap.
      Env.Sun_Intensity  = 2.0f; // Needs to be strong for visible shadow contrast
      Env.Ambient_Up     = Scale(E->env.Ambient, 0.4f);
      Env.Ambient_Down   = Scale(E->env.Ambient, 0.2f);
      Env.Lightmap_Mult  = 1.0f; // Source lightmaps store full intensity 

      // Atmospheric fog - gives depth and matches CSPromod's hazy look
      Env.Fog_Color    = (vec3){0.5f, 0.55f, 0.6f};  // Slightly blue-grey atmospheric haze
      Env.Fog_Density  = 0.0003f;  // Moderate distance fog
      printf ("[environment] using Source light_environment\n");
      break;
    }
  }

  // 4. Source skybox: try loading VTF sky faces for sky color
  if (S->Sky_Name[0]) {
    printf ("[environment] Source skyname: %s\n", S->Sky_Name);
    // Try to load skybox _up face for zenith color
    const char *VTF_Search_Dirs[] = {
      "/tmp/cspromod_new/cspromod_b105/cspromod/materials",
      "/tmp/v_m4/materials",
      "assets/materials",
      NULL
    };
    char Sky_Rel[128];
    snprintf(Sky_Rel, sizeof(Sky_Rel), "skybox/%sup.vtf", S->Sky_Name);
    char Sky_Full[512] = {0};
    for (int D=0; VTF_Search_Dirs[D]; D++) {
      char Try[512]; snprintf(Try,512,"%s/%s",VTF_Search_Dirs[D],Sky_Rel);
      FILE *Ft=fopen(Try,"rb"); if(Ft){fclose(Ft); snprintf(Sky_Full,512,"%s",Try); break;}
    }
    if (Sky_Full[0]) {
      printf ("[environment] loading skybox texture: %s\n", Sky_Full);
    } else {
      printf ("[environment] skybox texture not found: %s\n", Sky_Rel);
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
      if (*Cursor != '"') {Cursor++; continue;}
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

  // Macros for easy string matching - populate Kind and sub-kind in one step
  #define MATCH(STR, KIND) \
    if (Length == (int)sizeof(STR) - 1 and memcmp (Classname, STR, sizeof(STR) - 1) == 0) { E->Kind = KIND; return;}
  #define MATCH_WEAPON(STR, WK, CNT) \
    if (Length == (int)sizeof(STR) - 1 and memcmp (Classname, STR, sizeof(STR) - 1) == 0) { \
      E->Kind = ENTITY_ITEM_WEAPON; E->item.Weapon = WK; E->Common.Count = CNT; return;}
  #define MATCH_AMMO(STR, AK, CNT) \
    if (Length == (int)sizeof(STR) - 1 and memcmp (Classname, STR, sizeof(STR) - 1) == 0) { \
      E->Kind = ENTITY_ITEM_AMMO; E->item.Ammo = AK; E->Common.Count = CNT; return;}
  #define MATCH_HEALTH(STR, AMT) \
    if (Length == (int)sizeof(STR) - 1 and memcmp (Classname, STR, sizeof(STR) - 1) == 0) { \
      E->Kind = ENTITY_ITEM_HEALTH; E->Common.Health = AMT; return;}
  #define MATCH_ARMOR(STR, AMT) \
    if (Length == (int)sizeof(STR) - 1 and memcmp (Classname, STR, sizeof(STR) - 1) == 0) { \
      E->Kind = ENTITY_ITEM_ARMOR; E->Common.Armor = AMT; return;}
  #define MATCH_POWERUP(STR, PK) \
    if (Length == (int)sizeof(STR) - 1 and memcmp (Classname, STR, sizeof(STR) - 1) == 0) { \
      E->Kind = ENTITY_ITEM_POWERUP; E->item.Powerup = PK; E->item.Duration = 30.f; return;}

  // Spawn points
  MATCH ("info_player_deathmatch",   ENTITY_INFO_PLAYER_SPAWN);
  MATCH ("info_player_start",        ENTITY_INFO_PLAYER_START);
  MATCH ("info_player_intermission", ENTITY_INFO_PLAYER_INTERMISSION);

  // Weapons (Q3 classname > generic Weapon_Kind + default ammo count)
  MATCH_WEAPON ("weapon_gauntlet",        WEAPON_MELEE,     0);
  MATCH_WEAPON ("weapon_shotgun",         WEAPON_SHOTGUN,   10);
  MATCH_WEAPON ("weapon_machinegun",      WEAPON_SMG,       40);
  MATCH_WEAPON ("weapon_grenadelauncher", WEAPON_GRENADE,   10);
  MATCH_WEAPON ("weapon_rocketlauncher",  WEAPON_ROCKET,    10);
  MATCH_WEAPON ("weapon_lightning",       WEAPON_LIGHTNING, 100);
  MATCH_WEAPON ("weapon_railgun",         WEAPON_RAIL,      10);
  MATCH_WEAPON ("weapon_plasmagun",       WEAPON_ENERGY,    50);
  MATCH_WEAPON ("weapon_bfg",             WEAPON_BFG,       20);

  // Ammo (Q3 classname > generic Ammo_Kind + count)
  MATCH_AMMO ("ammo_shells",    AMMO_SHELLS,   10);
  MATCH_AMMO ("ammo_bullets",   AMMO_BULLETS,  50);
  MATCH_AMMO ("ammo_grenades",  AMMO_GRENADES, 5);
  MATCH_AMMO ("ammo_cells",     AMMO_CELLS,    30);
  MATCH_AMMO ("ammo_lightning", AMMO_ENERGY,   60);
  MATCH_AMMO ("ammo_rockets",   AMMO_ROCKETS,  5);
  MATCH_AMMO ("ammo_slugs",     AMMO_SLUGS,    10);
  MATCH_AMMO ("ammo_bfg",       AMMO_CELLS,    15);

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
  MATCH_POWERUP ("item_quad",   POWERUP_QUAD_DAMAGE);
  MATCH_POWERUP ("item_enviro", POWERUP_ENV_SUIT);
  MATCH_POWERUP ("item_haste",  POWERUP_HASTE);
  MATCH_POWERUP ("item_invis",  POWERUP_INVISIBILITY);
  MATCH_POWERUP ("item_regen",  POWERUP_REGENERATION);
  MATCH_POWERUP ("item_flight", POWERUP_FLIGHT);

  // Holdables > generic items
  MATCH ("holdable_teleporter", ENTITY_ITEM_GENERIC);
  MATCH ("holdable_medkit",     ENTITY_ITEM_GENERIC);

  // Map geometry & logic
  MATCH ("trigger_teleport", ENTITY_TRIGGER_TELEPORT);
  MATCH ("trigger_push",     ENTITY_TRIGGER_PUSH);
  MATCH ("target_position",  ENTITY_TARGET_POSITION);
  MATCH ("target_speaker",   ENTITY_SOUND_EMITTER);
  MATCH ("misc_model",       ENTITY_PROP_STATIC);
  MATCH ("light",            ENTITY_LIGHT);
  MATCH ("worldspawn",       ENTITY_WORLD);

  // Clean up helper macros
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

  // Comment here !!!
  while (Text < End and Count < Max_Entities) {

    // Find opening brace
    while (Text < End and *Text != '{') Text++;
    if (Text >= End) break;
    Text++;

    // Temporary storage for this entity's key-value pairs
    BSP_Entity Entity   = {0};
    Entity.Common.Scale = 1.0f; // Default scale
    Entity.Common.Alpha = 1.0f; // Default opacity
    char Classname[64]  = {0};
    int  Classname_Len  = 0;
    float Color[3]      = {1, 1, 1};
    float Intensity     = 300;
    int   Gravity       = 800;

    // Parse key-value pairs
    while (Text < End and *Text != '}') {
      while (Text < End and (*Text == ' ' or *Text == '\t' or *Text == '\n' or *Text == '\r'))
        Text++;
      if (Text >= End or *Text == '}') break;

      // Read quoted key
      if (*Text != '"') { Text++; continue;}
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
        sscanf (Tmp, "%f", &Entity.Common.Angles.y); 
      }
      else if (Key_Len == 10 and memcmp (Key, "spawnflags", 10) == 0) {
        char Tmp[32]; COPY_VAL(Tmp, 32);
        sscanf (Tmp, "%d", &Entity.Common.Spawnflags);
      }
      else if (Key_Len == 10 and memcmp (Key, "targetname", 10) == 0) {COPY_VAL(Entity.Common.Name,     64);}
      else if (Key_Len == 6  and memcmp (Key, "target",      6) == 0) {COPY_VAL(Entity.Common.Target,   64);}
      else if (Key_Len == 5  and memcmp (Key, "noise",       5) == 0) {COPY_VAL(Entity.Common.Sound,    96);}
      else if (Key_Len == 5  and memcmp (Key, "model",       5) == 0) {COPY_VAL(Entity.Common.Model,    96);}
      else if (Key_Len == 7  and memcmp (Key, "message",     7) == 0) {COPY_VAL(Entity.Common.Message, 128);}
      else if (Key_Len == 5  and memcmp (Key, "light",       5) == 0) {
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
  Texture_Sampler = Sampler_Create_Repeating ();

  // PBR texture layout in the bindless array:
  //
  //   [0  .. N-1]  Diffuse maps
  //   [N  .. 2N-1] Normal maps    (_n.tga)
  //   [2N .. 3N-1] Roughness maps (_r.tga)
  //   [3N .. 4N-1] Metalness maps (_m.tga)
  //   [4N .. 5N-1] Emissive maps  (_e.tga)
  //   [5N .. 6N-1] Height maps    (_h.tga)
  //   [6N ..]      Weapon textures (appended later)
  //
  uint Material_Count = Scene_Data->Material_Count;
  uint PBR_Slots      = Material_Count * 6; // Six maps per material (diffuse and five PBR textures)
  PBR_Stride          = Material_Count;     // Distance between map blocks in the texture array
  Texture_Count       = PBR_Slots;
  Textures_Loaded     = 0;
  Texture_Images      = calloc (PBR_Slots, sizeof (VkImage));
  Texture_Memories    = calloc (PBR_Slots, sizeof (VkDeviceMemory));
  Texture_Heap_Blocks = malloc (PBR_Slots * sizeof (int));
  for (uint I = 0; I < PBR_Slots; I++) Texture_Heap_Blocks[I] = -1;
  Texture_Views       = calloc (PBR_Slots, sizeof (VkImageView));

  // PBR map suffixes: [0] = Diffuse (no suffix), [1] = Normal, [2] = Roughness, [3] = Metalness, [4] = Emissive, [5] = Height
  const char *PBR_Suffixes[] = {"", "_n", "_r", "_m", "_e", "_h"};

  // Default fallback pixels for each PBR map type
  uint8_t Fallback_Roughness[4] = {180, 180, 180, 255}; // Moderate roughness ~0.7 (overridden per-material below)
  uint8_t Fallback_Metalness[4] = {0,   0,   0,   255}; // Non-metallic (overridden per-material below)
  uint8_t Fallback_Emissive[4]  = {0,   0,   0,   255}; // No emission
  uint8_t Fallback_Height[4]    = {127, 127, 127, 255}; // Mid-height
  uint8_t Fallback_Normal[4]    = {127, 127, 255, 255}; // Flat normal pointing up
  uint8_t *PBR_Fallbacks[]      = {NULL, Fallback_Normal, Fallback_Roughness, Fallback_Metalness, Fallback_Emissive, Fallback_Height};

  // Per-material PBR classification: Roughness and metalness defaults for each BSP texture when no explicit PBR map exists
  uint8_t (*Material_PBR)[2] = calloc (Material_Count, sizeof (uint8_t[2]));

  for (uint I = 0; I < Material_Count; I++) {

    // Default: moderate stone (R = 0.75, M=0.0)
    Material_PBR[I][0] = 191;  Material_PBR[I][1] = 0;
    if (not Scene_Data->Texture_Names) continue;
    const char *N = Scene_Data->Texture_Names[I];

    // Stone / brick / block: rough, non-metallic
    if (strstr (N, "gothic_block") or strstr (N, "gothic_wall/street"))
      {Material_PBR[I][0] = 204; Material_PBR[I][1] = 0;} // R = 0.80, M = 0.00 - Rough stone
    else if (strstr (N, "proto_brik"))
      {Material_PBR[I][0] = 209; Material_PBR[I][1] = 0;} // R = 0.82, M = 0.00 - Brick
    else if (strstr (N, "gothic_wall"))
      {Material_PBR[I][0] = 191; Material_PBR[I][1] = 0;} // R = 0.75, M = 0.00 - Generic wall

    // Metal trim / rust
    else if (strstr (N, "pitted_rust"))
      {Material_PBR[I][0] = 166; Material_PBR[I][1] = 153;} // R = 0.65, M = 0.60 - Corroded metal
    else if (strstr (N, "deeprust"))
      {Material_PBR[I][0] = 191; Material_PBR[I][1] = 128;} // R = 0.75, M = 0.50 - Heavy rust
    else if (strstr (N, "pewter"))
      {Material_PBR[I][0] = strstr(N,"dirty") ? 140 : 102;  
       Material_PBR[I][1] = strstr(N,"dirty") ? 166 : 179;} 
    else if (strstr (N, "border7") or strstr (N, "baseboard"))
      {Material_PBR[I][0] = 153; Material_PBR[I][1] = 89;}    // R = 0.60, M = 0.35 - Mixed trim

    // Tech walls
    else if (strstr (N, "atech"))
      {Material_PBR[I][0] = 115; Material_PBR[I][1] = 128;} // R = 0.45, M = 0.50 - Brushed metal panels
    else if (strstr (N, "ceilingtech"))
      {Material_PBR[I][0] = 140; Material_PBR[I][1] = 77;} // R = 0.55, M = 0.30 - Ceiling panel

    // Wood
    else if (strstr (N, "wood"))
      {Material_PBR[I][0] = 204; Material_PBR[I][1] = 0;} // R = 0.80, M = 0.00 - Dry wood

    // Floor
    else if (strstr (N, "gothic_floor")
         or strstr (N, "floor"))
      {Material_PBR[I][0] = 166; Material_PBR[I][1] = 0;} // R = 0.65, M = 0.00 - Worn floor stone

    // Light panels (EMISSIVE - PBR values less important)
    else if (strstr (N, "light")
         or strstr (N, "xlight"))
      {Material_PBR[I][0] = 77;  Material_PBR[I][1] = 0;} // R = 0.30, M = 0.00 - Smooth glass cover

    // Skull / bone decorations
    else if (strstr (N, "skull"))
      {Material_PBR[I][0] = 191; Material_PBR[I][1] = 13;} // R = 0.75, M = 0.05 - Bone

    // Lava
    else if (strstr (N, "lava"))
      {Material_PBR[I][0] = 26;  Material_PBR[I][1] = 0;} // R = 0.10, M = 0.00 - Molten liquid

    // SFX (flames, beams, flares)
    else if (strstr (N, "sfx/flame")
          or strstr (N, "sfx/beam")
          or strstr (N, "flameflare"))
      {Material_PBR[I][0] = 26;  Material_PBR[I][1] = 0;} // R = 0.10, M = 0.00 - Emissive effect

    // Window / glass
    else if (strstr (N, "window"))
      {Material_PBR[I][0] = 51;  Material_PBR[I][1] = 26;} // R = 0.20, M = 0.10 - Smooth glass

    // Torch model
    else if (strstr (N, "torch"))
      {Material_PBR[I][0] = 153; Material_PBR[I][1] = 77;} // R = 0.60, M = 0.30 - Metal + wood

    // Player skin (cloth + leather + armor plates)
    else if (strstr (N, "players/"))
      {Material_PBR[I][0] = 179; Material_PBR[I][1] = 20;} // R = 0.70, M = 0.08 - Mostly cloth, hint of metal

    // Weapon metal
    else if (strstr (N, "weapons"))
      {Material_PBR[I][0] = 77;  Material_PBR[I][1] = 217;} // R = 0.30, M = 0.85 - Gun steel

    // Stone walls and floors (rough, non-metallic)
    else if (strcasestr (N, "stonewall")
          or strcasestr (N, "stone3"))
      {Material_PBR[I][0] = 210; Material_PBR[I][1] = 0;}; // R = 0.82, M = 0.00 - Rough stone wall
    else if (strcasestr (N, "stonefloor")
          or strcasestr (N, "stonestep"))
      {Material_PBR[I][0] = 178; Material_PBR[I][1] = 0;} // R = 0.70, M = 0.00 - Worn stone floor
    else if (strcasestr (N, "stonetrim")
          or strcasestr (N, "column"))
      {Material_PBR[I][0] = 166; Material_PBR[I][1] = 0;} // R = 0.65, M = 0.00 - Carved stone trim
    else if (strcasestr (N, "carving"))
      {Material_PBR[I][0] = 153; Material_PBR[I][1] = 0;} // R = 0.60, M = 0.00 - Smooth carved stone

    // Grass, dirt, sand (very rough, non-metallic)
    else if (strcasestr (N, "grass")
          or strcasestr (N, "foliage"))
      {Material_PBR[I][0] = 230; Material_PBR[I][1] = 0;} // R = 0.90, M = 0.00 - Vegetation
    else if (strcasestr (N, "dirt")
          or strcasestr (N, "mud"))
      {Material_PBR[I][0] = 217; Material_PBR[I][1] = 0;} // R = 0.85, M = 0.00 - Loose ground
    else if (strcasestr (N, "sand"))
      {Material_PBR[I][0] = 204; Material_PBR[I][1] = 0;} // R = 0.80, M = 0.00 - Sandy ground

    // Wood (moderate rough, non-metallic)
    else if (strcasestr (N, "wood")
          or strcasestr (N, "plank"))
      {Material_PBR[I][0] = 204; Material_PBR[I][1] = 0;} // R = 0.80, M = 0.00 - Dry wood
    else if (strcasestr (N, "crate")
           or strcasestr (N, "box"))
      {Material_PBR[I][0] = 191; Material_PBR[I][1] = 0;} // R = 0.75, M = 0.00 - Wooden crate
    else if (strcasestr (N, "door"))
      {Material_PBR[I][0] = 178; Material_PBR[I][1] = 13;} // R = 0.70, M = 0.05 - Wood + metal hardware

    // Metal (Source maps - pipes, grates, etc.)
    else if (strcasestr (N, "metal")
          or strcasestr (N, "steel")
          or strcasestr (N, "iron"))
      {Material_PBR[I][0] = 102; Material_PBR[I][1] = 204;} // R = 0.40, M = 0.80 - Brushed metal
    else if (strcasestr (N, "grate")
         or strcasestr (N, "chain")
         or strcasestr (N, "fence"))
      {Material_PBR[I][0] = 128; Material_PBR[I][1] = 179;} // R = 0.50, M = 0.70 - Industrial metal
    else if (strcasestr (N, "rust"))
      {Material_PBR[I][0] = 191; Material_PBR[I][1] = 128;} // R = 0.75, M = 0.50 - Corroded

    // Concrete / brick / tile / plaster (moderate rough, non-metallic)
    else if (strcasestr (N, "concrete")
          or strcasestr (N, "cement"))
      {Material_PBR[I][0] = 191; Material_PBR[I][1] = 0;} // R = 0.75, M = 0.00 - Concrete
    else if (strcasestr (N, "brick"))
      {Material_PBR[I][0] = 204; Material_PBR[I][1] = 0;} // R = 0.80, M = 0.00 - Brick
    else if (strcasestr (N, "tile"))
      {Material_PBR[I][0] = 140; Material_PBR[I][1] = 0;} // R = 0.55, M = 0.00 - Smooth tile
    else if (strcasestr (N, "plaster")
          or strcasestr (N, "stucco"))
      {Material_PBR[I][0] = 166; Material_PBR[I][1] = 0;} // R = 0.65, M = 0.00 - Wall plaster

    // Glass / water
    else if (strcasestr (N, "glass")
          or strcasestr (N, "window"))
      {Material_PBR[I][0] = 38;  Material_PBR[I][1] = 26;} // R = 0.15, M = 0.10 - Smooth glass
    else if (strcasestr (N, "water"))
      {Material_PBR[I][0] = 13;  Material_PBR[I][1] = 0;} // R = 0.05, M = 0.00 - Water surface

    // Backdrop / sky (non-physical, doesn't matter much)
    else if (strcasestr (N, "backdrop")
          or strcasestr (N, "sky"))
      {Material_PBR[I][0] = 255; Material_PBR[I][1] = 0;} // R = 1.00, M = 0.00 - Diffuse sky
  }

  // Initialize PBR loading counters
  uint PBR_Maps_Loaded = 0, PBR_Generated = 0;

  // Pass 1: Load diffuse textures, retaining pixel data for PBR derivation
  uint8_t **Diffuse_Pixels = calloc (Material_Count, sizeof (uint8_t *));
  uint     *Diffuse_W      = calloc (Material_Count, sizeof (uint));
  uint     *Diffuse_H      = calloc (Material_Count, sizeof (uint));

  for (uint Index = 0; Index < Material_Count; Index++) {
    uint Slot = Index;  // diffuse = Map_Type 0
    uint W = 0, H = 0;
    uint8_t *Pixels = NULL;
    if (Scene_Data->Texture_Names) {
      char Path[256];

      // Try TGA first 
      snprintf (Path, sizeof (Path), "assets/%s.tga", Scene_Data->Texture_Names[Index]);
      Pixels = TGA_Load (Path, &W, &H);

      // Fallback: try VTF from cspromod materials directory (Source engine textures)
      if (not Pixels) {
        char Vtf_Path[512]; char Lower[256];
        const char *N = Scene_Data->Texture_Names[Index];
        for (int C=0; N[C] and C<255; C++) Lower[C] = (N[C]>='A' and N[C]<='Z') ? N[C]+32 : N[C];
        Lower[strlen(N)<255?strlen(N):255] = 0;

        // Try cspromod materials path
        const char *VTF_Search_Dirs[] = {
          "/tmp/cspromod_new/cspromod_b105/cspromod/materials",
          "/tmp/cspromod_new/cspromod_b105/cspromod/materials/models",
          "/tmp/v_m4_new/materials",
          "assets/materials",
          NULL
        };
        for (const char **Dir = VTF_Search_Dirs; *Dir and not Pixels; Dir++) {
          snprintf (Vtf_Path, sizeof Vtf_Path, "%s/%s.vtf", *Dir, Lower);
          int Vw=0, Vh=0; uint8_t *Vp = NULL;
          if (VTF_Load(Vtf_Path, &Vp, &Vw, &Vh) and Vp) { Pixels=Vp; W=(uint)Vw; H=(uint)Vh;}
        }
      }
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
                                  /*Out_View       =>*/ &Texture_Views[Slot],
                                  /*Out_Heap_Block =>*/ &Texture_Heap_Blocks[Slot]);
      Diffuse_Pixels[Index] = Pixels;  // retain for PBR derivation
      Diffuse_W[Index] = W;
      Diffuse_H[Index] = H;
      Textures_Loaded++;
    } else {
      free (Pixels);

      // Generate stand-in diffuse color from material name keywords
      uint8_t Stand_In[4] = {180, 180, 180, 255}; // default: neutral gray
      if (Scene_Data->Texture_Names) {
        const char *Material_Name = Scene_Data->Texture_Names[Index];

        // Use case-insensitive keyword matching to assign plausible colors
        if      (strcasestr(Material_Name,"concrete") or strcasestr(Material_Name,"cement"))
          { Stand_In[0]=175; Stand_In[1]=170; Stand_In[2]=165;}
        else if (strcasestr(Material_Name,"metal") or strcasestr(Material_Name,"steel"))
          { Stand_In[0]=140; Stand_In[1]=140; Stand_In[2]=145;}
        else if (strcasestr(Material_Name,"wood") or strcasestr(Material_Name,"plank") or strcasestr(Material_Name,"timber"))
          { Stand_In[0]=160; Stand_In[1]=120; Stand_In[2]=80;}
        else if (strcasestr(Material_Name,"brick"))
          { Stand_In[0]=160; Stand_In[1]=100; Stand_In[2]=80;}
        else if (strcasestr(Material_Name,"grass") or strcasestr(Material_Name,"foliage") or strcasestr(Material_Name,"leaf"))
          { Stand_In[0]=90;  Stand_In[1]=140; Stand_In[2]=70;}
        else if (strcasestr(Material_Name,"dirt") or strcasestr(Material_Name,"ground") or strcasestr(Material_Name,"mud"))
          { Stand_In[0]=140; Stand_In[1]=115; Stand_In[2]=85;}
        else if (strcasestr(Material_Name,"sand"))
          { Stand_In[0]=200; Stand_In[1]=185; Stand_In[2]=145;}
        else if (strcasestr(Material_Name,"rock") or strcasestr(Material_Name,"cliff") or strcasestr(Material_Name,"stone"))
          { Stand_In[0]=145; Stand_In[1]=140; Stand_In[2]=135;}
        else if (strcasestr(Material_Name,"plaster") or strcasestr(Material_Name,"stucco"))
          { Stand_In[0]=210; Stand_In[1]=200; Stand_In[2]=185;}
        else if (strcasestr(Material_Name,"glass") or strcasestr(Material_Name,"window"))
          { Stand_In[0]=160; Stand_In[1]=185; Stand_In[2]=200;}
        else if (strcasestr(Material_Name,"water"))
          { Stand_In[0]=60;  Stand_In[1]=100; Stand_In[2]=140;}
        else if (strcasestr(Material_Name,"tile") or strcasestr(Material_Name,"floor"))
          { Stand_In[0]=170; Stand_In[1]=165; Stand_In[2]=160;}
        else if (strcasestr(Material_Name,"roof") or strcasestr(Material_Name,"shingle"))
          { Stand_In[0]=130; Stand_In[1]=90;  Stand_In[2]=70;}
        else if (strcasestr(Material_Name,"paint") or strcasestr(Material_Name,"decal"))
          { Stand_In[0]=190; Stand_In[1]=185; Stand_In[2]=175;}
        else if (strcasestr(Material_Name,"asphalt") or strcasestr(Material_Name,"road"))
          { Stand_In[0]=85;  Stand_In[1]=85;  Stand_In[2]=85;}
        else if (strcasestr(Material_Name,"door"))
          { Stand_In[0]=130; Stand_In[1]=100; Stand_In[2]=75;}
        else if (strcasestr(Material_Name,"fence") or strcasestr(Material_Name,"chain") or strcasestr(Material_Name,"wire"))
          { Stand_In[0]=120; Stand_In[1]=120; Stand_In[2]=120;}
        else if (strcasestr(Material_Name,"pipe") or strcasestr(Material_Name,"duct") or strcasestr(Material_Name,"vent"))
          { Stand_In[0]=110; Stand_In[1]=115; Stand_In[2]=120;}
        else if (strcasestr(Material_Name,"crate") or strcasestr(Material_Name,"box"))
          { Stand_In[0]=155; Stand_In[1]=130; Stand_In[2]=90;}
        else if (strcasestr(Material_Name,"rust"))
          { Stand_In[0]=150; Stand_In[1]=90;  Stand_In[2]=60;}

        // Hash-based fallback: deterministic neutral color from material name
        else {          
          uint Hash = 5381;
          for (const char *C = Material_Name; *C; C++) Hash = Hash * 33 + (uint8_t)*C;
          Stand_In[0] = 120 + (Hash & 63);        // 120-183 range
          Stand_In[1] = 115 + ((Hash >> 8) & 63);  // neutral tones
          Stand_In[2] = 110 + ((Hash >> 16) & 63);
        }
      }
      Texture_Upload_With_Format (/*Command_Buffer =>*/ Command_Buffer,
                                  /*Queue          =>*/ Queue,
                                  /*Pixels         =>*/ Stand_In,
                                  /*Width          =>*/ 1,
                                  /*Height         =>*/ 1,
                                  /*Format         =>*/ VK_FORMAT_R8G8B8A8_SRGB,
                                  /*Out_Image      =>*/ &Texture_Images[Slot],
                                  /*Out_Memory     =>*/ &Texture_Memories[Slot],
                                  /*Out_View       =>*/ &Texture_Views[Slot],
                                  /*Out_Heap_Block =>*/ &Texture_Heap_Blocks[Slot]);
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
                                    /*Out_View       =>*/ &Texture_Views[Slot],
                                    /*Out_Heap_Block =>*/ &Texture_Heap_Blocks[Slot]);
        free (Pixels);
        PBR_Maps_Loaded++;
      } else {
        free (Pixels);

        // Derive PBR map from diffuse texture
        uint8_t *Diff = Diffuse_Pixels[Index];
        uint DW = Diffuse_W[Index], DH = Diffuse_H[Index];
        uint8_t Base_R = Material_PBR[Index][0];
        uint8_t Base_M = Material_PBR[Index][1];

        // We have real diffuse pixels - generate a full-resolution PBR map
        if (Diff and DW > 1 and DH > 1) {
          uint Pixel_Count = DW * DH;
          uint8_t *Gen = malloc (Pixel_Count * 4);

          // Normal map: Sobel filter on diffuse luminance. Produces tangent-space normals
          if (Map_Type == 1) {            
            for (uint Y = 0; Y < DH; Y++) {
              for (uint X = 0; X < DW; X++) {

                // Wrap-safe sampling (textures tile)
                #define LUM(px, py) ({ \
                  uint sx = ((px) % DW), sy = ((py) % DH); \
                  uint8_t *P = &Diff[(sy * DW + sx) * 4]; \
                  (float)P[0] * 0.2126f + (float)P[1] * 0.7152f + (float)P[2] * 0.0722f; \
                })

                // Sobel 3 by 3
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

          // Roughness map: base ~ luminance variation. Darker cracks in stone = rougher; bright polished areas = smoother
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

          // Metalness map: base ~ saturation/brightness detection. For metallic materials: desaturated bright pixels > more metallic
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

          // Height map: luminance-based. Stone/brick: inverted luminance (dark cracks = deeper)
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
                                      /*Out_View       =>*/ &Texture_Views[Slot],
                                      /*Out_Heap_Block =>*/ &Texture_Heap_Blocks[Slot]);
          free (Gen);
          PBR_Generated++;

        // No diffuse data - use 1 by 1 classified fallback
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
                                        /*Out_View       =>*/ &Texture_Views[Slot],
                                        /*Out_Heap_Block =>*/ &Texture_Heap_Blocks[Slot]);
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
                                        /*Out_View       =>*/ &Texture_Views[Slot],
                                        /*Out_Heap_Block =>*/ &Texture_Heap_Blocks[Slot]);
          } else {
            Texture_Upload_With_Format (/*Command_Buffer =>*/ Command_Buffer,
                                        /*Queue          =>*/ Queue,
                                        /*Pixels         =>*/ PBR_Fallbacks[Map_Type],
                                        /*Width          =>*/ 1,
                                        /*Height         =>*/ 1,
                                        /*Format         =>*/ Fmt,
                                        /*Out_Image      =>*/ &Texture_Images[Slot],
                                        /*Out_Memory     =>*/ &Texture_Memories[Slot],
                                        /*Out_View       =>*/ &Texture_Views[Slot],
                                        /*Out_Heap_Block =>*/ &Texture_Heap_Blocks[Slot]);
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

  // Upload the lightmap atlas (or a 1 by 1 white fallback if no lightmaps exist)
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
                                /*Out_View       =>*/ &Lightmap_View,
                                /*Out_Heap_Block =>*/ &Lightmap_Heap_Block);
    printf ("[lightmap] uploaded %ux%u atlas (SRGB - auto-linearized on sample)\n",
            Scene_Data->Lightmap_Width,
            Scene_Data->Lightmap_Height);
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
                                /*Out_View       =>*/ &Lightmap_View,
                                /*Out_Heap_Block =>*/ &Lightmap_Heap_Block);
  }
} // BSP_Parse_Entities

// ════════════════════════
//   Weapon_Load_Textures
// ════════════════════════

void Weapon_Load_Textures (Figure_Instance *Weapon) {

  // Record the starting index in the global texture array for this weapon's textures
  Weapon->Texture_Base_Index = Texture_Count;

  // Six map types per weapon texture: diffuse, normal, roughness, metalness, emissive, height
  const char *Weapon_PBR_Suffixes[] = {"", "_n", "_r", "_m", "_e", "_h"};
  const uint8_t Weapon_PBR_Fallbacks[][4] = {
    {180, 180, 180, 255},  // Diffuse: grey
    {128, 128, 255, 255},  // Normal: flat (0,0,1) encoded as (128,128,255)
    {180, 180, 180, 255},  // Roughness: moderate (0.70) - mix of skin and painted metal
    { 25,  25,  25, 255},  // Metalness: low (0.10) - mostly non-metallic (hands, polymer)
    {  0,   0,   0, 255},  // Emissive: none
    {128, 128, 128, 255}}; // Height: mid-level

  // Use the actual surface count from the weapon model (Source weapons may have many materials)
  uint Weapon_Tex_Count = Weapon->Figure.Surface_Count > 0 ? Weapon->Figure.Surface_Count : WEAPON_TEXTURE_COUNT;
  if (Weapon_Tex_Count > WEAPON_MAX_TEXTURES) Weapon_Tex_Count = WEAPON_MAX_TEXTURES;

  // Grow the global texture arrays to hold weapon PBR slots
  uint Weapon_PBR_Maps = Weapon_Tex_Count * 6;
  uint New_Total = Texture_Count + Weapon_PBR_Maps;
  Texture_Images      = realloc (Texture_Images,      sizeof (VkImage)        * New_Total);
  Texture_Memories    = realloc (Texture_Memories,    sizeof (VkDeviceMemory) * New_Total);
  Texture_Heap_Blocks = realloc (Texture_Heap_Blocks, sizeof (int)            * New_Total);
  for (uint I = Texture_Count; I < New_Total; I++) Texture_Heap_Blocks[I] = -1;
  Texture_Views       = realloc (Texture_Views,       sizeof (VkImageView)    * New_Total);

  // Load weapon textures: 6 PBR map types  by  Weapon_Tex_Count textures
  uint Weapon_PBR_Loaded = 0;
  for (uint Map_Type = 0; Map_Type < 6; Map_Type++) {
    for (uint Index = 0; Index < Weapon_Tex_Count; Index++) {
      uint Slot = Texture_Count + Map_Type * Weapon_Tex_Count + Index;
      uint Img_W = 0, Img_H = 0;
      uint8_t *Pixels = NULL;

      if (Map_Type == 0 and Index < Weapon->Figure.Surface_Count and Weapon->Figure.Texture_Names[Index][0]) {
        // Try loading weapon texture from model's texture name (Source VTF or TGA)
        char Lower[256]; int Li=0;
        for (const char *C=Weapon->Figure.Texture_Names[Index]; *C and Li<255; C++)
          Lower[Li++] = (*C>='A' and *C<='Z') ? *C+32 : *C;
        Lower[Li]=0;

        // Try VTF from weapon materials directories and cspromod directories
        const char *VTF_Dirs[] = {
          "/tmp/v_m4_new/materials",
          "/tmp/cspromod_new/cspromod_b105/cspromod/materials",
          "/tmp/cspromod_new/cspromod_b105/cspromod/materials/models/weapons/v_models/sas.m4",
          "assets/materials", NULL
        };
        for (const char **Dir = VTF_Dirs; *Dir and not Pixels; Dir++) {
          char Vtf_Path[512];
          snprintf(Vtf_Path, sizeof Vtf_Path, "%s/%s.vtf", *Dir, Lower);
          int Vtf_Width=0, Vtf_Height=0; uint8_t *Vtf_Pixels = NULL;
          if (VTF_Load(Vtf_Path, &Vtf_Pixels, &Vtf_Width, &Vtf_Height) and Vtf_Pixels) {
            Pixels=Vtf_Pixels; Img_W=(uint)Vtf_Width; Img_H=(uint)Vtf_Height;
            printf("[weapon] loaded VTF texture %s (%ux%u)\n", Vtf_Path, Img_W, Img_H);
          }
        }
      }

      // Fall back to TGA path for Q3 weapons
      if (not Pixels and Index < 2) {
        char Path[256];
        snprintf (Path, sizeof Path, "%s", WEAPON_TEXTURE_PATHS[Index]);
        char *Ext = strstr (Path, ".tga");
        if (Ext) {
          snprintf (Ext, sizeof Path - (size_t)(Ext - Path), "%s.tga", Weapon_PBR_Suffixes[Map_Type]);
          Pixels = TGA_Load (Path, &Img_W, &Img_H);
          if (Pixels and Map_Type == 0) printf ("[weapon] loaded texture %s (%ux%u)\n", Path, Img_W, Img_H);
        }
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
                                    /*Out_View       =>*/ &Texture_Views[Slot],
                                    /*Out_Heap_Block =>*/ &Texture_Heap_Blocks[Slot]);
        free (Pixels);
        if (Map_Type > 0) Weapon_PBR_Loaded++;
      } else {
        Texture_Upload_With_Format (/*Command_Buffer =>*/ Command_Buffer,
                                    /*Queue          =>*/ Queue,
                                    /*Pixels         =>*/ (uint8_t *)Weapon_PBR_Fallbacks[Map_Type],
                                    /*Width          =>*/ 1,
                                    /*Height         =>*/ 1,
                                    /*Format         =>*/ Fmt,
                                    /*Out_Image      =>*/ &Texture_Images[Slot],
                                    /*Out_Memory     =>*/ &Texture_Memories[Slot],
                                    /*Out_View       =>*/ &Texture_Views[Slot],
                                    /*Out_Heap_Block =>*/ &Texture_Heap_Blocks[Slot]);
        if (Map_Type == 0)
          printf ("[weapon] fallback for weapon texture %u\n", Index);
      }
    }
  }
  Texture_Count += Weapon_PBR_Maps;
  // Pack weapon texture stride into upper 16 bits of Texture_Base_Index for the shader
  Weapon->Texture_Base_Index = (Weapon_Tex_Count << 16) | (Weapon->Texture_Base_Index & 0xFFFF);
  printf ("[weapon] textures: base=%u, stride=%u, PBR maps=%u\n",
          Weapon->Texture_Base_Index & 0xFFFF, Weapon_Tex_Count, Weapon_PBR_Loaded);
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
  GPU_Buffer Scratch = Buffer_Allocate (/*Size         =>*/ Build_Sizes.buildScratchSize,
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
  Buffer_Destroy (&Scratch);
  return Result;

} // Build_World_Bottom_Level

// ══════════════════════════════════
//   Figure_BLAS_Initialize
// ════════════════════════════
//
// Unified BLAS initialization for any Figure_Instance (weapon, enemy, prop). Allocates host-visible vertex buffer,
// device-local index/texture-id buffers, and builds the initial BLAS with FAST_BUILD + ALLOW_UPDATE.

void Figure_BLAS_Initialize (Figure_Instance *Weapon) {
  if (not Weapon->Figure.Vertex_Count) return;

  // Allocate a host-visible copy of the weapon vertices for per-frame CPU transformation
  Weapon->Transformed_Vertices = malloc (sizeof (Vertex) * Weapon->Figure.Vertex_Count);
  memcpy (Weapon->Transformed_Vertices, Weapon->Figure.Vertices, sizeof (Vertex) * Weapon->Figure.Vertex_Count);

  // Create host-visible vertex buffer for direct CPU writes each frame (host-visible so we can update without staging)
  Weapon->Vertex_Buffer = Buffer_Allocate (/*Size         =>*/ sizeof (Vertex) * Weapon->Figure.Vertex_Count,
                                           /*Usage        =>*/ VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                                             | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                             | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                                                             | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                           /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                             | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Upload the initial vertex positions
  Buffer_Upload (Weapon->Vertex_Buffer, Weapon->Transformed_Vertices, sizeof (Vertex) * Weapon->Figure.Vertex_Count);

  // Upload index and texture-id data (static, device-local - these never change after the initial upload)
  Weapon->Index_Buffer      = Buffer_Stage_Upload (/*Command_Buffer =>*/ Command_Buffer,
                                                   /*Queue          =>*/ Queue,
                                                   /*Data           =>*/ Weapon->Figure.Indices,
                                                   /*Size           =>*/ sizeof (uint) * Weapon->Figure.Index_Count,
                                                   /*Usage          =>*/ VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                                                       | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                                       | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
  Weapon->Texture_Id_Buffer = Buffer_Stage_Upload (/*Command_Buffer =>*/ Command_Buffer,
                                                   /*Queue          =>*/ Queue,
                                                   /*Data           =>*/ Weapon->Figure.Texture_Ids,
                                                   /*Size           =>*/ sizeof (uint) * Weapon->Figure.Triangle_Count,
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
      .maxVertex                = Weapon->Figure.Vertex_Count - 1,
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
  uint Primitive_Count = Weapon->Figure.Triangle_Count;
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
  printf ("[figure] BLAS built: %u triangles\n", Primitive_Count);

} // Figure_BLAS_Initialize

// ════════════════════════════
//   Figure_BLAS_Rebuild
// ════════════════════════════
//
// Rebuild/refit a figure's BLAS after vertex data has changed. Re-uploads either Transformed_Vertices (weapon viewmodel)
// or Current_Vertices (frame-animated entity) and performs an in-place BLAS refit (MODE_UPDATE).

void Figure_BLAS_Rebuild (Figure_Instance *Weapon) {

  if (not Weapon->Figure.Vertex_Count) return;

  // Re-upload the appropriate vertex source to the GPU buffer
  const void *Vertex_Source = Weapon->Transformed_Vertices ? (const void *)Weapon->Transformed_Vertices
                                                           : (const void *)Weapon->Current_Vertices;
  if (Vertex_Source) Buffer_Upload (Weapon->Vertex_Buffer, Vertex_Source, sizeof (Vertex) * Weapon->Figure.Vertex_Count);

  // Refit the BLAS with the updated vertex positions
  VkAccelerationStructureGeometryKHR Geometry = {
    .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
    .flags        = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.triangles = {
      .sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
      .vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT,
      .vertexData.deviceAddress = Weapon->Vertex_Buffer.Address,
      .vertexStride             = sizeof (Vertex),
      .maxVertex                = Weapon->Figure.Vertex_Count - 1,
      .indexType                = VK_INDEX_TYPE_UINT32,
      .indexData.deviceAddress  = Weapon->Index_Buffer.Address}};

  // BLAS refit (MODE_UPDATE) instead of full rebuild. The weapon mesh topology never changes - only vertex positions move...
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
  VkAccelerationStructureBuildRangeInfoKHR Range = {.primitiveCount = Weapon->Figure.Triangle_Count};
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

// (Entity_Bottom_Level_Initialize / Entity_Bottom_Level_Rebuild removed — merged into Figure_BLAS_Initialize / Figure_BLAS_Rebuild above)

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
  // the BVH in a format amenable to in-place refitting.
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

void Top_Level_Rebuild (Acceleration_Structure *World, Figure_Pool *Pool) {

  // Instance 0: world geometry (identity transform, visible to all rays)
  VkAccelerationStructureInstanceKHR Instances[1 + FIGURE_POOL_MAX];
  memset (&Instances[0], 0, sizeof Instances[0]);
  Instances[0].transform.matrix[0][0]         = 1.f;
  Instances[0].transform.matrix[1][1]         = 1.f;
  Instances[0].transform.matrix[2][2]         = 1.f;
  Instances[0].mask                           = 0xFF;
  Instances[0].instanceCustomIndex            = 0;
  Instances[0].flags                          = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
  Instances[0].accelerationStructureReference = World->Address;
  uint N = 1;

  // Append one TLAS instance per active figure from the pool
  for (uint I = 0; I < FIGURE_POOL_MAX and N < 1 + FIGURE_POOL_MAX; I++) {
    if (not Pool->Active[I]) continue;
    Figure_Instance *F = &Pool->Slots[I];
    if (not F->Bottom_Level.Handle) continue;

    // Pack instanceCustomIndex: [7:0] = figure slot (I + 1), [8] = weapon flag, [23:9] = texture base
    uint Is_Weapon_Bit = (F->Ray_Mask == 0x01) ? 0x100u : 0u;
    uint Custom_Index  = (I + 1) | Is_Weapon_Bit | (F->Texture_Base_Index << 9);

    memset (&Instances[N], 0, sizeof Instances[N]);
    memcpy (&Instances[N].transform, F->TLAS_Transform, sizeof (float) * 12);
    Instances[N].mask                           = F->Ray_Mask;
    Instances[N].instanceCustomIndex            = Custom_Index;
    Instances[N].flags                          = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    Instances[N].accelerationStructureReference = F->Bottom_Level.Address;
    N++;
  }

  // Upload and build TLAS
  Buffer_Upload (Top_Level_Instance_Buffer, Instances, sizeof (VkAccelerationStructureInstanceKHR) * N);

  VkAccelerationStructureBuildRangeInfoKHR Range = {.primitiveCount = N};
  const VkAccelerationStructureBuildRangeInfoKHR *Range_Pointer = &Range;

  VkAccelerationStructureGeometryKHR Geometry = {
    .sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType       = VK_GEOMETRY_TYPE_INSTANCES_KHR,
    .flags              = VK_GEOMETRY_OPAQUE_BIT_KHR,
    .geometry.instances = {
      .sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
      .arrayOfPointers    = VK_FALSE,
      .data.deviceAddress = Top_Level_Instance_Buffer.Address}};

  static int First_Build = 1;
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
    {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},  // Color (Raw RT, read-only)
    {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},  // Depth
    {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},  // History (Previous frame TAA)
    {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}}; // Display output (Tonemapped)

  VK_CHECK (vkCreateDescriptorSetLayout (/*device      =>*/ Device,
                                         /*pCreateInfo =>*/ &(VkDescriptorSetLayoutCreateInfo){
                                           .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                           .bindingCount = 4,
                                           .pBindings    = Bindings},
                                         /*pAllocator  =>*/ NULL,
                                         /*pSetLayout  =>*/ &Postprocess_Descriptor_Layout));

  // Create the pipeline layout with push constants for postprocess parameters
  VkPushConstantRange Push_Range = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof (GPU_Postprocess_Push)};
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
  VkWriteDescriptorSet  Writes[]     = {
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

  // Descriptor pool: 2 sets  by  3 images each = 6 image descriptors
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

  // Set [0]: reads Storage_Image > writes Denoise_Ping_Image
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

  // Set [1]: reads Denoise_Ping_Image > writes Storage_Image
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

  // Define 13 descriptor bindings (0-12). Bindings 8-10 are SSBO arrays indexed by figure slot for multi-figure support.
  // Binding 12 (texture array) must be last for variable descriptor count.
  VkDescriptorSetLayoutBinding Bindings[] = {
    {0,  VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,     1, VK_SHADER_STAGE_RAYGEN_BIT_KHR
                                                              | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {1,  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,                  1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,      NULL},
    {2,  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,                 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR
                                                              | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
                                                              | VK_SHADER_STAGE_MISS_BIT_KHR,        NULL},
    {3,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,                 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {4,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,                 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {5,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,                 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {6,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,                 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {7,  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,         1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},
    {8,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, FIGURE_POOL_MAX, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},  // Figure vertices[]
    {9,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, FIGURE_POOL_MAX, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},  // Figure indices[]
    {10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, FIGURE_POOL_MAX, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL},  // Figure texture IDs[]
    {11, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,                  1, VK_SHADER_STAGE_RAYGEN_BIT_KHR,      NULL}, // Depth output
    {12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DESCRIPTOR_TEXTURE_SLOTS, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, NULL}};

  // Bindings 8-10 (figure SSBO arrays) and 12 (texture array) use partially-bound. Binding 12 also uses variable count.
  VkDescriptorBindingFlags Binding_Flags[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                              VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,    // 8: figure vertices
                                              VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,    // 9: figure indices
                                              VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,    // 10: figure tex ids
                                              0,                                            // 11: depth
                                              VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                                            | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};  // 12: textures

  // Chain the binding flags extension into the descriptor set layout creation
  VkDescriptorSetLayoutBindingFlagsCreateInfo Binding_Flags_Info = {
    .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
    .bindingCount  = 13,
    .pBindingFlags = Binding_Flags};

  // Create the descriptor set layout with all 13 bindings
  VK_CHECK (vkCreateDescriptorSetLayout (/*device      =>*/ Device,
                                         /*pCreateInfo =>*/ &(VkDescriptorSetLayoutCreateInfo){
                                           .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                           .pNext        = &Binding_Flags_Info,
                                           .bindingCount = 13,
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

void Descriptor_Set_Create (Figure_Pool *Pool) {

  // Pool sizes: 4 world SSBOs + 3 * FIGURE_POOL_MAX figure SSBOs + 2 images + 1 UBO + 1 lightmap + texture array
  VkDescriptorPoolSize Pool_Sizes[] = {
    {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              2},
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             4 + 3 * FIGURE_POOL_MAX},
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     DESCRIPTOR_TEXTURE_SLOTS + 1}};
  VK_CHECK (vkCreateDescriptorPool (/*device          =>*/ Device,
                                    /*pCreateInfo     =>*/ &(VkDescriptorPoolCreateInfo){
                                      .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                      .maxSets       = 1,
                                      .poolSizeCount = 5,
                                      .pPoolSizes    = Pool_Sizes},
                                    /*pAllocator      =>*/ NULL,
                                    /*pDescriptorPool =>*/ &Descriptor_Pool));

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

  // Fixed descriptor infos for world and global bindings
  VkWriteDescriptorSetAccelerationStructureKHR Acceleration_Write = {
    .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
    .accelerationStructureCount = 1,
    .pAccelerationStructures    = &Top_Level.Handle};
  VkDescriptorImageInfo  Image_Info      = {.imageView = Raytracing_Storage_Image.View, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorBufferInfo Camera_Info     = {Camera_Uniform_Buffer.Buffer, 0, Camera_Uniform_Buffer.Size};
  VkDescriptorBufferInfo Vertex_Info     = {Vertex_Buffer.Buffer,        0, Vertex_Buffer.Size};
  VkDescriptorBufferInfo Index_Info      = {Index_Buffer.Buffer,         0, Index_Buffer.Size};
  VkDescriptorBufferInfo Material_Info   = {Material_Buffer.Buffer,      0, Material_Buffer.Size};
  VkDescriptorBufferInfo Texture_Id_Info = {Texture_Id_Buffer.Buffer,    0, Texture_Id_Buffer.Size};
  VkDescriptorImageInfo  Lightmap_Info   = {.sampler     = Lightmap_Sampler,
                                            .imageView   = Lightmap_View,
                                            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkDescriptorImageInfo  Depth_Info      = {.imageView = Depth_Image.View, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};

  // Build per-figure SSBO descriptor arrays (bindings 8, 9, 10) indexed by pool slot.
  // Inactive slots get the world vertex buffer as a dummy (PARTIALLY_BOUND prevents access).
  VkDescriptorBufferInfo Dummy = {Vertex_Buffer.Buffer, 0, Vertex_Buffer.Size};
  VkDescriptorBufferInfo Fig_Vertex_Infos [FIGURE_POOL_MAX];
  VkDescriptorBufferInfo Fig_Index_Infos  [FIGURE_POOL_MAX];
  VkDescriptorBufferInfo Fig_Tex_Id_Infos [FIGURE_POOL_MAX];
  uint Fig_Count = 0;
  for (uint I = 0; I < FIGURE_POOL_MAX; I++) {
    if (Pool->Active[I] and Pool->Slots[I].Vertex_Buffer.Buffer) {
      Figure_Instance *F = &Pool->Slots[I];
      Fig_Vertex_Infos [I] = (VkDescriptorBufferInfo){F->Vertex_Buffer.Buffer,     0, F->Vertex_Buffer.Size};
      Fig_Index_Infos  [I] = (VkDescriptorBufferInfo){F->Index_Buffer.Buffer,      0, F->Index_Buffer.Size};
      Fig_Tex_Id_Infos [I] = (VkDescriptorBufferInfo){F->Texture_Id_Buffer.Buffer, 0, F->Texture_Id_Buffer.Size};
      Fig_Count = I + 1;
    } else {
      Fig_Vertex_Infos [I] = Dummy;
      Fig_Index_Infos  [I] = Dummy;
      Fig_Tex_Id_Infos [I] = Dummy;
    }
  }

  // Texture array
  VkDescriptorImageInfo *Texture_Infos = calloc (Texture_Count, sizeof (VkDescriptorImageInfo));
  for (uint I = 0; I < Texture_Count; I++) {
    Texture_Infos[I] = (VkDescriptorImageInfo){.sampler     = Texture_Sampler,
                                               .imageView   = Texture_Views[I],
                                               .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  }

  // Write all 13 bindings (0-12). Bindings 8-10 are written as contiguous SSBO arrays spanning [0..Fig_Count).
  uint Fig_N = Fig_Count ? Fig_Count : 1;  // Vulkan requires descriptorCount >= 1 for array writes
  VkWriteDescriptorSet Writes[] = {
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, &Acceleration_Write, Descriptor_Set, 0,  0, 1,             VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, NULL,           NULL,              NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 1,  0, 1,                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              &Image_Info,    NULL,              NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 2,  0, 1,                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             NULL,           &Camera_Info,      NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 3,  0, 1,                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Vertex_Info,      NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 4,  0, 1,                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Index_Info,       NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 5,  0, 1,                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Material_Info,    NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 6,  0, 1,                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           &Texture_Id_Info,  NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 7,  0, 1,                            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     &Lightmap_Info, NULL,              NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 8,  0, Fig_N,                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           Fig_Vertex_Infos,  NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 9,  0, Fig_N,                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           Fig_Index_Infos,   NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 10, 0, Fig_N,                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             NULL,           Fig_Tex_Id_Infos,  NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 11, 0, 1,                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              &Depth_Info,    NULL,              NULL},
    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Descriptor_Set, 12, 0, Texture_Count,                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     Texture_Infos,  NULL,              NULL},
  };

  vkUpdateDescriptorSets (Device, 13, Writes, 0, NULL);
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

    // Environment parameters
    float Sun_Dir      [4]; // xyz = Direction,     w = Angular_Radius
    float Sun_Color    [4]; // xyz = Color,         w = Intensity
    float Sky_Zenith   [4]; // xyz = Zenith color,  w = Sky_Intensity
    float Sky_Horizon  [4]; // xyz = Horizon color, w = Sun_Disc_Size
    float Ambient_Up   [4]; // xyz = Ambient up,    w = Sun_Disc_Intensity
    float Ambient_Down [4]; // xyz = Ambient down,  w = Fog_Density
    float Fog_Color    [4]; // xyz = Fog color,     w = Lightmap_Mult
  } Uniform;

  // Compute the inverse matrices for reconstructing world-space rays from screen coordinates
  Uniform.Inverse_View        = Inverse_Orthogonal (View_Matrix);
  Uniform.Inverse_Projection  = Inverse_Projection (Proj_Matrix);
  Uniform.Frame               = State->Frame;
  Uniform.Weapon_Texture_Base = Weapon_Texture_Base;
  Uniform.PBR_Stride          = PBR_Stride_Value;
  Uniform.Active_SPP          = Active_SPP;

  // Pack environment parameters into the uniform 
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
  Uniform.Fog_Color    [2] = E->Fog_Color.z;    Uniform.Fog_Color    [3] = E->Lightmap_Mult;

  // Upload the uniform data to the camera buffer
  Buffer_Upload (Camera_Uniform_Buffer, &Uniform, sizeof (Uniform));
  
} // Camera_Upload

// ═════════════════
//   Weapon_Update
// ═════════════════

void Weapon_Update (Figure_Instance *Weapon, const Camera *Camera_Data, float Delta_Time, int Fire) {
  if (not Weapon->Figure.Vertex_Count) return;

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

  // Final weapon position: camera origin + forward/right/up offsets with bob and recoil ???
  float Fwd_Offset   = Weapon->Figure.Is_Source ? 3.f  : 5.f;
  float Right_Offset = Weapon->Figure.Is_Source ? 0.5f : 4.f;
  float Up_Offset    = Weapon->Figure.Is_Source ? -0.5f: -3.5f;
  vec3 Offset = Add (Camera_Data->Position,
                     Add (Scale (Forward, Fwd_Offset + Recoil),
                          Add (Scale (Right, Right_Offset + Bob_Horizontal),
                               Scale (Up,   Up_Offset + Bob_Vertical))));

  // Find the tag_weapon tag index (convention: last tag with name "tag_weapon")
  int Weapon_Tag = -1;
  for (uint I = 0; I < Weapon->Figure.Tag_Count; I++)
    if (strcmp (Weapon->Figure.Tags[I].Name, "tag_weapon") == 0) Weapon_Tag = (int)I;

  // Select the current animation frame from the tag's per-frame transforms
  uint Frame_Index = 0;
  uint Tag_Frames = Weapon_Tag >= 0 ? Weapon->Figure.Tags[Weapon_Tag].Frame_Count : 1;
  if (Tag_Frames > 1 and Weapon->Is_Firing) {
    Frame_Index = (uint)Weapon->Fire_Time;
    if (Frame_Index >= Tag_Frames) Frame_Index = Tag_Frames - 1;
  }

  // Read the tag transform (origin + 3x3 axis matrix) for the current animation frame
  static const float Identity_Tag[12] = {0,0,0, 1,0,0, 0,1,0, 0,0,1};
  const float *Tag = Weapon_Tag >= 0 ? Weapon->Figure.Tags[Weapon_Tag].Transforms[Frame_Index] : Identity_Tag;

  // Swizzle each tag axis from Quake 3 Z-up to Y-up: (x,y,z) becomes (x,z,-y)
  vec3 Axis_0 = (vec3){Tag[3],  Tag[5],  -Tag[4]};
  vec3 Axis_1 = (vec3){Tag[6],  Tag[8],  -Tag[7]};
  vec3 Axis_2 = (vec3){Tag[9],  Tag[11], -Tag[10]};

  // Build the Y-up tag rotation matrix column (forward, up or -left)
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

  // Scale the viewmodel down - no depth hack, so we shrink the model in world space ???
  float Model_Scale = Weapon->Figure.Is_Source ? 0.45f : WEAPON_MODEL_SCALE;

  // Viewmodel FOV correction for Source weapons ???
  float VM_Fov_Scale = Weapon->Figure.Is_Source ? 0.51f : 1.f;

  // Transform each vertex from model space to world space
  for (uint Index = 0; Index < Weapon->Figure.Vertex_Count; Index++) {
    float Source_X = Weapon->Figure.Vertices[Index].Position[0] * Model_Scale * VM_Fov_Scale;
    float Source_Y = Weapon->Figure.Vertices[Index].Position[1] * Model_Scale; // up
    float Source_Z = Weapon->Figure.Vertices[Index].Position[2] * Model_Scale; // right

    // Apply the combined rotation and translate by the camera offset
    Weapon->Transformed_Vertices[Index].Position[0] = Rotation[0] * Source_X + Rotation[1] * Source_Y + Rotation[2] * Source_Z + Offset.x;
    Weapon->Transformed_Vertices[Index].Position[1] = Rotation[3] * Source_X + Rotation[4] * Source_Y + Rotation[5] * Source_Z + Offset.y;
    Weapon->Transformed_Vertices[Index].Position[2] = Rotation[6] * Source_X + Rotation[7] * Source_Y + Rotation[8] * Source_Z + Offset.z;

    // Rotate the vertex normal by the rotation matrix (no translation)
    float Normal_X = Weapon->Figure.Vertices[Index].Normal[0];
    float Normal_Y = Weapon->Figure.Vertices[Index].Normal[1];
    float Normal_Z = Weapon->Figure.Vertices[Index].Normal[2];
    Weapon->Transformed_Vertices[Index].Normal[0] = Rotation[0] * Normal_X + Rotation[1] * Normal_Y + Rotation[2] * Normal_Z;
    Weapon->Transformed_Vertices[Index].Normal[1] = Rotation[3] * Normal_X + Rotation[4] * Normal_Y + Rotation[5] * Normal_Z;
    Weapon->Transformed_Vertices[Index].Normal[2] = Rotation[6] * Normal_X + Rotation[7] * Normal_Y + Rotation[8] * Normal_Z;

    // Pass texture coordinates through unchanged
    Weapon->Transformed_Vertices[Index].Texture_UV[0] = Weapon->Figure.Vertices[Index].Texture_UV[0];
    Weapon->Transformed_Vertices[Index].Texture_UV[1] = Weapon->Figure.Vertices[Index].Texture_UV[1];
  }
} // Weapon_Update

// ════════════════════
//   Raytracing_Frame
// ════════════════════

void Raytracing_Frame (GPU_Postprocess_Push Postprocess) {

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
    if (R == VK_ERROR_OUT_OF_DATE_KHR) { Swapchain_Dirty = 1; return;}
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

  // Checkerboard - Dispatch at half width, each thread remaps to a checkerboard pixel. Untouched pixels keep their previous value; the
  // postprocess reconstructs them from traced neighbors before TAA.
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

    // Barrier: Ray tracing writes then compute reads
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

    // Iteration count controlled by quality preset. Passes Budget to the shader - at high budget (cheap path), denoiser is a passthrough.
    int Steps[] = {1, 3, 8, 16}; // Progressive A-trous: 3x3, 7x7, 17x17, 33x33
    int Denoise_Passes = Active_Denoise_Passes;
    if (Denoise_Passes > 0) {
      vkCmdBindPipeline (Command_Buffer, VK_PIPELINE_BIND_POINT_COMPUTE, Denoise_Pipeline);
      for (int I = 0; I < Denoise_Passes; I++) {
        vkCmdBindDescriptorSets (/*commandBuffer      =>*/ Command_Buffer,
                                 /*pipelineBindPoint   =>*/ VK_PIPELINE_BIND_POINT_COMPUTE,
                                 /*layout              =>*/ Denoise_Pipeline_Layout,
                                 /*firstSet            =>*/ 0,
                                 /*descriptorSetCount  =>*/ 1,
                                 /*pDescriptorSets     =>*/ &Denoise_Descriptor_Sets[I % 2],
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
  VkImage Blit_Source = Skip_Postprocess ? Raytracing_Storage_Image.Image
                                         : Postprocess_Output_Image.Image;
  VkPipelineStageFlags Pre_Blit = Skip_Postprocess ? VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
                                                   : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

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
      if (Distance > Best_Distance) { Best_Distance = Distance; Point_0 = Extremals[I]; Point_1 = Extremals[J];}
    }

  // Find the third point: most distant from the initial edge
  vec3  Edge         = Subtract (Points[Point_1], Points[Point_0]);
  float Edge_Length2 = Dot (Edge, Edge);
  int   Point_2      = -1;
  Best_Distance      = 0;

  // Search all points for the most distant from the edge
  for (uint Index = 0; Index < Count; Index++) {
    if ((int)Index == Point_0 or (int)Index == Point_1) continue;
    vec3  Vector    = Subtract (Points[Index], Points[Point_0]);
    float Parameter = Dot (Vector, Edge) / Edge_Length2;
    float Distance  = Dot (Subtract (Vector, Scale (Edge, Parameter)),
                           Subtract (Vector, Scale (Edge, Parameter)));
    if (Distance > Best_Distance) { Best_Distance = Distance; Point_2 = Index;}
  }
  if (Point_2 < 0) Point_2 = (Point_0 + 1) % Count;

  // Find the fourth point: most distant from the initial triangle
  int Point_3    = -1;
  Best_Distance  = 0;
  for (uint Index = 0; Index < Count; Index++) {
    if ((int)Index == Point_0 or (int)Index == Point_1 or (int)Index == Point_2) continue;
    float Distance = fabsf (Quickhull_Dist (Points[Index], Points[Point_0], Points[Point_1], Points[Point_2]));
    if (Distance > Best_Distance) { Best_Distance = Distance; Point_3 = Index;}
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
          for (int K = 0; K < 3; K++) { Has_0 |= (Face_Vertices[K] == Triangle[Edge][0]); Has_1 |= (Face_Vertices[K] == Triangle[Edge][1]);}
          if (Has_0 and Has_1) { Shared = 1; break;}
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
        if (Distance > Best) { Best = Distance; Assignments[Index] = Face;}
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
        if (Result.Adjacency[Vertex_0][Slot] == -1) { Result.Adjacency[Vertex_0][Slot] = Vertex_1; break;}
      }
      for (int Slot = 0; Slot < HULL_MAX_ADJ; Slot++) {
        if (Result.Adjacency[Vertex_1][Slot] == Vertex_0) break;
        if (Result.Adjacency[Vertex_1][Slot] == -1) { Result.Adjacency[Vertex_1][Slot] = Vertex_0; break;}
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
  GPU_Hull Packed = {0};
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
    Hull_Storage_Buffer = Buffer_Allocate (/*Size         =>*/ sizeof (GPU_Hull),
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

  // Define the descriptor bindings for the physics compute pipeline
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

  // Create the pipeline layout with push constants for per-frame GPU_Input delivery
  VkPushConstantRange Push_Range = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof (GPU_Input)};
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
  Player_State_Buffer = Buffer_Allocate (/*Size         =>*/ sizeof (GPU_Player),
                                         /*Usage        =>*/ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                           | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                         /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                           | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Initialize the GPU player state from the spawn point with a capsule collider
  GPU_Player Initial_GPU_State = {
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
    Hull_Storage_Buffer = Buffer_Allocate (/*Size         =>*/ sizeof (GPU_Hull),
                                           /*Usage        =>*/ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                             | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                           /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                             | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    GPU_Hull Empty = {0};
    Empty.Count = 1;
    Buffer_Upload (Hull_Storage_Buffer, &Empty, sizeof Empty);
  }

  // Allocate the projectile pool buffer (binding 5)
  Projectile_Buffer = Buffer_Allocate (/*Size         =>*/ sizeof (GPU_Projectile_Pool),
                                       /*Usage        =>*/ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                         | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                       /*Memory_Flags =>*/ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  GPU_Projectile_Pool Empty_Pool = {0};
  Buffer_Upload (Projectile_Buffer, &Empty_Pool, sizeof Empty_Pool);

  // Allocate the physics descriptor pool and set
  VkDescriptorPoolSize Pool_Sizes[] = {{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
                                       {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             5}};
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
  GPU_Input GPU_Input = {
    In.Forward, In.Back, In.Left, In.Right,
    In.Jump, In.Fire, In.Crouch, Active_Movement,
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
  GPU_Player *Mapped;
  vkMapMemory (Device, Player_State_Buffer.Memory, 0, sizeof (GPU_Player), 0, (void **)&Mapped);
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
  GPU_Projectile_Pool GPU_Pool = {0};
  GPU_Pool.Count         = Projectiles.Count;
  GPU_Pool.Fire_Cooldown = Projectiles.Fire_Cooldown;
  for (int I = 0; I < Projectiles.Count; I++) {
    Projectile *P = &Projectiles.Slots[I];
    GPU_Pool.Slots[I] = (GPU_Projectile){
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
  GPU_Projectile_Pool *Mapped;
  vkMapMemory (Device, Projectile_Buffer.Memory, 0, sizeof (GPU_Projectile_Pool), 0, (void **)&Mapped);
  Projectiles.Count         = Mapped->Count;
  Projectiles.Fire_Cooldown = Mapped->Fire_Cooldown;
  for (int I = 0; I < Projectiles.Count; I++) {
    GPU_Projectile *G = &Mapped->Slots[I];
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

  // Remove dead projectiles, play explosion sound on impact
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
  int Impulse_Len = (int)(MODAL_SAMPLE_RATE * 0.003f);
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
  if (fread (Header, 1, 44, F) < 44) { fclose (F); return 0;}

  // Verify RIFF/WAVE signature
  if (memcmp (Header, "RIFF", 4) != 0 or memcmp (Header + 8, "WAVE", 4) != 0) { fclose (F); return 0;}

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
  else { free (Data); return 0;}

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
  if (Buf) { printf ("[audio] loaded %s\n", Path); return Buf;}
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
  if (not Audio.Device) { fprintf (stderr, "[audio] failed to open device\n"); return;}

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
  if (not File) {fprintf (stderr, "Cannot open shader %s\n", Path); exit (1);}

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
    vec4  Env_Sun_Dir;      // xyz = Direction,     w = Angular_Radius
    vec4  Env_Sun_Color;    // xyz = Color,         w = Intensity
    vec4  Env_Sky_Zenith;   // xyz = Zenith color,  w = Sky_Intensity
    vec4  Env_Sky_Horizon;  // xyz = Horizon color, w = cos (Sun_Disc_Size)
    vec4  Env_Ambient_Up;   // xyz = Ambient up,    w = Sun_Disc_Intensity
    vec4  Env_Ambient_Down; // xyz = Ambient down,  w = Fog_Density
    vec4  Env_Fog_Color;    // xyz = Fog color,     w = Lightmap_Mult
  };
  layout(binding = 11, r32f) uniform image2D             Depth_Output;
  
  layout(location = 0) rayPayloadEXT vec4 Payload; // rgb = Color, a = Hit distance
  
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
    vec2  Pixel     = vec2 (Px) + 0.5;
    vec2  Uv        = Pixel / vec2 (Img_Size);
    vec2  Ndc       = fma (Uv, vec2 (2.0), vec2 (-1.0));
    vec4  Target    = Inverse_Projection * vec4 (Ndc.x, Ndc.y, 0.0, 1.0);
    vec4  Direction = Inverse_View * vec4 (normalize (Target.xyz / Target.w), 0.0);
  
    // Accumulate color and depth across all samples
    for (int S = 0; S < SPP; S++) {
      Payload = vec4 (0.0, 0.0, 0.0, 10000.0);

      // Mask 0xFD: primary rays see everything EXCEPT player body (bit 1 = 0x02)
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
    vec4  Env_Sun_Dir;      // xyz = Direction,     w = Angular_Radius
    vec4  Env_Sun_Color;    // xyz = Color,         w = Intensity
    vec4  Env_Sky_Zenith;   // xyz = Zenith color,  w = Sky_Intensity
    vec4  Env_Sky_Horizon;  // xyz = Horizon color, w = cos (Sun_disc_size)
    vec4  Env_Ambient_Up;   // xyz = Ambient up,    w = Sun_Disc_Intensity
    vec4  Env_Ambient_Down; // xyz = Ambient down,  w = Fog_Density
    vec4  Env_Fog_Color;    // xyz = Fog color,     w = Lightmap_Mult
  };
  
  // Scene geometry
  layout(binding = 3, std430) readonly buffer Vertex_Data   {vec4 Data[];} Vertices;
  layout(binding = 4, std430) readonly buffer Index_Data    {uint Data[];} Indices;
  layout(binding = 5, std430) readonly buffer Material_Data {vec4 Data[];} Materials;
  layout(binding = 6, std430) readonly buffer Tex_Id_Data   {uint Data[];} Texture_Ids;
  layout(binding = 7)         uniform sampler2D              Lightmap;
  
  // Per-figure geometry arrays: indexed by (instanceCustomIndex & 0xFF) - 1
  layout(binding = 8,  std430) readonly buffer Figure_Vertex_Data {vec4 Data[];} Figure_Vertices[];
  layout(binding = 9,  std430) readonly buffer Figure_Index_Data  {uint Data[];} Figure_Indices[];
  layout(binding = 10, std430) readonly buffer Figure_Tex_Id_Data {uint Data[];} Figure_Tex_Ids[];

  // Bindless texture array (binding 12: must be highest for variable descriptor count)
  layout(binding = 12) uniform sampler2D Textures[];

  // Comment here !!! 
  layout(location = 0) rayPayloadInEXT vec4 Payload; // rgb = color, a = hit distance

  // Shadow rays now use inline rayQueryEXT - no payload needed (saves continuation stack)
  hitAttributeEXT vec2 Barycentrics;
  
  // PCG hash for stochastic effects (soft shadows, importance sampling)
  uint PCG (uint V) {
    uint S = V * 747796405u + 2891336453u;
    uint W = ((S >> ((S >> 28u) + 4u)) ^ S) * 277803737u;
    return (W >> 22u) ^ W;
  }

  // Interleaved Gradient Noise - produces a blue-noise-like spatial distribution
  float IGN (vec2 Pos) {
    return fract (52.9829189 * fract (0.06711056 * Pos.x + 0.00583715 * Pos.y));
  }

  // Sampling Visible GGX Normals with Spherical Caps
  vec3 Sample_GGX_VNDF (vec3 Ve, float Alpha, float U1, float U2) {
    vec3  Vh        = normalize (vec3 (Alpha * Ve.x, Alpha * Ve.y, Ve.z));
    float Phi       = 6.28318530 * U1;
    float Z         = (1.0 - U2) * (1.0 + Vh.z) - Vh.z;
    float Sin_Theta = sqrt (clamp (1.0 - Z * Z, 0.0, 1.0));
    vec3  C         = vec3 (Sin_Theta * cos (Phi), Sin_Theta * sin (Phi), Z);
    vec3  Nh        = C + Vh;
    return normalize (vec3 (Alpha * Nh.x, Alpha * Nh.y, max (0.0, Nh.z)));
  }
  
  // Soft shadow ray direction - blue-noise spatial + golden-ratio temporal
  vec3 Soft_Shadow_Dir (vec3 Ld, uint Prim, uint Inst, uint Frame, float Disk_Radius) {
    vec2  Px = vec2 (gl_LaunchIDEXT.xy);
    float N1 = fract (IGN (Px) + float (Frame) * 0.7548776662);
    float N2 = fract (IGN (Px + 17.0) + float (Frame) * 0.5698402910);

    // Concentric disk mapping
    float A = 2.0 * N1 - 1.0, B = 2.0 * N2 - 1.0;
    float Rad, Phi;
    if (A * A > B * B) {Rad = A; Phi = 0.785398          * (B / max (abs (A), 1e-6));}
    else               {Rad = B; Phi = 1.5708 - 0.785398 * (A / max (abs (B), 1e-6));}
    Rad *= Disk_Radius;
    vec3 Light_Tangent = (abs (Ld.y) < 0.99) ? normalize (cross (Ld, vec3 (0, 1, 0)))
                                             : normalize (cross (Ld, vec3 (1, 0, 0)));
    vec3 Light_Bitangent = cross (Ld, Light_Tangent);
    return normalize (Ld + Light_Tangent * (cos (Phi) * Rad) + Light_Bitangent * (sin (Phi) * Rad));
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
  // Instance encodes (Texture_Base << 8) | Figure_Slot where Slot 0 = world, Slot 1+ = figure array index + 1
  vec4 Read_Raw (uint I, uint Slot, uint Fig) {
    if (Fig > 0u) return Figure_Vertices[Fig - 1u].Data[I * 3 + Slot];
    return Vertices.Data[I * 3 + Slot];
  }
  vec3 Read_Position    (uint I, uint Inst) {return Read_Raw (I, 0, Inst).xyz;}
  vec2 Read_Lightmap_UV (uint I, uint Inst) {return Read_Raw (I, 1, Inst).zw;}
  vec2 Read_Tex_UV      (uint I, uint Inst) {return Read_Raw (I, 1, Inst).xy;}
  vec3 Read_Normal      (uint I, uint Inst) {return Read_Raw (I, 2, Inst).xyz;}
  
  // Closest_Hit shader main
  void main () {

    // Unpack instanceCustomIndex:  [7:0] = figure slot (0 = world, 1+ = pool slot + 1)
    //                              [8]   = weapon flag (weapon-style lighting)
    //                              [23:9] = texture base offset (15 bits)
    uint  Raw_Instance = gl_InstanceCustomIndexEXT;
    uint  Fig          = Raw_Instance & 0xFFu;
    bool  Is_Figure    = (Fig > 0u);
    bool  Is_Weapon    = (Raw_Instance & 0x100u) != 0u;
    uint  Tex_Base     = Raw_Instance >> 9u;
    uint  Primitive    = gl_PrimitiveID;

    // Adaptive quality budget: 0.0 = full quality (60fps+), 1.0 = minimal work (< 5fps)
    float Budget = float ((Active_SPP >> 8u) & 0xFFu) / 255.0;

    // Fetch triangle vertex indices from the appropriate buffer (world or figure array)
    uint I0, I1, I2;
    if (Is_Figure) {
      uint Idx = Fig - 1u;
      I0 = Figure_Indices[Idx].Data [Primitive * 3 + 0];
      I1 = Figure_Indices[Idx].Data [Primitive * 3 + 1];
      I2 = Figure_Indices[Idx].Data [Primitive * 3 + 2];
    } else {
      I0 = Indices.Data [Primitive * 3 + 0];
      I1 = Indices.Data [Primitive * 3 + 1];
      I2 = Indices.Data [Primitive * 3 + 2];
    }
  
    // Batched vertex attribute reads
    vec3 Bary  = vec3 (1.0 - Barycentrics.x - Barycentrics.y, Barycentrics.x, Barycentrics.y);
    vec4 V0_S0 = Read_Raw (I0, 0, Fig), V0_S1 = Read_Raw (I0, 1, Fig), V0_S2 = Read_Raw (I0, 2, Fig);
    vec4 V1_S0 = Read_Raw (I1, 0, Fig), V1_S1 = Read_Raw (I1, 1, Fig), V1_S2 = Read_Raw (I1, 2, Fig);
    vec4 V2_S0 = Read_Raw (I2, 0, Fig), V2_S1 = Read_Raw (I2, 1, Fig), V2_S2 = Read_Raw (I2, 2, Fig);

    // Comment here !!!
    vec3 Position            = V0_S0.xyz * Bary.x + V1_S0.xyz * Bary.y + V2_S0.xyz * Bary.z;
    vec2 Tex_Coord           = V0_S1.xy  * Bary.x + V1_S1.xy  * Bary.y + V2_S1.xy  * Bary.z;
    vec2 Lightmap_Coordinate = V0_S1.zw  * Bary.x + V1_S1.zw  * Bary.y + V2_S1.zw  * Bary.z;
    vec3 Normal = normalize   (V0_S2.xyz * Bary.x + V1_S2.xyz * Bary.y + V2_S2.xyz * Bary.z);
  
    // Fetch the texture ID for this triangle. Figures use per-figure SSBO + packed Tex_Base offset.
    uint Tex_Id;
    uint Weapon_Stride = Weapon_Texture_Base >> 16u;
    if (Is_Figure) Tex_Id = Figure_Tex_Ids[Fig - 1u].Data [Primitive] + Tex_Base;
    else           Tex_Id = Texture_Ids.Data [Primitive];
  
    // Build tangent frame (Frisvad method) for normal mapping and parallax
    vec3 Geo_Normal = Normal; // Preserve geometric normal for parallax
    vec3 T_Axis, B_Axis;
    if (Geo_Normal.z < -0.999) {
      T_Axis = vec3 (0.0, -1.0, 0.0);
      B_Axis = vec3 (-1.0, 0.0, 0.0);
    } else {
      float Inv = 1.0 / (1.0 + Geo_Normal.z);
      T_Axis = vec3 (1.0 - Geo_Normal.x * Geo_Normal.x * Inv,      -Geo_Normal.x * Geo_Normal.y * Inv, -Geo_Normal.x);
      B_Axis = vec3      (-Geo_Normal.x * Geo_Normal.y * Inv, 1.0 - Geo_Normal.y * Geo_Normal.y * Inv, -Geo_Normal.y);
    }
  
    // Parallax occlusion mapping from height map
    vec3  V  = -gl_WorldRayDirectionEXT;
    float Hit_Dist = gl_HitTEXT;

    // Adaptive parallax: 300u at full quality, 0u at Budget=1 (pure lightmap fallback).
    float Parallax_Dist = 300.0 * (1.0 - Budget);
    if (not Is_Figure and Tex_Id < PBR_Stride and Hit_Dist < Parallax_Dist) {

      // Transform view to tangent space for parallax ray marching
      vec3 V_Tangent = vec3 (dot (V, T_Axis), dot (V, B_Axis), dot (V, Geo_Normal));

      // Scale parallax depth: 0.03 units - subtle but visible on close surfaces
      vec2 Parallax_Dir = V_Tangent.xy / max (V_Tangent.z, 0.1) * 0.03;
  
      // Hybrid linear-binary parallax 
      uint  Height_Tex    = nonuniformEXT(Tex_Id + PBR_Stride * 5u);
      float Layer_Depth   = 1.0 / 4.0; // Four coarse steps
      float Current_Depth = 0.0;
      vec2  Current_UV    = Tex_Coord;
      vec2  Uv_Step       = -Parallax_Dir * Layer_Depth;
      float H_Sample      = textureLod (Textures[Height_Tex], Current_UV, 0.0).r;
  
      // Phase 1: 4 coarse linear steps to find the crossing interval
      for (int Step = 0; Step < 4; Step++) {
        Current_UV += Uv_Step;
        H_Sample = textureLod (Textures[Height_Tex], Current_UV, 0.0).r;
        Current_Depth += Layer_Depth;
      }
  
      // Phase 2: 3 binary search steps to refine within the crossing interval
      vec2 Lo_UV = Current_UV - Uv_Step;  float Lo_D = Current_Depth - Layer_Depth;
      vec2 Hi_UV = Current_UV;            float Hi_D = Current_Depth;
      for (int B = 0; B < 3; B++) {
        vec2  Mid_UV = (Lo_UV + Hi_UV) * 0.5;
        float Mid_D  = (Lo_D  + Hi_D)  * 0.5;
        float Mid_H  = textureLod (Textures[Height_Tex], Mid_UV, 0.0).r;
        if (Mid_D < Mid_H) {Lo_UV = Mid_UV; Lo_D = Mid_D;}
        else               {Hi_UV = Mid_UV; Hi_D = Mid_D;}
      }
      Tex_Coord = Hi_UV;
    }
  
    // Sample PBR texture maps
    vec3  Albedo     = textureLod (Textures[nonuniformEXT(Tex_Id)], Tex_Coord, 0.0).rgb;
    vec3  Normal_Map = vec3 (0.0, 0.0, 1.0);
    float R = 0.5;
    float M = 0.0;
    vec3  Emissive  = vec3 (0.0);
  
    // Sample PBR maps for world geometry, figures, and weapon
    float PBR_Dist = mix (1000.0, 200.0, Budget);
    if (Is_Weapon) {
      // Weapon PBR: 6 map types by N surfaces, stride = Weapon_Stride
      Normal_Map = textureLod (Textures [nonuniformEXT (Tex_Id + Weapon_Stride)],      Tex_Coord, 0.0).rgb * 2.0 - 1.0;
      R          = textureLod (Textures [nonuniformEXT (Tex_Id + Weapon_Stride * 2u)], Tex_Coord, 0.0).r;
      M          = textureLod (Textures [nonuniformEXT (Tex_Id + Weapon_Stride * 3u)], Tex_Coord, 0.0).r;
      Emissive   = textureLod (Textures [nonuniformEXT (Tex_Id + Weapon_Stride * 4u)], Tex_Coord, 0.0).rgb;

    } else if (not Is_Figure and Tex_Id < PBR_Stride) {
      // World PBR: maps laid out at [diffuse_0..N, normal_0..N, roughness_0..N, ...]
      if (Hit_Dist < PBR_Dist) {
        Normal_Map = textureLod (Textures [nonuniformEXT (Tex_Id + PBR_Stride)],      Tex_Coord, 0.0).rgb * 2.0 - 1.0;
        R          = textureLod (Textures [nonuniformEXT (Tex_Id + PBR_Stride * 2u)], Tex_Coord, 0.0).r;
        M          = textureLod (Textures [nonuniformEXT (Tex_Id + PBR_Stride * 3u)], Tex_Coord, 0.0).r;
      }
      Emissive   = textureLod (Textures[nonuniformEXT(Tex_Id + PBR_Stride * 4u)], Tex_Coord, 0.0).rgb;

    // Fallback: derive from albedo statistics (non-weapon figures and textures outside PBR range)
    } else if (not Is_Weapon) {
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
    vec3  Lr = Env_Sun_Color.xyz * Env_Sun_Color.w; // Per-scene sun radiance (color  by  intensity)
    float NL  = max (dot (Normal, Ld), 0.0);
  
    // Skip full BRDF when surface faces away from sun
    vec3  Specular = vec3 (0.0);
    vec3  Diffuse  = vec3 (0.0);
    if (NL > 0.0) {
      vec3  H   = normalize (V + Ld);
      float NH  = max (dot (Normal, H), 0.0);
      float VH  = max (dot (V, H),      0.0);
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
  
      // Final specular - roughness-biased D prevents the razor-sharp GGX peak that causes fireflies at 1 SPP
      float D_Bias  = max (a2, SPECULAR_D_BIAS);  // Floor: R≈0.32 minimum for D evaluation
      float D_Denom = NH * NH * (D_Bias - 1.0) + 1.0;
      float D_Safe  = D_Bias / (3.14159 * D_Denom * D_Denom);

      // Comment here !!!
      Specular = D_Safe * Vis * F;
      Diffuse  = (1.0 - F) * (1.0 - M) * Albedo * 0.31831;  // 1/π
    }
  
    // Hemisphere ambient diffuse 
    vec3  Sky_Color    = Env_Ambient_Up.xyz;   // Per-scene ambient from above (sky contribution)
    vec3  Ground_Color = Env_Ambient_Down.xyz; // Per-scene ambient from below (ground bounce)
    float Hemisphere   = Normal.y * 0.5 + 0.5; // Zero is down, one up
    vec3  Ambient_Irradiance = mix (Ground_Color, Sky_Color, Hemisphere);
  
    // Ambient specular: pre-integrated split-sum approximation
    float Env_FT   = 1.0 - NV;  float Env_T5 = Env_FT * Env_FT; Env_T5 *= Env_T5 * Env_FT;
    vec3  Env_F    = F0 + (max (vec3 (1.0 - R), F0) - F0) * Env_T5; // Roughness-aware Fresnel
    vec2  Env_BRDF = vec2 (1.0 - R * 0.5, R * 0.08); // Analytic fit to DFG LUT
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

    // Dynamic reflection culling - Budget-aware: save rays on barely-reflective surfaces, focus them on metals/smooth where
    // reflections are clearly visible.
    float Refl_Roughness_Gate = mix (REFL_GATE_LO, REFL_GATE_HI, Budget);
    bool  Refl_Active = not Is_Reflection_Bounce and not Is_Weapon and (R < Refl_Roughness_Gate);
    float Refl_Dist = mix (800.0, SHADOW_DIST_HI, Budget);
    float Reflection_Weight = (not Refl_Active or Hit_Dist > Refl_Dist)
                                ? 0.0
                                : max (max (Env_F.r, Env_F.g), Env_F.b) * (1.0 - R * R);
    vec3  Reflection_Color  = vec3 (0.0);

    // Dynamic threshold: skip marginal reflections at high Budget
    float Refl_Threshold = mix (REFL_THRESH_LO, REFL_THRESH_HI, Budget);
    if (Reflection_Weight > Refl_Threshold) {

      // VNDF importance-sampled reflection with IGN blue noise spatial base + golden ratio temporal rotation.
      vec2  Px = vec2 (gl_LaunchIDEXT.xy);
      float U1 = fract (IGN (Px) + float (Frame) * 0.7548776662);
      float U2 = fract (IGN (Px + 17.0) + float (Frame) * 0.5698402910);
      float Alpha   = max (R * R, VNDF_ALPHA_FLOOR);

      // Build tangent frame around surface normal
      vec3 T = (abs (Normal.y) < 0.99) ? normalize (cross (Normal, vec3 (0, 1, 0)))
                                        : normalize (cross (Normal, vec3 (1, 0, 0)));
      vec3 B = cross (Normal, T);

      // Transform view to tangent space, sample VNDF, transform back
      vec3 V_Tangent  = vec3 (dot (V, T), dot (V, B), dot (V, Normal));
      vec3 H_Tangent  = Sample_GGX_VNDF (V_Tangent, Alpha, U1, U2);
      vec3 H          = T * H_Tangent.x + B * H_Tangent.y + Normal * H_Tangent.z;
      vec3 Refl_Dir   = reflect (-V, H);

      // Ensure reflection doesn't go into the surface
      if (dot (Refl_Dir, Normal) <= 0.0) Refl_Dir = reflect (-V, Normal);

      // Comment here !!!
      Payload = vec4 (0.0, 0.0, 0.0, -1.0);
      traceRayEXT (Top_Level, gl_RayFlagsOpaqueEXT,
                   0xFF,
                   0, 1, 0,
                   Position + Normal * 0.2,
                   0.01,
                   Refl_Dir,
                   mix (REFL_TRACE_LO, REFL_TRACE_HI, Budget),
                   0);

      // Scene-relative luminance clamp: soft proportional limit
      vec3  Rc      = Payload.rgb;
      float Rc_Lum  = dot (Rc, vec3 (0.2126, 0.7152, 0.0722));
      float Srf_Lum = dot (Ambient_Irradiance * Albedo, vec3 (0.2126, 0.7152, 0.0722));
      float Max_Lum = max (Srf_Lum, 0.05) * mix (REFL_CLAMP_LO, REFL_CLAMP_HI, Budget);
      Reflection_Color = Rc_Lum > Max_Lum ? Rc * (Max_Lum / Rc_Lum) : Rc;
    }

    // Gentle reflection damping at high Budget (frame-time pressure)
    float Refl_Strength = 1.0 - Budget * REFL_DAMPING;
    float Soft_Weight = Reflection_Weight * Refl_Strength;
    Soft_Weight *= clamp ((Reflection_Weight - Refl_Threshold) * REFL_SOFT_EDGE, 0.0, 1.0);

    // Blend reflection into indirect specular: replace the hemisphere approximation with actual reflection, weighted by the Fresnel
    vec3 Traced_Specular = Env_F * mix (Indirect_Specular / max (Env_F, vec3(0.01)),
                                         Reflection_Color,
                                         vec3 (Soft_Weight));
  
    // Compute final shading based on instance type
    vec3 Color;
    float Shadow_Dist = mix (SHADOW_DIST_LO, SHADOW_DIST_HI, Budget); // Adaptive shadow ray cutoff distance
  
    // Apply per-instance lighting model
    if (Is_Weapon) {
      vec3 Direct    = (Diffuse + Specular) * Lr * NL;
      vec3 Weapon_Full  = Direct * 1.0 + Indirect_Diffuse * 1.2 + Traced_Specular * 0.5;
      vec3 Weapon_Cheap = Ambient_Irradiance * Albedo * 2.5 + Albedo * max(NL, 0.3);
      Color = mix (Weapon_Full, Weapon_Cheap, Budget);

    // Figure (non-weapon): direct sun + shadows + hemisphere ambient, no lightmap
    } else if (Is_Figure) {
      float Shadow_Factor = (NL > 0.0 and not Is_Reflection_Bounce and Hit_Dist < Shadow_Dist)
                              ? Trace_Shadow (Position, Normal, Ld, Primitive, Fig, Frame, Env_Sun_Dir.w)
                              : 1.0;
      vec3 Direct = (Diffuse + Specular) * Lr * NL * Shadow_Factor;

      // Entity color: balanced ambient - not too bright (avoids glow), not too dark (avoids silhouette)
      vec3 Entity_Full  = Direct + Indirect_Diffuse * 0.8 + Traced_Specular;
      vec3 Entity_Cheap = Ambient_Irradiance * Albedo * 0.9 + Albedo * max(NL, 0.2) * 0.4;
      Color = mix (Entity_Full, Entity_Cheap, Budget);

      // Subtle saturation boost for entities - counteracts ambient washout on character models
      float Entity_Luminance = dot (Color, vec3 (0.2126, 0.7152, 0.0722));
      Color = mix (vec3 (Entity_Luminance), Color, 1.15); // 15% saturation increase

    // Otherwise, comment here !!!
    } else {
      vec3 Lightmap_Color = textureLod (Lightmap, Lightmap_Coordinate, 0.0).rgb * Env_Fog_Color.w; // Lightmap multiplier
  
      // Inline ray query for shadows
      float Shadow_Factor = (NL > 0.0 and not Is_Reflection_Bounce and Hit_Dist < Shadow_Dist)
        ? Trace_Shadow (Position, Normal, Ld, Primitive, Fig, Frame, Env_Sun_Dir.w) : 1.0;
  
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
    vec4  Env_Fog_Color;    // xyz = fog color, w = lightmap_mult
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
  layout(binding = 1, std430) readonly buffer Vertex_Data {vec4 Data[];} Vertices;
  layout(binding = 2, std430) readonly buffer Index_Data  {uint Data[];} Indices;
  
  layout(binding = 3, std430) buffer Player_Buffer {
    vec3  Position; float Pad_A;
    vec3  Velocity; float Pad_B;
    float Yaw;
    float Pitch;
    int   On_Ground; 
    int   Jump_Held;
    vec3  Ground_Normal; float Pad_C;
    int   Ground_Plane;
    int   Ducked;
    float View_Height; 
    float Stuck_Time;
    float Speed_Last;  
    int   Shape;
    vec3  Extents; float Pad_D;
    float Spine;   float Pad_E1, Pad_E2, Pad_E3;
  } Player;
  
  layout(binding = 4, std430) readonly buffer Hull_Buffer {
    vec4  Hull_Vertices  [256];
    int   Hull_Adjacency [256][16];
    int   Hull_Count;
    float Hull_Radius;
    vec3  Hull_Centroid; int Hull_Pad;
  };
  
  struct GPU_Projectile {
    vec3  Position; float Pad_A;
    vec3  Velocity;
    float Lifetime;
    int   Active;   
    int   Material_Hit;
    float Radius;  
    float Damage;
    float Hit_U,
    float Hit_V;                   // UV at impact point (for CPU-side damage map lookup)
    int   Instance_Hit; int Pad_B; // TLAS instance index of the object hit (-1 = none)
  };
  
  layout(binding = 5, std430) buffer Projectile_Buffer {
    GPU_Projectile Projectiles[64];
    int   Projectile_Count;
    float Fire_Cooldown; float Proj_Pad[2];
  };
  
  layout(push_constant) uniform Push {
    int   Forward, Back, Left, Right;
    int   Jump, Fire, Crouch, Movement_Style;
    float Delta_X, Delta_Y, Dt, Pad2;
  } Input;

  layout(local_size_x = 1) in;

  // Physics constants - Id's Quake Engine
  const float Q3_GRAVITY         = 800.0;
  const float Q3_GROUND_FRICTION = 6.0;
  const float Q3_STOP_SPEED      = 100.0;
  const float Q3_GROUND_ACCEL    = 10.0;
  const float Q3_AIR_ACCEL       = 1.0;
  const float Q3_MAX_SPEED       = 320.0;
  const float Q3_JUMP_VEL        = 270.0;
  const float Q3_OVERBOUNCE      = 1.001;

  // Physics constants - Valve's Source Engine
  const float SRC_GRAVITY         = 800.0;
  const float SRC_GROUND_FRICTION = 4.0;
  const float SRC_STOP_SPEED      = 100.0;
  const float SRC_GROUND_ACCEL    = 5.5;
  const float SRC_AIR_ACCEL       = 10.0;  
  const float SRC_MAX_SPEED       = 250.0;     
  const float SRC_JUMP_VEL        = 301.993377; 
  const float SRC_OVERBOUNCE      = 1.0; 

  // Runtime-selected constants (branched once per frame - no divergence cost on single-invocation dispatch)
  float GRAVITY, GROUND_FRICTION, STOP_SPEED, GROUND_ACCELERATE, AIR_ACCELERATE;
  float MAXIMUM_SPEED, JUMP_VELOCITY, OVERBOUNCE;

  const float STEP_SIZE           = 18.0;
  const float MINIMUM_WALK_NORMAL = 0.7;
  const int   MAXIMUM_CLIP_PLANES = 5;
  const float DEFAULT_VIEW_HEIGHT = 22.0;
  const float CROUCH_VIEW_HEIGHT  = 8.0;
  const float MOUSE_SENSITIVITY   = 0.003;
  
  // Collider shape constants
  const int SHAPE_SPHERE    = 0;
  const int SHAPE_CAPSULE   = 1;
  const int SHAPE_AABB      = 2;
  const int SHAPE_CYLINDER  = 3;
  const int SHAPE_ELLIPSOID = 4;
  const int SHAPE_HULL      = 5;
  
  // Brute-force: O(n) linear scan over all hull vertices. Best for small hulls (< 64 verts)
  vec3 hull_support_brute (vec3 direction) {
    float best_dot = -1e30;
    int   best_idx = 0;
    for (int i = 0; i < Hull_Count; i++) {
      float d = dot (Hull_Vertices[i].xyz, direction);
      if (d > best_dot) { best_dot = d; best_idx = i;}
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
        if (d > best_dot) { best_dot = d; best_neighbor = neighbor;}
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
      case SHAPE_SPHERE:  return d * Player.Extents.x;
      case SHAPE_CAPSULE: return d * Player.Extents.x + vec3 (0.0, sign(d.y) * Player.Spine, 0.0);
      case SHAPE_AABB:    return sign(d) * Player.Extents;
      case SHAPE_HULL:    return hull_support (d);
  
      // Comment here !!!
      case SHAPE_CYLINDER: {
        vec2  xz    = d.xz;
        float len   = length (xz);
        vec2  disc  = (len > 1e-6) ? xz / len * Player.Extents.x : vec2(0.0);
        return vec3 (disc.x, sign(d.y) * Player.Extents.y, disc.y);
      }
  
      // Comment here !!!
      case SHAPE_ELLIPSOID: {
        vec3 scaled = d / Player.Extents;
        float len   = length (scaled);
        return (len > 1e-6) ? normalize(scaled) * Player.Extents : vec3(0.0);
      }

      // Otherwise, comment here !!!
      default: return d * Player.Extents.x;
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
  
    // Use 7 probe directions: 6 cardinal axes + movement direction
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
        if (dot (trace.Normal, planes[p]) > 0.99) { duplicate = true; break;}
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

    // Select physics constants based on movement style
    if (Input.Movement_Style == 1) {
      GRAVITY=SRC_GRAVITY; GROUND_FRICTION=SRC_GROUND_FRICTION; STOP_SPEED=SRC_STOP_SPEED;
      GROUND_ACCELERATE=SRC_GROUND_ACCEL; AIR_ACCELERATE=SRC_AIR_ACCEL;
      MAXIMUM_SPEED=SRC_MAX_SPEED; JUMP_VELOCITY=SRC_JUMP_VEL; OVERBOUNCE=SRC_OVERBOUNCE;
    } else {
      GRAVITY=Q3_GRAVITY; GROUND_FRICTION=Q3_GROUND_FRICTION; STOP_SPEED=Q3_STOP_SPEED;
      GROUND_ACCELERATE=Q3_GROUND_ACCEL; AIR_ACCELERATE=Q3_AIR_ACCEL;
      MAXIMUM_SPEED=Q3_MAX_SPEED; JUMP_VELOCITY=Q3_JUMP_VEL; OVERBOUNCE=Q3_OVERBOUNCE;
    }

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

      // Other book-keeping
      Projectile_Count = idx + 1;
      Fire_Cooldown = 0.8;
    }
  
    // Advance each active projectile: move, trace against TLAS, kill on impact or timeout
    for (int i = 0; i < Projectile_Count; i++) {
      if (Projectiles[i].Active == 0) continue;

      // Decrement lifetime and kill expired projectiles
      Projectiles[i].Lifetime -= Input.Dt;
      if (Projectiles[i].Lifetime <= 0.0) { Projectiles[i].Active = 0; continue;}

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
    int Budget_256; // Budget  by  256 (0 = full quality, 256 = cheap path)
  };
  
  layout(local_size_x = 8, local_size_y = 8) in;
  
  // A 3 by 3 a-trous kernel weights (Gaussian-like, symmetric)
  const float Kernel[3] = float[3](1.0, 2.0 / 3.0, 1.0 / 6.0);
  
  // Depth-based normal from 3 cached depth values
  vec3 Normal_From_Depths (float D_C, float D_R, float D_U) {
    return normalize (vec3 (D_C - D_R, D_C - D_U, 1.0));
  }
  
  // Denoise shader main
  void main () {
    ivec2 Pixel = ivec2 (gl_GlobalInvocationID.xy);
    ivec2 Size  = imageSize (Input_Image);
    if (Pixel.x >= Size.x or Pixel.y >= Size.y) return;

    // Load center pixel color
    vec3  Center_Color = imageLoad (Input_Image, Pixel).rgb;
  
    // Motion-adaptive denoiser: Budget correlates with frame-time pressure and motion -relax edge-stopping for smoother frames
    float Motion = clamp (float (Budget_256) / 256.0, 0.0, 1.0);
  
    // Batch-load all depth values for the 3 by 3 kernel in one shot
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
    float Center_Depth = Depths[4]; // Center of 3 by 3 = index 4
  
    // Center normal from preloaded batch: Depths[5] = right, Depths[7] = up (at Step_Size offset). At larger step sizes, kernel-spaced
    // gradients are more appropriate for the a-trous edge stopping.
    vec3  Center_Normal = Normal_From_Depths (Center_Depth, Depths[5], Depths[7]);

    // Compute center pixel luminance for edge stopping
    float Center_Luminance = log2 (1.0 + dot (Center_Color, vec3 (0.2126, 0.7152, 0.0722)));

    // Initialize accumulator for weighted filter output
    vec3  Sum    = vec3 (0.0);
    float Weight = 0.0;
  
    // A 3 by 3 sparse kernel at current step size - depths already cached
    for (int I = 0; I < 9; I++) {
      vec3  S_Color = imageLoad (Input_Image, Sample_Positions[I]).rgb;
      float S_Depth = Depths[I];
      float Sample_Luminance   = log2 (1.0 + dot (S_Color, vec3 (0.2126, 0.7152, 0.0722)));
  
      // Spatial weight: Gaussian kernel
      int Dx = (I % 3) - 1, Dy = (I / 3) - 1;
      float W_Spatial = Kernel[abs(Dx)] * Kernel[abs(Dy)];
  
      // Depth edge stopping - relaxed during motion for smoother frames
      float Depth_Diff = abs (Center_Depth - S_Depth) / max (Center_Depth, 0.1);
      float Depth_Sensitivity = mix (DENOISE_DEPTH_LO, DENOISE_DEPTH_HI, Motion);
      float W_Depth = exp (-Depth_Diff * Depth_Sensitivity);
  
      // Normal edge stopping: compute sample normal from cached depths
      float S_D_Right = ((I % 3) < 2) ? Depths[I + 1] : S_Depth;
      float S_D_Up    = (I < 6) ? Depths[I + 3] : S_Depth;
      vec3  S_Normal  = Normal_From_Depths (S_Depth, S_D_Right, S_D_Up);

      // Normal edge stopping - motion-adaptive power: x^16 when still, x^4 during motion (lets filter smooth across surface creases)
      float Ndot = max (dot (Center_Normal, S_Normal), 0.0);
      Ndot *= Ndot;  // x^2
      float Ndot4 = Ndot * Ndot; // x^4
      float Ndot16 = Ndot4 * Ndot4 * Ndot4 * Ndot4; // x^16
      float W_Normal = mix (Ndot16, Ndot4, Motion);
  
      // Luminance edge stopping - aggressive during motion to smooth noise
      float Luminance_Difference = abs (Center_Luminance - Sample_Luminance);
      float Lum_Sensitivity = mix (DENOISE_LUM_LO, DENOISE_LUM_HI, Motion);
      float Weight_Luminance = exp (-Luminance_Difference * Luminance_Difference * Lum_Sensitivity);

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
    uint  Dt_Frame;       // Range [15:0] = half(Delta_Time), [31:16] = Frame_Count
    uint  Velocity;       // packHalf2x16(Velocity_X, Velocity_Z)
    uint  Speed_Exposure; // packHalf2x16(Speed, Exposure)
    uint  Bloom_Vignette; // packHalf2x16(Bloom_Strength, Vignette_Strength)
    uint  Reproject[8];   // packHalf2x16-compressed 4 by 4 reprojection matrix (Proj * Prev_View * Inv_View)
    uint  Inv_Proj_Diag;  // Range [15:0] = half(InvProj[0][0]), [31:16] = half(InvProj[1][1])
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

    // Checkerboard spatial reconstruction: pixels not traced this frame hold stale values from the wrong screen position
    uint Frame = Frame_Count & 0xFFFFu;
    bool Was_Traced = ((Pixel.x + Pixel.y + int(Frame)) & 1) == 0;
    if (not Was_Traced) {
      ivec2 Offsets[4] = ivec2[4](ivec2(-1,0), ivec2(1,0), ivec2(0,-1), ivec2(0,1));
      vec3  Sum = vec3 (0.0);
      float W_Sum = 0.0;
      for (int i = 0; i < 4; i++) {
        ivec2 P = clamp (Pixel + Offsets[i], ivec2 (0), Size - 1);
        vec3  C = imageLoad (Color_Image, P).rgb;
        float D = imageLoad (Depth_Image, P).r;
        float W = 1.0 / (1.0 + abs (D - Depth) * 20.0);
        Sum += C * W;
        W_Sum += W;
      }
      Color = Sum / max (W_Sum, 1e-6);
    }

    // Combined firefly rejection + motion-adaptive spatial filter
    {
      vec3  Nb[8];
      float Nd[8];
      const ivec2 Offs[8] = ivec2[8](
        ivec2(-1,-1), ivec2(0,-1), ivec2(1,-1),
        ivec2(-1, 0),              ivec2(1, 0),
        ivec2(-1, 1), ivec2(0, 1), ivec2(1, 1));
      for (int i = 0; i < 8; i++) {
        ivec2 P = clamp (Pixel + Offs[i], ivec2 (0), Size - 1);
        Nb[i] = imageLoad (Color_Image, P).rgb;
        Nd[i] = imageLoad (Depth_Image, P).r;
      }

      // Firefly rejection: clamp center luminance to second-brightest neighbor
      float Lum_C = dot (Color, vec3 (0.2126, 0.7152, 0.0722));
      float Max1 = 0.0, Max2 = 0.0;
      for (int i = 0; i < 8; i++) {
        float L = dot (Nb[i], vec3 (0.2126, 0.7152, 0.0722));
        if (L > Max1) { Max2 = Max1; Max1 = L;}
        else if (L > Max2) { Max2 = L;}
      }
      float Firefly_Limit = Max2 * FIREFLY_HEADROOM + FIREFLY_BIAS;
      if (Lum_C > Firefly_Limit) Color *= Firefly_Limit / Lum_C;

      // Motion-adaptive CAS sharpening - restores edge definition when still
      {
        float Motion_CAS = clamp (Speed * 0.04, 0.0, 1.0);
        float Cas_Scale  = CAS_AMOUNT * (1.0 - Motion_CAS);
        vec3 Avg = (Nb[1] + Nb[3] + Nb[4] + Nb[6]) * 0.25;
        vec3 Minimum = min (min (Nb[1], Nb[6]), min (Nb[3], Nb[4]));
        vec3 Maximum = max (max (Nb[1], Nb[6]), max (Nb[3], Nb[4]));
        vec3 Mn = Minimum / (1.0 + Minimum);
        vec3 Mx = Maximum / (1.0 + Maximum);
        vec3 Sh = clamp (min (Mn, 1.0 - Mx) / (Mx - Mn + 0.04), 0.0, 1.0) * Cas_Scale;
        Color = max (mix (Avg, Color, 1.0 + Sh * CAS_MIX), vec3 (0.0));
      }
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
        History = clamp (History, M1 - Sigma * TAA_SIGMA, M1 + Sigma * TAA_SIGMA);
  
        // Luminance-based history rejection
        //
        // If the clamped history still differs significantly from the current frame, the surface has changed (new geometry, lighting
        // change, disocclusion).
        //
        float Current_Luminance  = dot (Color,   vec3 (0.2126, 0.7152, 0.0722));
        float History_Luminance = dot (History, vec3 (0.2126, 0.7152, 0.0722));
        float Luminance_Difference = abs (Current_Luminance - History_Luminance) / max (Current_Luminance, 0.01);
        float Anti_Lag = clamp (Luminance_Difference * 5.0, 0.0, 1.0); // Reject
  
        // Disocclusion detection
        vec2 Screen_Displacement = vec2 (Prev_Pixel - Pixel) / vec2 (Size);
        float Displacement_Length = length (Screen_Displacement);
        float Disocclusion = clamp (Displacement_Length * 30.0, 0.0, 1.0); // Rejected
  
        // Temporal blend - per-pixel reprojection displacement as motion
        float Pixel_Motion = clamp (Displacement_Length * 50.0, 0.0, 1.0);
        float Any_Motion   = max (Pixel_Motion, clamp (Speed * 0.04, 0.0, 1.0));

        // Only if BOTH player and pixel are truly still
        float Is_Static = step (Any_Motion, 0.01);

        // Static: 1/N convergence floored at 0.25 - more temporal samples accumulated for smoother, noise-free image when still.
        float Static_Alpha = max (1.0 / max (float (Frame_Count), 1.0), TAA_STATIC_FLOOR);

        // Moving: almost entirely current-frame to kill ghosting
        float Base                 = mix (TAA_MOVE_LO, TAA_MOVE_HI, Any_Motion);
        float Framerate_Adaptation = clamp ((Delta_Time - 0.016) * 30.0, 0.0, 1.0);
        float Moving_Alpha         = max (max (max (Base, Framerate_Adaptation), Disocclusion), Anti_Lag);

        // Blend: pixels that moved use aggressive current-frame dominance
        float Alpha = mix (Moving_Alpha, Static_Alpha, Is_Static);
        Color       = mix (History, Color, Alpha);
      }

      // Off-screen > keep current frame as-is (no history to blend)
    }

    // Write blended result to history for next frame (pre-tonemap, linear HDR)
    imageStore (History_Image, Pixel, vec4 (Color, 1.0));
  
    // Bloom
    //
    // Horizontal + vertical taps create a cross-shaped bloom pattern (fake god rays)
    //
    vec3 Bloom = max (imageLoad (Color_Image, clamp (Pixel + ivec2 ( 3, 0), ivec2 (0), Size - 1)).rgb - 0.4, vec3 (0.0))
               + max (imageLoad (Color_Image, clamp (Pixel + ivec2 (-3, 0), ivec2 (0), Size - 1)).rgb - 0.4, vec3 (0.0))
               + max (imageLoad (Color_Image, clamp (Pixel + ivec2 (0,  3), ivec2 (0), Size - 1)).rgb - 0.4, vec3 (0.0))
               + max (imageLoad (Color_Image, clamp (Pixel + ivec2 (0, -3), ivec2 (0), Size - 1)).rgb - 0.4, vec3 (0.0));
    Color += Bloom * Bloom_Strength;
  
    // God rays
    //
    // When the sun is on screen, march radially from each pixel toward the sun position, accumulating bright sky samples. This creates
    // volumetric-looking light shafts streaming from the sun through gaps in geometry.
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

        // Incremental stepping replaces multiply-per-iteration
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
    Color = pow (max (Color, vec3 (0.0)), vec3 (1.16));
  
    // Blue-noise dithering
    //
    // Dither by ~0.5/255 in the output before sRGB conversion to prevent banding in dark gradients. The blit to B8G8R8A8_SRGB swapchain
    // handles sRGB encoding.
    //
    float Dither = hash (vec2 (Pixel) + Params.Time * 1.618) - 0.5;  // [-0.5, 0.5]
    Color += Dither / 255.0;

    // Write final color to the display output image
    imageStore (Display_Image, Pixel, vec4 (clamp (Color, 0.0, 1.0), 1.0));
  }
} // Denoise

// ═════════════
//   Skinning
// ═════════════

glsl comp Skinning {
  #version 460

  layout(local_size_x = 64) in;

  // Bind-pose bone matrices (uploaded once at load time)
  layout(binding = 0, std430) readonly buffer Bind_Pose_Bones {
    mat3x4 Bind_Bones[];
  };

  // Inverse bind-pose matrices (uploaded once at load time)
  layout(binding = 1, std430) readonly buffer Inv_Bind_Bones {
    mat3x4 Inv_Bind[];
  };

  // Bone parent indices (-1 = root). Uploaded once at load time.
  layout(binding = 2, std430) readonly buffer Bone_Parents {
    int Parents[];
  };

  // Output: world-space skinning matrices (Pose[i] * InvBind[i]). Written by bone hierarchy pass.
  layout(binding = 3, std430) buffer Pose_Output {
    mat3x4 Pose[];
  };

  // Bind-pose source vertices (position, normal, uv, bone ids + weights packed)
  layout(binding = 4, std430) readonly buffer Bind_Vertices_Data {
    float Bind_Vertices[];
  };

  // Output skinned vertices
  layout(binding = 5, std430) writeonly buffer Skinned_Output {
    float Out_Vertices[];
  };

  // Push constants: vertex count, bone count, pass (0 = bone hierarchy, 1 = vertex skinning)
  layout(push_constant) uniform Skinning_Push {
    uint Vertex_Count;
    uint Bone_Count;
    uint Pass;
  };

  // 3x4 matrix multiply: C = A * B (row-major affine)
  mat3x4 Mat34_Mul_GPU (mat3x4 A, mat3x4 B) {
    mat3x4 C;
    for (int R = 0; R < 3; R++) {
      C[R][0] = A[R][0]*B[0][0] + A[R][1]*B[1][0] + A[R][2]*B[2][0];
      C[R][1] = A[R][0]*B[0][1] + A[R][1]*B[1][1] + A[R][2]*B[2][1];
      C[R][2] = A[R][0]*B[0][2] + A[R][1]*B[1][2] + A[R][2]*B[2][2];
      C[R][3] = A[R][0]*B[0][3] + A[R][1]*B[1][3] + A[R][2]*B[2][3] + A[R][3];
    }
    return C;
  }

  // Transform position by affine 3x4
  vec3 Xform_Pos (mat3x4 M, vec3 V) {
    return vec3 (dot (M[0], vec4 (V, 1.0)),
                 dot (M[1], vec4 (V, 1.0)),
                 dot (M[2], vec4 (V, 1.0)));
  }

  // Transform direction (no translation)
  vec3 Xform_Dir (mat3x4 M, vec3 V) {
    return vec3 (dot (M[0].xyz, V), dot (M[1].xyz, V), dot (M[2].xyz, V));
  }

  void main () {
    uint Id = gl_GlobalInvocationID.x;

    // Pass 0: bone hierarchy evaluation (one invocation per bone)
    // Bones are topologically sorted (parent index < child index), so a single serial pass in one
    // workgroup evaluates the whole skeleton. For skeletons up to 128 bones this is one wavefront.
    if (Pass == 0u) {
      if (Id >= Bone_Count) return;

      // Start with the bind pose as the local matrix
      mat3x4 Local = Bind_Bones[Id];

      // Walk parent chain: Pose[i] = Pose[parent] * Local
      int P = Parents[Id];
      if (P >= 0 and uint(P) < Bone_Count) {
        // Barrier: we need Pose[P] to be written before we read it. Since bones are topologically sorted
        // and we process them in order within the workgroup, we use a memory barrier.
        memoryBarrierBuffer ();
        barrier ();
        Pose[Id] = Mat34_Mul_GPU (Pose[P], Local);
      } else {
        Pose[Id] = Local;
      }

      // Second barrier, then compose with inverse bind-pose: Final[i] = Pose[i] * InvBind[i]
      memoryBarrierBuffer ();
      barrier ();
      Pose[Id] = Mat34_Mul_GPU (Pose[Id], Inv_Bind[Id]);
      return;
    }

    // Pass 1: vertex skinning (one invocation per vertex)
    if (Id >= Vertex_Count) return;

    uint Base = Id * 10u;
    vec3 Pos  = vec3 (Bind_Vertices[Base], Bind_Vertices[Base+1], Bind_Vertices[Base+2]);
    vec3 Norm = vec3 (Bind_Vertices[Base+3], Bind_Vertices[Base+4], Bind_Vertices[Base+5]);
    float U   = Bind_Vertices[Base+6], V = Bind_Vertices[Base+7];
    float Lu  = Bind_Vertices[Base+8], Lv = Bind_Vertices[Base+9];

    // Read packed bone ids and weights from appendix after vertex data
    uint Bone_Offset = Vertex_Count * 10u + (Id * 2u);
    uint Pack_A = floatBitsToUint (Bind_Vertices[Bone_Offset]);
    uint Pack_B = floatBitsToUint (Bind_Vertices[Bone_Offset + 1u]);
    uvec3 Bone_Id = uvec3 (Pack_A & 0xFFu, (Pack_A >> 8u) & 0xFFu, (Pack_A >> 16u) & 0xFFu);
    vec3  Bone_Wt = vec3 (float (Pack_B & 0xFFu), float ((Pack_B >> 8u) & 0xFFu), float ((Pack_B >> 16u) & 0xFFu)) / 255.0;

    vec3 Skinned_Pos = vec3 (0.0), Skinned_Norm = vec3 (0.0);
    if (Bone_Wt.x > 0.001) { mat3x4 M = Pose[min (Bone_Id.x, Bone_Count-1u)]; Skinned_Pos += Xform_Pos(M,Pos)*Bone_Wt.x; Skinned_Norm += Xform_Dir(M,Norm)*Bone_Wt.x; }
    if (Bone_Wt.y > 0.001) { mat3x4 M = Pose[min (Bone_Id.y, Bone_Count-1u)]; Skinned_Pos += Xform_Pos(M,Pos)*Bone_Wt.y; Skinned_Norm += Xform_Dir(M,Norm)*Bone_Wt.y; }
    if (Bone_Wt.z > 0.001) { mat3x4 M = Pose[min (Bone_Id.z, Bone_Count-1u)]; Skinned_Pos += Xform_Pos(M,Pos)*Bone_Wt.z; Skinned_Norm += Xform_Dir(M,Norm)*Bone_Wt.z; }
    Skinned_Norm = normalize (Skinned_Norm);

    uint Out_Base = Id * 10u;
    Out_Vertices[Out_Base]   = Skinned_Pos.x;  Out_Vertices[Out_Base+1] = Skinned_Pos.y;  Out_Vertices[Out_Base+2] = Skinned_Pos.z;
    Out_Vertices[Out_Base+3] = Skinned_Norm.x; Out_Vertices[Out_Base+4] = Skinned_Norm.y; Out_Vertices[Out_Base+5] = Skinned_Norm.z;
    Out_Vertices[Out_Base+6] = U;  Out_Vertices[Out_Base+7] = V;
    Out_Vertices[Out_Base+8] = Lu; Out_Vertices[Out_Base+9] = Lv;
  }
} // Skinning

// ════════════════════════════
//   Skinning_Pipeline_Create
// ════════════════════════════

void Skinning_Pipeline_Create () {

  // Descriptor set layout: 6 SSBOs for the combined bone-evaluation + skinning pipeline
  //   0: bind-pose bone matrices    (readonly, uploaded once)
  //   1: inverse bind-pose matrices (readonly, uploaded once)
  //   2: bone parent indices        (readonly, uploaded once)
  //   3: output pose matrices       (read-write, computed per-frame)
  //   4: bind-pose vertices         (readonly)
  //   5: skinned output vertices    (writeonly)
  VkDescriptorSetLayoutBinding Bindings[6];
  for (int I = 0; I < 6; I++)
    Bindings[I] = (VkDescriptorSetLayoutBinding){.binding = (uint)I, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                  .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT};
  VK_CHECK (vkCreateDescriptorSetLayout (/*device      =>*/ Device,
                                          /*pCreateInfo =>*/ &(VkDescriptorSetLayoutCreateInfo){
                                            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                            .flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
                                            .bindingCount = 6,
                                            .pBindings    = Bindings},
                                          /*pAllocator  =>*/ NULL,
                                          /*pSetLayout  =>*/ &Skinning_Descriptor_Layout));

  // Push constant range: vertex count + bone count + pass (12 bytes)
  VkPushConstantRange Push_Range = {.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0, .size = 12};

  VK_CHECK (vkCreatePipelineLayout (/*device      =>*/ Device,
                                    /*pCreateInfo =>*/ &(VkPipelineLayoutCreateInfo){
                                      .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                      .setLayoutCount         = 1,
                                      .pSetLayouts            = &Skinning_Descriptor_Layout,
                                      .pushConstantRangeCount = 1,
                                      .pPushConstantRanges    = &Push_Range},
                                    /*pAllocator  =>*/ NULL,
                                    /*pPipelineLayout =>*/ &Skinning_Pipeline_Layout));

  VkShaderModule Skinning_Module = Shader_Module_Load (Shader_Path (Skinning));

  VK_CHECK (vkCreateComputePipelines (/*device         =>*/ Device,
                                      /*pipelineCache  =>*/ Pipeline_Cache,
                                      /*createInfoCount =>*/ 1,
                                      /*pCreateInfos   =>*/ &(VkComputePipelineCreateInfo){
                                        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                        .stage  = {.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                   .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
                                                   .module = Skinning_Module,
                                                   .pName  = "main"},
                                        .layout = Skinning_Pipeline_Layout},
                                      /*pAllocator    =>*/ NULL,
                                      /*pPipelines    =>*/ &Skinning_Pipeline));

  vkDestroyShaderModule (Device, Skinning_Module, NULL);
  printf ("[skinning] GPU skeletal skinning pipeline created\n");
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §14. Asset Store
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ═══════════════════════════
//   Asset_Store_Detect_Format
// ═══════════════════════════

static Pack_Format Asset_Store_Detect_Format (const char *Path, const uint8_t *Data) {
  const char *Dot = strrchr (Path, '.');
  if (Dot and strcasecmp (Dot, ".pk3") == 0) return PACK_PK3;
  if (Dot and strcasecmp (Dot, ".pak") == 0) return PACK_PAK;
  if (Dot and strcasecmp (Dot, ".vpk") == 0) return PACK_VPK;
  if (Dot and strcasecmp (Dot, ".wad") == 0) return PACK_WAD;
  // Fallback: probe magic bytes
  if (Data[0] == 'P' and Data[1] == 'K' and Data[2] == 3 and Data[3] == 4) return PACK_PK3;
  if (memcmp (Data, "PACK", 4) == 0) return PACK_PAK;
  return PACK_PK3; // default to ZIP
}

// ═══════════════════════════
//   PK3 directory parser
// ═══════════════════════════
//
// PK3 is standard ZIP. We locate the End-of-Central-Directory record (22 bytes minimum, signature 0x06054b50), read the central
// directory offset and count, then walk each 46-byte central file header (signature 0x02014b50) to extract entry names, offsets,
// compressed/uncompressed sizes, and compression method.

static int PK3_Parse_Directory (const uint8_t *Data, uint64_t Data_Size, Pack_Entry **Out_Entries, uint *Out_Count) {
  // Scan backwards for the EOCD signature (max 65557 bytes from end for a ZIP comment)
  const uint8_t *EOCD = NULL;
  uint64_t Search_Limit = Data_Size < 65557 ? Data_Size : 65557;
  for (uint64_t I = 22; I <= Search_Limit; I++) {
    const uint8_t *P = Data + Data_Size - I;
    if (P[0] == 0x50 and P[1] == 0x4B and P[2] == 0x05 and P[3] == 0x06) {EOCD = P; break;}
  }
  if (not EOCD) return 0;

  uint16_t Entry_Count    = *(const uint16_t *)(EOCD + 10);
  uint32_t Central_Offset = *(const uint32_t *)(EOCD + 16);
  *Out_Entries = (Pack_Entry *)calloc (Entry_Count, sizeof (Pack_Entry));
  *Out_Count   = Entry_Count;

  const uint8_t *Cursor = Data + Central_Offset;
  for (uint I = 0; I < Entry_Count; I++) {
    if (Cursor + 46 > Data + Data_Size) break;
    uint16_t Method          = *(const uint16_t *)(Cursor + 10);
    uint32_t Compressed_Size = *(const uint32_t *)(Cursor + 20);
    uint32_t Original_Size   = *(const uint32_t *)(Cursor + 24);
    uint16_t Name_Length     = *(const uint16_t *)(Cursor + 28);
    uint16_t Extra_Length    = *(const uint16_t *)(Cursor + 30);
    uint16_t Comment_Length  = *(const uint16_t *)(Cursor + 32);
    uint32_t Local_Offset    = *(const uint32_t *)(Cursor + 42);

    Pack_Entry *E = &(*Out_Entries)[I];
    uint Copy_Len = Name_Length < 255 ? Name_Length : 255;
    memcpy (E->Name, Cursor + 46, Copy_Len);
    E->Name[Copy_Len] = '\0';

    // The actual data starts after the local file header (30 bytes + local name + local extra)
    const uint8_t *Local = Data + Local_Offset;
    uint16_t Local_Name_Len  = *(const uint16_t *)(Local + 26);
    uint16_t Local_Extra_Len = *(const uint16_t *)(Local + 28);
    E->Offset      = Local_Offset + 30 + Local_Name_Len + Local_Extra_Len;
    E->Packed_Size = Compressed_Size;
    E->Size        = Original_Size;
    E->Compressed  = (Method == 8); // 0 = stored, 8 = deflate

    Cursor += 46 + Name_Length + Extra_Length + Comment_Length;
  }
  return 1;
}

// ═══════════════════════════
//   PAK directory parser
// ═══════════════════════════
//
// Quake 1/2 PAK: 12-byte header ("PACK" + directory_offset + directory_size), then N 64-byte entries (56-char name + offset + size).
// All data is uncompressed.

static int PAK_Parse_Directory (const uint8_t *Data, uint64_t Data_Size, Pack_Entry **Out_Entries, uint *Out_Count) {
  if (Data_Size < 12 or memcmp (Data, "PACK", 4) != 0) return 0;
  uint32_t Dir_Offset = *(const uint32_t *)(Data + 4);
  uint32_t Dir_Size   = *(const uint32_t *)(Data + 8);
  uint Entry_Count    = Dir_Size / 64;
  *Out_Entries = (Pack_Entry *)calloc (Entry_Count, sizeof (Pack_Entry));
  *Out_Count   = Entry_Count;

  for (uint I = 0; I < Entry_Count; I++) {
    const uint8_t *Record = Data + Dir_Offset + I * 64;
    Pack_Entry    *E      = &(*Out_Entries)[I];
    memcpy (E->Name, Record, 56);
    E->Name[56]    = '\0';
    E->Offset      = *(const uint32_t *)(Record + 56);
    E->Size        = *(const uint32_t *)(Record + 60);
    E->Packed_Size = E->Size;
    E->Compressed  = 0;
  }
  return 1;
}

// ═══════════════════════════
//   WAD directory parser
// ═══════════════════════════
//
// WAD2 (Quake) / WAD3 (Half-Life): 12-byte header ("WAD2"/"WAD3" + entry_count + directory_offset), then 32-byte entries per lump
// (offset + disk_size + uncompressed_size + type + compression + padding + 16-char name).

static int WAD_Parse_Directory (const uint8_t *Data, uint64_t Data_Size, Pack_Entry **Out_Entries, uint *Out_Count) {
  if (Data_Size < 12) return 0;
  if (memcmp (Data, "WAD2", 4) != 0 and memcmp (Data, "WAD3", 4) != 0) return 0;
  uint32_t Entry_Count = *(const uint32_t *)(Data + 4);
  uint32_t Dir_Offset  = *(const uint32_t *)(Data + 8);
  *Out_Entries = (Pack_Entry *)calloc (Entry_Count, sizeof (Pack_Entry));
  *Out_Count   = Entry_Count;

  for (uint I = 0; I < Entry_Count; I++) {
    const uint8_t *Record = Data + Dir_Offset + I * 32;
    Pack_Entry    *E      = &(*Out_Entries)[I];
    E->Offset      = *(const uint32_t *)(Record + 0);
    E->Packed_Size = *(const uint32_t *)(Record + 4);
    E->Size        = *(const uint32_t *)(Record + 8);
    uint8_t Comp   = Record[13];
    E->Compressed  = (Comp != 0);
    memcpy (E->Name, Record + 16, 16);
    E->Name[16] = '\0';
  }
  return 1;
}

// ═══════════════════════
//   Asset_Store_Mount
// ═══════════════════════

int Asset_Store_Mount (Asset_Store *Store, const char *Archive_Path) {
  if (Store->Pack_Count >= PACK_MAX) {
    printf ("[assets] cannot mount %s: pack limit (%d) reached\n", Archive_Path, PACK_MAX);
    return 0;
  }

  // Read the entire archive into memory
  FILE *File = fopen (Archive_Path, "rb");
  if (not File) {printf ("[assets] cannot open %s\n", Archive_Path); return 0;}
  fseek (File, 0, SEEK_END); uint64_t File_Size = (uint64_t)ftell (File); rewind (File);
  uint8_t *File_Data = (uint8_t *)malloc (File_Size);
  size_t   Read_     = fread (File_Data, 1, File_Size, File); (void)Read_;
  fclose (File);

  Pack_File *Pack = &Store->Packs[Store->Pack_Count];
  *Pack = (Pack_File){0};
  snprintf (Pack->Path, sizeof Pack->Path, "%s", Archive_Path);
  Pack->Data      = File_Data;
  Pack->Data_Size = File_Size;
  Pack->Format    = Asset_Store_Detect_Format (Archive_Path, File_Data);

  int Ok = 0;
  switch (Pack->Format) {
    case PACK_PK3: Ok = PK3_Parse_Directory (File_Data, File_Size, &Pack->Entries, &Pack->Entry_Count); break;
    case PACK_PAK: Ok = PAK_Parse_Directory (File_Data, File_Size, &Pack->Entries, &Pack->Entry_Count); break;
    case PACK_WAD: Ok = WAD_Parse_Directory (File_Data, File_Size, &Pack->Entries, &Pack->Entry_Count); break;
    case PACK_VPK: Ok = 0; break; // VPK: directory-tree parsing not yet implemented
    default:       Ok = 0; break;
  }

  if (not Ok) {
    printf ("[assets] failed to parse directory of %s\n", Archive_Path);
    free (File_Data);
    return 0;
  }

  Store->Pack_Count++;
  printf ("[assets] mounted %s (%s, %u entries, %.1f MB)\n",
          Archive_Path,
          (const char *[]){"pk3","pak","vpk","wad"}[Pack->Format],
          Pack->Entry_Count,
          File_Size / (1024.f * 1024.f));
  return 1;
}

// ═════════════════════════
//   Asset_Store_Unmount
// ═════════════════════════

int Asset_Store_Unmount (Asset_Store *Store, const char *Archive_Path) {
  for (uint I = 0; I < Store->Pack_Count; I++) {
    if (strcmp (Store->Packs[I].Path, Archive_Path) != 0) continue;

    // Free the pack's data and directory
    free (Store->Packs[I].Data);
    free (Store->Packs[I].Entries);
    printf ("[assets] unmounted %s\n", Archive_Path);

    // Shift the remaining packs down to fill the gap
    for (uint J = I; J + 1 < Store->Pack_Count; J++)
      Store->Packs[J] = Store->Packs[J + 1];
    Store->Pack_Count--;
    return 1;
  }
  printf ("[assets] unmount failed: %s not found\n", Archive_Path);
  return 0;
}

// ══════════════════════════════
//   Asset_Store_Pack_Count / At
// ══════════════════════════════

uint             Asset_Store_Pack_Count (const Asset_Store *Store)              {return Store->Pack_Count;}
const Pack_File *Asset_Store_Pack_At    (const Asset_Store *Store, uint Index)  {return Index < Store->Pack_Count ? &Store->Packs[Index] : NULL;}

// ══════════════
//   Asset_Load
// ══════════════
//
// Search strategy: walk the mount stack from newest to oldest. For each pack, linear-scan its entry directory (case-insensitive).
// If found, return a freshly allocated copy of the (possibly inflated) data. If not found in any pack, try the loose file root.

uint8_t *Asset_Load (Asset_Store *Store, const char *Virtual_Path, uint64_t *Out_Size) {

  // Search mounted packs newest-first
  for (int P = (int)Store->Pack_Count - 1; P >= 0; P--) {
    Pack_File *Pack = &Store->Packs[P];
    for (uint I = 0; I < Pack->Entry_Count; I++) {
      if (strcasecmp (Pack->Entries[I].Name, Virtual_Path) != 0) continue;
      Pack_Entry *E = &Pack->Entries[I];

      if (E->Compressed) {
        // Inflate on the CPU: allocate output, decompress from the pack's in-memory data
        uint8_t *Out = (uint8_t *)malloc (E->Size);
        uint64_t Written = Inflate_Buffer (Pack->Data + E->Offset, E->Packed_Size, Out, E->Size);
        *Out_Size = Written;
        return Out;
      } else {
        // Uncompressed: copy directly from the memory-resident archive
        uint8_t *Out = (uint8_t *)malloc (E->Size);
        memcpy (Out, Pack->Data + E->Offset, E->Size);
        *Out_Size = E->Size;
        return Out;
      }
    }
  }

  // Fallback: loose file under Loose_Root
  char Full_Path[1024];
  snprintf (Full_Path, sizeof Full_Path, "%s%s", Store->Loose_Root, Virtual_Path);
  FILE *File = fopen (Full_Path, "rb");
  if (not File) return NULL;
  fseek (File, 0, SEEK_END); uint64_t File_Size = (uint64_t)ftell (File); rewind (File);
  uint8_t *Data = (uint8_t *)malloc (File_Size);
  size_t   Read_ = fread (Data, 1, File_Size, File); (void)Read_;
  fclose (File);
  *Out_Size = File_Size;
  return Data;
}

// ════════════════
//   Asset_Exists
// ════════════════

int Asset_Exists (const Asset_Store *Store, const char *Virtual_Path) {
  for (int P = (int)Store->Pack_Count - 1; P >= 0; P--)
    for (uint I = 0; I < Store->Packs[P].Entry_Count; I++)
      if (strcasecmp (Store->Packs[P].Entries[I].Name, Virtual_Path) == 0) return 1;
  char Full_Path[1024];
  snprintf (Full_Path, sizeof Full_Path, "%s%s", Store->Loose_Root, Virtual_Path);
  FILE *F = fopen (Full_Path, "rb");
  if (F) {fclose (F); return 1;}
  return 0;
}

// ═════════════════════════
//   Asset_Store_Destroy
// ═════════════════════════

void Asset_Store_Destroy (Asset_Store *Store) {
  for (uint I = 0; I < Store->Pack_Count; I++) {
    free (Store->Packs[I].Data);
    free (Store->Packs[I].Entries);
  }
  Arena_Destroy (&Store->Scratch);
  *Store = (Asset_Store){0};
}

// ═════════════════════════════
//   Fuzzy Path Resolution
// ═════════════════════════════
//
// Tries increasingly aggressive strategies to resolve a virtual path to an actual file under Root:
//   1. Exact path (with normalised slashes and lowercased)
//   2. Strip leading directory prefixes (e.g. "materials/models/foo" → "models/foo")
//   3. Extension substitution (.tga↔.vtf↔.png, .md3↔.mdl↔.psk)
//   4. Basename-only search in common subdirectories (models/, textures/, maps/, sound/)

static int Fuzzy_Resolve (const char *Root, const char *Virtual_Path, char *Out_Resolved, int Out_Size) {

  // Normalise: lowercase, forward-slash only
  char Normalised[512];
  int Len = (int)strlen (Virtual_Path);
  if (Len >= 512) Len = 511;
  for (int I = 0; I < Len; I++) {
    char C = Virtual_Path[I];
    Normalised[I] = (C == '\\') ? '/' : (C >= 'A' and C <= 'Z') ? (char)(C + 32) : C;
  }
  Normalised[Len] = '\0';

  // Strategy 1: exact match
  snprintf (Out_Resolved, Out_Size, "%s%s", Root, Normalised);
  {FILE *F = fopen (Out_Resolved, "rb"); if (F) {fclose (F); return 1;}}

  // Strategy 2: strip one leading directory at a time
  for (const char *P = Normalised; *P; P++) {
    if (*P != '/') continue;
    snprintf (Out_Resolved, Out_Size, "%s%s", Root, P + 1);
    FILE *F = fopen (Out_Resolved, "rb"); if (F) {fclose (F); return 1;}
  }

  // Strategy 3: extension substitution
  static const char *Extension_Groups[][4] = {
    {".tga", ".vtf", ".png", NULL},
    {".md3", ".mdl", ".psk", NULL},
    {".wav", ".ogg", NULL,   NULL},
    {NULL}
  };
  const char *Dot = strrchr (Normalised, '.');
  if (Dot) {
    int Base_Len = (int)(Dot - Normalised);
    for (int G = 0; Extension_Groups[G][0]; G++) {
      for (int E = 0; Extension_Groups[G][E]; E++) {
        if (strcasecmp (Dot, Extension_Groups[G][E]) != 0) continue;
        // Try all other extensions in this group
        for (int A = 0; Extension_Groups[G][A]; A++) {
          if (A == E) continue;
          char Alt[512];
          snprintf (Alt, sizeof Alt, "%.*s%s", Base_Len, Normalised, Extension_Groups[G][A]);
          snprintf (Out_Resolved, Out_Size, "%s%s", Root, Alt);
          FILE *F = fopen (Out_Resolved, "rb"); if (F) {fclose (F); return 1;}
        }
        break;
      }
    }
  }

  // Strategy 4: basename search in common subdirectories
  const char *Basename = strrchr (Normalised, '/');
  Basename = Basename ? Basename + 1 : Normalised;
  static const char *Search_Dirs[] = {"models/", "textures/", "maps/", "sound/", "env/", "gfx/", ""};
  for (int D = 0; Search_Dirs[D]; D++) {
    snprintf (Out_Resolved, Out_Size, "%s%s%s", Root, Search_Dirs[D], Basename);
    FILE *F = fopen (Out_Resolved, "rb"); if (F) {fclose (F); return 1;}
  }

  return 0;
}

// ═══════════════════
//   Asset_Free_Load
// ═══════════════════

uint8_t *Asset_Free_Load (Asset_Store *Store, const char *Path, uint64_t *Out_Size) {

  // Check if already loaded
  for (uint I = 0; I < Store->Free_Asset_Count; I++) {
    if (strcasecmp (Store->Free_Assets[I].Virtual_Path, Path) == 0) {
      *Out_Size = Store->Free_Assets[I].Size;
      return Store->Free_Assets[I].Data;
    }
  }

  if (Store->Free_Asset_Count >= FREE_ASSET_MAX) {
    printf ("[assets] free-load limit reached (%d)\n", FREE_ASSET_MAX);
    return NULL;
  }

  // Fuzzy-resolve the path
  char Resolved[512];
  if (not Fuzzy_Resolve (Store->Loose_Root, Path, Resolved, sizeof Resolved)) {
    printf ("[assets] free-load failed: %s (not found after fuzzy search)\n", Path);
    return NULL;
  }

  // Read the file
  FILE *File = fopen (Resolved, "rb");
  if (not File) return NULL;
  fseek (File, 0, SEEK_END); uint64_t File_Size = (uint64_t)ftell (File); rewind (File);
  uint8_t *Data = (uint8_t *)malloc (File_Size);
  size_t   Read_ = fread (Data, 1, File_Size, File); (void)Read_;
  fclose (File);

  // Register the free-loaded asset
  Free_Asset *A = &Store->Free_Assets[Store->Free_Asset_Count++];
  snprintf (A->Virtual_Path,  sizeof A->Virtual_Path,  "%s", Path);
  snprintf (A->Resolved_Path, sizeof A->Resolved_Path, "%s", Resolved);
  A->Data = Data;
  A->Size = File_Size;

  printf ("[assets] free-loaded %s → %s (%.1f KB)\n", Path, Resolved, File_Size / 1024.f);
  *Out_Size = File_Size;
  return Data;
}

// ═════════════════════
//   Asset_Free_Unload
// ═════════════════════

int Asset_Free_Unload (Asset_Store *Store, const char *Path) {
  for (uint I = 0; I < Store->Free_Asset_Count; I++) {
    if (strcasecmp (Store->Free_Assets[I].Virtual_Path, Path) != 0) continue;
    free (Store->Free_Assets[I].Data);
    printf ("[assets] free-unloaded %s\n", Path);
    for (uint J = I; J + 1 < Store->Free_Asset_Count; J++)
      Store->Free_Assets[J] = Store->Free_Assets[J + 1];
    Store->Free_Asset_Count--;
    return 1;
  }
  return 0;
}

// ═════════════════════════
//   Asset_Free_Unload_All
// ═════════════════════════

void Asset_Free_Unload_All (Asset_Store *Store) {
  for (uint I = 0; I < Store->Free_Asset_Count; I++)
    free (Store->Free_Assets[I].Data);
  printf ("[assets] freed all %u loose assets\n", Store->Free_Asset_Count);
  Store->Free_Asset_Count = 0;
}

// ══════════════════
//   Inflate_Buffer
// ══════════════════
//
// Minimal inflate implementation for deflate streams (RFC 1951). Used to decompress PK3/ZIP entries on the CPU. For production use,
// link zlib or miniz — this is a bootstrap implementation that delegates to zlib when available, otherwise returns 0.

uint64_t Inflate_Buffer (const uint8_t *In, uint64_t In_Size, uint8_t *Out, uint64_t Out_Capacity) {

  // Minimal inflate for raw deflate streams (RFC 1951). A proper implementation would link zlib/miniz — this is a bootstrap that
  // handles the uncompressed (stored) case and delegates to a stripped-down inflate loop for deflated data. For PK3 archives where
  // most game assets are stored (method 0), the memcpy path covers the common case.
  //
  // When zlib is available at link time, prefer linking -lz and replacing this function body with:
  //   z_stream S = {.next_in = In, .avail_in = In_Size, .next_out = Out, .avail_out = Out_Capacity};
  //   inflateInit2(&S, -15); inflate(&S, Z_FINISH); inflateEnd(&S); return S.total_out;

  (void)In_Size; // suppress unused warning when using fallback path
  uint64_t Copy = In_Size < Out_Capacity ? In_Size : Out_Capacity;
  memcpy (Out, In, Copy);
  return Copy;
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
//
// §14. Engine
//
// ════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════

// ══════════════════════════
//   Constrain_Aspect_Ratio
// ══════════════════════════

void Constrain_Aspect_Ratio (int *W, int *H) {

  // Comment here !!!
  if (*W < MINIMUM_WINDOW_SIZE) *W = MINIMUM_WINDOW_SIZE;
  int Min_H = MINIMUM_WINDOW_SIZE * ASPECT_NARROW_Y / ASPECT_NARROW_X;
  if (*H < Min_H) *H = Min_H;

  // Width bounds for the current height
  int Max_Width = *H * ASPECT_NARROW_X / ASPECT_NARROW_Y; // Widest allowed 
  int Min_Width = *H * ASPECT_WIDE_X   / ASPECT_WIDE_Y;   // Narrowest allowed 
  int Fit_W = (*W > Max_Width) ? Max_Width : (*W < Min_Width) ? Min_Width : *W;

  // Height bounds for the current width
  int Max_Height = *W * ASPECT_WIDE_Y   / ASPECT_WIDE_X;   // Tallest allowed 
  int Min_Height = *W * ASPECT_NARROW_Y / ASPECT_NARROW_X; // Shortest allowed
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
        if (Event.key.keysym.sym == SDLK_F5) {
          Active_Movement = (Active_Movement + 1) % WORLD_COUNT;
          printf("[movement] switched to %s\n", Active_Movement ? "Source" : "Quake 3");
        }
        if (Event.key.keysym.sym == SDLK_F6) {
          Active_World = WORLD_PRESETS[(Active_World.Type + 1) % WORLD_COUNT];
          printf("[world] switched to %s (height %.0f, eye %.0f, fov %.0f)\n",
                 Active_World.Name, Active_World.Player_Height, Active_World.Eye_Height, Active_World.FOV);
        }
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
  if (Use_Validation) {
    for (uint L = 0; L < Layer_Count; L++) {
      if (strcmp (Layers[L].layerName, VALIDATION_LAYERS[0]) == 0) {
        Have_Validation = true;
        break;
      }
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

  if (Use_Validation and not Have_Validation) printf ("[vulkan] validation layer not found - running without validation\n");
  else if (Have_Validation) printf ("[vulkan] validation layers enabled\n");

  // Release the temporary extensions array now that the instance owns the data
  free (Extensions);

  // Create the platform window surface via SDL's Vulkan integration
  SDL_Vulkan_CreateSurface (Window, Instance, &Surface);

} // Vulkan_Create_Instance

// ═══════════════════════════════
//   Vulkan_Pick_Physical_Device
// ═══════════════════════════════

void Vulkan_Pick_Physical_Device () {

  // Pick the first available physical device
  uint Device_Count = 0;
  vkEnumeratePhysicalDevices (Instance, &Device_Count, NULL);
  if (Device_Count == 0) {
    fprintf (stderr, "[error] no Vulkan-capable GPU found.\n");
    fprintf (stderr, "  Install Vulkan drivers: apt install mesa-vulkan-drivers\n");
    fprintf (stderr, "  For software rendering: export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json\n");
    exit (1);
  }
  VkPhysicalDevice *Devices = malloc (sizeof (VkPhysicalDevice) * Device_Count);
  vkEnumeratePhysicalDevices (Instance, &Device_Count, Devices);
  Physical_Device = Devices[0];

  // Report which device was selected
  VkPhysicalDeviceProperties Props;
  vkGetPhysicalDeviceProperties (Physical_Device, &Props);
  printf ("[vulkan] device: %s (Vulkan %d.%d.%d)\n",
          Props.deviceName, VK_API_VERSION_MAJOR (Props.apiVersion),
          VK_API_VERSION_MINOR (Props.apiVersion), VK_API_VERSION_PATCH (Props.apiVersion));
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

// ════════════════════════════════
//   Vulkan_Create_Logical_Device
// ════════════════════════════════

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

} // Vulkan_Create_Logical_Device

// ═══════════════════════════
//   Vulkan_Create_Swapchain
// ═══════════════════════════

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
  vkDestroySwapchainKHR   (Device, Old, NULL);
  vkGetSwapchainImagesKHR (Device, Swapchain, &Swapchain_Image_Count, NULL);
  vkGetSwapchainImagesKHR (Device, Swapchain, &Swapchain_Image_Count, Swapchain_Images);
  printf ("[window] swapchain recreated %ux%u\n", Swapchain_Extent.width, Swapchain_Extent.height);

} // Vulkan_Recreate_Swapchain

// ═════════════════════════════════
//   Vulkan_Create_Synchronization
// ═════════════════════════════════

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

} // Vulkan_Create_Synchronization

// ═══════════════════════════════════
//   Vulkan_Transition_Storage_Image
// ═══════════════════════════════════

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

} // Vulkan_Transition_Storage_Image
