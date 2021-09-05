#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "../rpi-base.h"
#include "../rpi-mailbox-interface.h"

#include "v3d.h"
#include "framebuffer.h"

// #define DEBUG_V3D

#define MAGIC_COLOUR 0x12345678


// Defines for mapping memory
#define MEM_FLAG_DISCARDABLE      (1 << 0) // can be resized to 0 at any time. Use for cached data
#define MEM_FLAG_NORMAL           (0 << 2) // normal allocating alias. Don't use from ARM
#define MEM_FLAG_DIRECT           (1 << 2) // 0xC alias uncached
#define MEM_FLAG_COHERENT         (2 << 2) // 0x8 alias. Non-allocating in L2 but coherent
#define MEM_FLAG_L1_NONALLOCATING ((MEM_FLAG_DIRECT | MEM_FLAG_COHERENT)) // Allocating in L2
#define MEM_FLAG_ZERO             (1 << 4) // initialise buffer to all zeros
#define MEM_FLAG_NO_INIT          (1 << 5) // don't initialise (default is initialise to all ones
#define MEM_FLAG_HINT_PERMALOCK   (1 << 6) // Likely to be locked for long periods of time.

// Defines for v3d register offsets
#define V3D_IDENT0  (0x000>>2) // V3D Identification 0 (V3D block identity)
#define V3D_IDENT1  (0x004>>2) // V3D Identification 1 (V3D Configuration A)
#define V3D_IDENT2  (0x008>>2) // V3D Identification 1 (V3D Configuration B)

#define V3D_SCRATCH (0x010>>2) // Scratch Register

#define V3D_L2CACTL (0x020>>2) // 2 Cache Control
#define V3D_SLCACTL (0x024>>2) // Slices Cache Control

#define V3D_INTCTL  (0x030>>2) // Interrupt Control
#define V3D_INTENA  (0x034>>2) // Interrupt Enables
#define V3D_INTDIS  (0x038>>2) // Interrupt Disables

#define V3D_CT0CS   (0x100>>2) // Control List Executor Thread 0 Control and Status.
#define V3D_CT1CS   (0x104>>2) // Control List Executor Thread 1 Control and Status.
#define V3D_CT0EA   (0x108>>2) // Control List Executor Thread 0 End Address.
#define V3D_CT1EA   (0x10c>>2) // Control List Executor Thread 1 End Address.
#define V3D_CT0CA   (0x110>>2) // Control List Executor Thread 0 Current Address.
#define V3D_CT1CA   (0x114>>2) // Control List Executor Thread 1 Current Address.
#define V3D_CT00RA0 (0x118>>2) // Control List Executor Thread 0 Return Address.
#define V3D_CT01RA0 (0x11c>>2) // Control List Executor Thread 1 Return Address.
#define V3D_CT0LC   (0x120>>2) // Control List Executor Thread 0 List Counter
#define V3D_CT1LC   (0x124>>2) // Control List Executor Thread 1 List Counter
#define V3D_CT0PC   (0x128>>2) // Control List Executor Thread 0 Primitive List Counter
#define V3D_CT1PC   (0x12c>>2) // Control List Executor Thread 1 Primitive List Counter

#define V3D_PCS     (0x130>>2) // V3D Pipeline Control and Status
#define V3D_BFC     (0x134>>2) // Binning Mode Flush Count
#define V3D_RFC     (0x138>>2) // Rendering Mode Frame Count

#define V3D_BPCA    (0x300>>2) // Current Address of Binning Memory Pool
#define V3D_BPCS    (0x304>>2) // Remaining Size of Binning Memory Pool
#define V3D_BPOA    (0x308>>2) // Address of Overspill Binning Memory Block
#define V3D_BPOS    (0x30c>>2) // Size of Overspill Binning Memory Block
#define V3D_BXCF    (0x310>>2) // Binner Debug

#define V3D_SQRSV0  (0x410>>2) // Reserve QPUs 0-7
#define V3D_SQRSV1  (0x414>>2) // Reserve QPUs 8-15
#define V3D_SQCNTL  (0x418>>2) // QPU Scheduler Control

#define V3D_SRQPC   (0x430>>2) // QPU User Program Request Program Address
#define V3D_SRQUA   (0x434>>2) // QPU User Program Request Uniforms Address
#define V3D_SRQUL   (0x438>>2) // QPU User Program Request Uniforms Length
#define V3D_SRQCS   (0x43c>>2) // QPU User Program Request Control and Status

#define V3D_VPACNTL (0x500>>2) // VPM Allocator Control
#define V3D_VPMBASE (0x504>>2) // VPM base (user) memory reservation

#define V3D_PCTRC   (0x670>>2) // Performance Counter Clear
#define V3D_PCTRE   (0x674>>2) // Performance Counter Enables

#define V3D_PCTR0   (0x680>>2) // Performance Counter Count 0
#define V3D_PCTRS0  (0x684>>2) // Performance Counter Mapping 0
#define V3D_PCTR1   (0x688>>2) // Performance Counter Count 1
#define V3D_PCTRS1  (0x68c>>2) // Performance Counter Mapping 1
#define V3D_PCTR2   (0x690>>2) // Performance Counter Count 2
#define V3D_PCTRS2  (0x694>>2) // Performance Counter Mapping 2
#define V3D_PCTR3   (0x698>>2) // Performance Counter Count 3
#define V3D_PCTRS3  (0x69c>>2) // Performance Counter Mapping 3
#define V3D_PCTR4   (0x6a0>>2) // Performance Counter Count 4
#define V3D_PCTRS4  (0x6a4>>2) // Performance Counter Mapping 4
#define V3D_PCTR5   (0x6a8>>2) // Performance Counter Count 5
#define V3D_PCTRS5  (0x6ac>>2) // Performance Counter Mapping 5
#define V3D_PCTR6   (0x6b0>>2) // Performance Counter Count 6
#define V3D_PCTRS6  (0x6b4>>2) // Performance Counter Mapping 6
#define V3D_PCTR7   (0x6b8>>2) // Performance Counter Count 7
#define V3D_PCTRS7  (0x6bc>>2) // Performance Counter Mapping 7
#define V3D_PCTR8   (0x6c0>>2) // Performance Counter Count 8
#define V3D_PCTRS8  (0x6c4>>2) // Performance Counter Mapping 8
#define V3D_PCTR9   (0x6c8>>2) // Performance Counter Count 9
#define V3D_PCTRS9  (0x6cc>>2) // Performance Counter Mapping 9
#define V3D_PCTR10  (0x6d0>>2) // Performance Counter Count 10
#define V3D_PCTRS10 (0x6d4>>2) // Performance Counter Mapping 10
#define V3D_PCTR11  (0x6d8>>2) // Performance Counter Count 11
#define V3D_PCTRS11 (0x6dc>>2) // Performance Counter Mapping 11
#define V3D_PCTR12  (0x6e0>>2) // Performance Counter Count 12
#define V3D_PCTRS12 (0x6e4>>2) // Performance Counter Mapping 12
#define V3D_PCTR13  (0x6e8>>2) // Performance Counter Count 13
#define V3D_PCTRS13 (0x6ec>>2) // Performance Counter Mapping 13
#define V3D_PCTR14  (0x6f0>>2) // Performance Counter Count 14
#define V3D_PCTRS14 (0x6f4>>2) // Performance Counter Mapping 14
#define V3D_PCTR15  (0x6f8>>2) // Performance Counter Count 15
#define V3D_PCTRS15 (0x6fc>>2) // Performance Counter Mapping 15

#define V3D_DBGE    (0xf00>>2) // PSE Error Signals
#define V3D_FDBGO   (0xf04>>2) // FEP Overrun Error Signals
#define V3D_FDBGB   (0xf08>>2) // FEP Interface Ready and Stall Signals, FEP Busy Signals
#define V3D_FDBGR   (0xf0c>>2) // FEP Internal Ready Signals
#define V3D_FDBGS   (0xf10>>2) // FEP Internal Stall Input Signals

#define V3D_ERRSTAT (0xf20>>2) // Miscellaneous Error Signals (VPM, VDW, VCD, VCM, L2C)


// I/O access
volatile unsigned *v3d;

//#define GPU_ALLOC

#ifdef GPU_ALLOC

// The handle (used to free the control list buffer)
static unsigned int handle;

static unsigned int mem_alloc(unsigned int size, unsigned int align, unsigned int flags)
{
   rpi_mailbox_property_t *buf;
   RPI_PropertyInit();
   RPI_PropertyAddTag(TAG_ALLOCATE_MEMORY, size, align, flags);
   RPI_PropertyProcess();
   buf = RPI_PropertyGet(TAG_ALLOCATE_MEMORY);
   if (buf) {
      return buf->data.buffer_32[1];
   } else {
      printf("mem_alloc failed\r\n");
      return 0;
   }
}

static unsigned int mem_free(unsigned int handle)
{
   rpi_mailbox_property_t *buf;
   RPI_PropertyInit();
   RPI_PropertyAddTag(TAG_RELEASE_MEMORY, handle);
   RPI_PropertyProcess();
   buf = RPI_PropertyGet(TAG_RELEASE_MEMORY);
   if (buf) {
      return buf->data.buffer_32[1];
   } else {
      printf("mem_free failed\r\n");
      return 0;
   }
}

static unsigned int mem_lock(unsigned int handle)
{
   rpi_mailbox_property_t *buf;
   RPI_PropertyInit();
   RPI_PropertyAddTag(TAG_LOCK_MEMORY, handle);
   RPI_PropertyProcess();
   buf = RPI_PropertyGet(TAG_LOCK_MEMORY);
   if (buf) {
      return buf->data.buffer_32[1];
   } else {
      printf("mem_lock failed\r\n");
      return 0;
   }
}

static unsigned int mem_unlock(unsigned int handle)
{
   rpi_mailbox_property_t *buf;
   RPI_PropertyInit();
   RPI_PropertyAddTag(TAG_UNLOCK_MEMORY, handle);
   RPI_PropertyProcess();
   buf = RPI_PropertyGet(TAG_UNLOCK_MEMORY);
   if (buf) {
      return buf->data.buffer_32[1];
   } else {
      printf("mem_unlock failed\r\n");
      return 0;
   }
}

#else

static uint8_t v3d_buffer[1024*1048] __attribute__((aligned(0x1000)));

#endif

// Execute a nop control list to prove that we have contol.

static void addbyte(uint8_t **list, uint8_t d) {
  *((*list)++) = d;
}

static void addshort(uint8_t **list, uint16_t d) {
  *((*list)++) = (uint8_t)(d);
  *((*list)++) = (uint8_t)(d >> 8);
}

static void addword(uint8_t **list, uint32_t d) {
  *((*list)++) = (uint8_t)(d);
  *((*list)++) = (uint8_t)(d >> 8);
  *((*list)++) = (uint8_t)(d >> 16);
  *((*list)++) = (uint8_t)(d >> 24);
}

static void addfloat(uint8_t **list, float f) {
#if 0
  uint32_t d = *((uint32_t *)&f);
  *((*list)++) = (uint8_t)(d);
  *((*list)++) = (uint8_t)(d >> 8);
  *((*list)++) = (uint8_t)(d >> 16);
  *((*list)++) = (uint8_t)(d >> 24);
#endif
  uint8_t *d = (uint8_t *)&f;
  *((*list)++) = *d++;
  *((*list)++) = *d++;
  *((*list)++) = *d++;
  *((*list)++) = *d++;
}

// The Bus address of the control list buffer
static uint32_t bus_addr;

// The ARM virtual address of the control list buffer
static uint8_t *list;


static void allocate_control_list() {
#ifdef GPU_ALLOC
  // 8Mb, 4k alignment
  handle = mem_alloc(0x800000, 0x1000, MEM_FLAG_COHERENT | MEM_FLAG_ZERO);
  if (!handle) {
    printf("Error: Unable to allocate memory\r\n");
    return;
  }
  bus_addr = mem_lock(handle);
#else
  bus_addr = (uint32_t) v3d_buffer;
#endif

  list = (uint8_t*) (bus_addr & 0x3FFFFFFF);

  printf("V3D Control List bus addr = %08"PRIx32"\r\n", bus_addr);
  printf("V3D Control List arm addr = %08"PRIx32"\r\n", (uint32_t) list);
}

static void free_control_list() {
#ifdef GPU_ALLOC
  mem_unlock(handle);
  mem_free(handle);
#endif
}

// Render a single triangle to memory.
void v3d_draw_triangle(screen_mode_t *screen, int x1, int y1, int x2, int y2, int x3, int y3, unsigned int colour) {
  unsigned int x, y;

  uint8_t *p = list;

  unsigned int w = (unsigned int)screen->width;
  unsigned int h = (unsigned int)screen->height;

  unsigned int bw = (w + 63) / 64;
  unsigned int bh = (h + 63) / 64;

  float c1_r, c1_g, c1_b;
  float c2_r, c2_g, c2_b;
  float c3_r, c3_g, c3_b;

  // Calculate the vertex colours

  if (colour == MAGIC_COLOUR) {
     c1_r = c2_g = c3_b = 1.0;
     c1_g = c2_b = c3_r = 0.0;
     c1_b = c2_r = c3_g = 0.0;
  } else if (screen->log2bpp == 5) {
     c1_r = c2_r = c3_r = (float) ((colour >> 16) & 0xff) / 255.0f;
     c1_g = c2_g = c3_g = (float) ((colour >>  8) & 0xff) / 255.0f;
     c1_b = c2_b = c3_b = (float) ((colour      ) & 0xff) / 255.0f;
  } else if (screen->log2bpp == 4) {
     c1_r = c2_r = c3_r = (float) ((colour >> 11) & 0x1f) / 31.0f;
     c1_g = c2_g = c3_g = (float) ((colour >>  5) & 0x3f) / 63.0f;
     c1_b = c2_b = c3_b = (float) ((colour      ) & 0x1f) / 31.0f;
  } else {
     c1_r = c2_r = c3_r = (float) ((colour / 36) % 6) / 5.0f;
     c1_g = c2_g = c3_g = (float) ((colour /  6) % 6) / 5.0f;
     c1_b = c2_b = c3_b = (float) ( colour       % 6) / 5.0f;
  }

// Configuration stuff
  // Tile Binning Configuration.
  //   Tile state data is 48 bytes per tile, I think it can be thrown away
  //   as soon as binning is finished.
  addbyte(&p, 112);
  addword(&p, bus_addr + 0x6200); // tile allocation memory address
  addword(&p, 0x8000); // tile allocation memory size
  addword(&p, bus_addr + 0x100); // Tile state data address
  addbyte(&p, (uint8_t)bw); // 1920/64
  addbyte(&p, (uint8_t)bh); // 1080/64 (16.875)
  addbyte(&p, 0x04); // config

  // Start tile binning.
  addbyte(&p, 6);

  // Primitive type
  addbyte(&p, 56);
  addbyte(&p, 0x32); // 16 bit triangle

  // Clip Window
  addbyte(&p, 102);
  addshort(&p, 0);
  addshort(&p, 0);
  addshort(&p, (uint16_t)w); // width
  addshort(&p, (uint16_t)h); // height

  // State
  addbyte(&p, 96);
  addbyte(&p, 0x03); // enable both foward and back facing polygons
  addbyte(&p, 0x00); // depth testing disabled
  addbyte(&p, 0x02); // enable early depth write

  // Viewport offset
  addbyte(&p, 103);
  addshort(&p, 0);
  addshort(&p, 0);

// The triangle
  // No Vertex Shader state (takes pre-transformed vertexes,
  // so we don't have to supply a working coordinate shader to test the binner.
  addbyte(&p, 65);
  addword(&p, bus_addr + 0x80); // Shader Record

  // primitive index list
  addbyte(&p, 32);
  addbyte(&p, 0x04); // 8bit index, trinagles
  addword(&p, 3); // Length
  addword(&p, bus_addr + 0x70); // address
  addword(&p, 2); // Maximum index

// End of bin list
  // Flush
  addbyte(&p, 5);
  // Nop
  addbyte(&p, 1);
  // Halt
  addbyte(&p, 0);

  int length = p - list;
  //assert(length < 0x80);

// Shader Record
  p = list + 0x80;
  addbyte(&p, 0x01); // flags
  addbyte(&p, 6*4); // stride
  addbyte(&p, 0xcc); // num uniforms (not used)
  addbyte(&p, 3); // num varyings
  addword(&p, bus_addr + 0xfe00); // Fragment shader code
  addword(&p, bus_addr + 0xff00); // Fragment shader uniforms
  addword(&p, bus_addr + 0xa0); // Vertex Data

// Vertex Data
  p = list + 0xa0;
  // Vertex: Top, red
  addshort(&p, (uint16_t)(x1 << 4)); // X in 12.4 fixed point
  addshort(&p, (uint16_t)(y1 << 4)); // Y in 12.4 fixed point
  addfloat(&p, 1.0f); // Z
  addfloat(&p, 1.0f); // 1/W
  addfloat(&p, c1_r); // Varying 0 (Red)
  addfloat(&p, c1_g); // Varying 1 (Green)
  addfloat(&p, c1_b); // Varying 2 (Blue)

  // Vertex: bottom left, Green
  addshort(&p, (uint16_t)(x2 << 4)); // X in 12.4 fixed point
  addshort(&p, (uint16_t)(y2 << 4)); // Y in 12.4 fixed point
  addfloat(&p, 1.0f); // Z
  addfloat(&p, 1.0f); // 1/W
  addfloat(&p, c2_r); // Varying 0 (Red)
  addfloat(&p, c2_g); // Varying 1 (Green)
  addfloat(&p, c2_b); // Varying 2 (Blue)

  // Vertex: bottom right, Blue
  addshort(&p, (uint16_t)(x3 << 4)); // X in 12.4 fixed point
  addshort(&p, (uint16_t)(y3 << 4)); // Y in 12.4 fixed point
  addfloat(&p, 1.0f); // Z
  addfloat(&p, 1.0f); // 1/W
  addfloat(&p, c3_r); // Varying 0 (Red)
  addfloat(&p, c3_g); // Varying 1 (Green)
  addfloat(&p, c3_b); // Varying 2 (Blue)

// Vertex list
  p = list + 0x70;
  addbyte(&p, 0); // top
  addbyte(&p, 1); // bottom left
  addbyte(&p, 2); // bottom right

// fragment shader
  p = list + 0xfe00;
  addword(&p, 0x958e0dbf);
  addword(&p, 0xd1724823); /* mov r0, vary; mov r3.8d, 1.0 */
  addword(&p, 0x818e7176);
  addword(&p, 0x40024821); /* fadd r0, r0, r5; mov r1, vary */
  addword(&p, 0x818e7376);
  addword(&p, 0x10024862); /* fadd r1, r1, r5; mov r2, vary */
  addword(&p, 0x819e7540);
  addword(&p, 0x114248a3); /* fadd r2, r2, r5; mov r3.8a, r0 */
  addword(&p, 0x809e7009);
  addword(&p, 0x115049e3); /* nop; mov r3.8b, r1 */
  addword(&p, 0x809e7012);
  addword(&p, 0x116049e3); /* nop; mov r3.8c, r2 */
  addword(&p, 0x159e76c0);
  addword(&p, 0x30020ba7); /* mov tlbc, r3; nop; thrend */
  addword(&p, 0x009e7000);
  addword(&p, 0x100009e7); /* nop; nop; nop */
  addword(&p, 0x009e7000);
  addword(&p, 0x500009e7); /* nop; nop; sbdone */

#if 0
// fragment shader 2
  p = list + 0xfd00;
  addword(&p, 0x009e7000);
  addword(&p, 0x100009e7); /* nop; nop; nop */
  addword(&p,     colour); /* rgba <colour> */
  addword(&p, 0xe0020ba7); /* ldi tlbc, <colour> */
  addword(&p, 0x009e7000);
  addword(&p, 0x500009e7); /* nop; nop; sbdone */
  addword(&p, 0x009e7000);
  addword(&p, 0x300009e7); /* nop; nop; thrend */
  addword(&p, 0x009e7000);
  addword(&p, 0x100009e7); /* nop; nop; nop */
  addword(&p, 0x009e7000);
  addword(&p, 0x100009e7); /* nop; nop; nop */
#endif

// Render control list
  p = list + 0xe200;

  // Clear color
  addbyte(&p, 114);
  addword(&p, 0xff000000); // Opaque Black
  addword(&p, 0xff000000); // 32 bit clear colours need to be repeated twice
  addword(&p, 0);
  addbyte(&p, 0);

  // Tile Rendering Mode Configuration
  addbyte(&p, 113);
  addword(&p, get_fb_address()); // framebuffer addresss
  addshort(&p, (uint16_t)w); // width
  addshort(&p, (uint16_t)h); // height
  if (screen->log2bpp == 5) {
     addbyte(&p, 0x04); // framebuffer mode (linear rgba8888)
  } else if (screen->log2bpp == 4) {
     addbyte(&p, 0x08); // framebuffer mode (linear bgr565)
  } else {
     addbyte(&p, 0x08); // framebuffer mode (linear bgr565) TODO: This is wrong!
  }
  addbyte(&p, 0x00);

  // Link all binned lists together
  for(x = 0; x < bw; x++) {
    for(y = 0; y < bh; y++) {

      // Load tile buffer general
      addbyte(&p, 29);
      addbyte(&p, 0x01); // format = raster format; buffer to load = colour
      if (screen->log2bpp == 5) {
         addbyte(&p, 0x00); // Pixel colour format = rgba8888
      } else if (screen->log2bpp == 4) {
         addbyte(&p, 0x02); // Pixel colour format = bgr565
      } else {
         addbyte(&p, 0x02); // Pixel colour format = bgr565 TODO: This is wrong!
      }
      addword(&p, get_fb_address());

      // Tile Coordinates
      addbyte(&p, 115);
      addbyte(&p, (uint8_t)x);
      addbyte(&p, (uint8_t)y);

      // Call Tile sublist
      addbyte(&p, 17);
      addword(&p, bus_addr + 0x6200 + (y * bw + x) * 32);

      // Last tile needs a special store instruction
      if(x == bw - 1 && y == bh - 1) {
        // Store resolved tile color buffer and signal end of frame
        addbyte(&p, 25);
      } else {
        // Store resolved tile color buffer
        addbyte(&p, 24);
      }
    }
  }

  int render_length = p - (list + 0xe200);


// Run our control list
#ifdef XDEBUG_V3D
  printf("V3D Control List Constructed\r\n");
  printf("Start Address: 0x%08"PRIx32", length: 0x%x\r\n", bus_addr, length);
#endif

#ifdef DEBUG_V3D
  printf("V3D_CT0CS: 0x%08x, V3D_CT0CA: 0x%08x, V3D_CT0EA: 0x%08x\r\n", v3d[V3D_CT0CS], v3d[V3D_CT0CA], v3d[V3D_CT0EA]);
  printf("V3D_CT1CS: 0x%08x, V3D_CT1CA: 0x%08x, V3D_CT1EA: 0x%08x\r\n", v3d[V3D_CT1CS], v3d[V3D_CT1CA], v3d[V3D_CT1EA]);
#endif

  // Binning

  if (v3d[V3D_CT0CS] & 0x20) {
     printf("Waiting for V3D_CT0CS to complete\r\n");
     while(v3d[V3D_CT0CS] & 0x20);
  }

  v3d[V3D_CT0CA] = bus_addr;
  v3d[V3D_CT0EA] = bus_addr + (uint32_t)length;

  // Wait for control list to execute
  while(v3d[V3D_CT0CS] & 0x20);

#ifdef DEBUG_V3D
  printf("V3D_CT0CS: 0x%08x, V3D_CT0CA: 0x%08x, V3D_CT0EA: 0x%08x\r\n", v3d[V3D_CT0CS], v3d[V3D_CT0CA], v3d[V3D_CT0EA]);
  printf("V3D_CT1CS: 0x%08x, V3D_CT1CA: 0x%08x, V3D_CT1EA: 0x%08x\r\n", v3d[V3D_CT1CS], v3d[V3D_CT1CA], v3d[V3D_CT1EA]);
#endif

  // Rendering

  if (v3d[V3D_CT1CS] & 0x20) {
     printf("Waiting for V3D_CT1CS to complete\r\n");
     while(v3d[V3D_CT1CS] & 0x20);
  }

  v3d[V3D_CT1CA] = bus_addr + 0xe200;
  v3d[V3D_CT1EA] = bus_addr + 0xe200 + (uint32_t)render_length;

  while(v3d[V3D_CT1CS] & 0x20);

#ifdef DEBUG_V3D
  printf("V3D_CT0CS: 0x%08x, V3D_CT0CA: 0x%08x, V3D_CT0EA: 0x%08x\r\n", v3d[V3D_CT0CS], v3d[V3D_CT0CA], v3d[V3D_CT0EA]);
  printf("V3D_CT1CS: 0x%08x, V3D_CT1CA: 0x%08x, V3D_CT1EA: 0x%08x\r\n", v3d[V3D_CT1CS], v3d[V3D_CT1CA], v3d[V3D_CT1EA]);
#endif

  v3d[V3D_CT1CS] = 0x10;

}

int v3d_initialize(screen_mode_t *screen) {

   rpi_mailbox_property_t *buf;
   RPI_PropertyInit();
   RPI_PropertyAddTag(TAG_ENABLE_GPU, 1);
   RPI_PropertyProcess();
   buf = RPI_PropertyGet(TAG_ENABLE_GPU);
   if (buf) {
      printf("%08"PRIx32"\r\n", buf->data.buffer_32[0]);
   } else {
      printf("null response\r\n");

   }

   // map v3d's registers into our address space.
   v3d = (unsigned *) (PERIPHERAL_BASE + 0xc00000);

   printf("%08x\r\n", v3d[V3D_IDENT0]);

   if (v3d[V3D_IDENT0] != 0x02443356) { // Magic number.
      printf("Error: V3D pipeline isn't powered up and accessable.\r\n");
      return 1;
   }

   allocate_control_list();

   return 0;
}

void v3d_test(screen_mode_t *screen) {
  int w = screen->width;
  int h = screen->height;
  printf("Drawing a %d x %d triangle\r\n", w, h);
  v3d_draw_triangle(screen, w / 2, 0, 0, h-1, w-1, h-1, MAGIC_COLOUR);
}

int v3d_close(screen_mode_t *screen) {

   free_control_list();

   return 0;
}
