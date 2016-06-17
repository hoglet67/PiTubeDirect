#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "rpi-base.h"
#include "rpi-mailbox-interface.h"
#include "v3d.h"
#include "framebuffer.h"

// #define DEBUG_V3D


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
  *((*list)++) = (d) & 0xff;
  *((*list)++) = (d >> 8)  & 0xff;
}

static void addword(uint8_t **list, uint32_t d) {
  *((*list)++) = (d) & 0xff;
  *((*list)++) = (d >> 8)  & 0xff;
  *((*list)++) = (d >> 16) & 0xff;
  *((*list)++) = (d >> 24) & 0xff;
}

static void addfloat(uint8_t **list, float f) {
#if 0
  uint32_t d = *((uint32_t *)&f);
  *((*list)++) = (d) & 0xff;
  *((*list)++) = (d >> 8)  & 0xff;
  *((*list)++) = (d >> 16) & 0xff;
  *((*list)++) = (d >> 24) & 0xff;
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
void v3d_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, unsigned int colour) {
  int x, y;

  uint8_t *p = list;

  unsigned int w = fb_get_width();
  unsigned int h = fb_get_height();

  unsigned int bw = (w + 63) / 64;
  unsigned int bh = (h + 63) / 64;

// Configuration stuff
  // Tile Binning Configuration.
  //   Tile state data is 48 bytes per tile, I think it can be thrown away
  //   as soon as binning is finished.
  addbyte(&p, 112);
  addword(&p, bus_addr + 0x6200); // tile allocation memory address
  addword(&p, 0x8000); // tile allocation memory size
  addword(&p, bus_addr + 0x100); // Tile state data address
  addbyte(&p, bw); // 1920/64
  addbyte(&p, bh); // 1080/64 (16.875)
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
  addshort(&p, w); // width
  addshort(&p, h); // height

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
  addshort(&p, x1 << 4); // X in 12.4 fixed point
  addshort(&p, y1 << 4); // Y in 12.4 fixed point
  addfloat(&p, 1.0f); // Z
  addfloat(&p, 1.0f); // 1/W
  addfloat(&p, 1.0f); // Varying 0 (Red)
  addfloat(&p, 0.0f); // Varying 1 (Green)
  addfloat(&p, 0.0f); // Varying 2 (Blue)

  // Vertex: bottom left, Green
  addshort(&p, x2 << 4); // X in 12.4 fixed point
  addshort(&p, y2 << 4); // Y in 12.4 fixed point
  addfloat(&p, 1.0f); // Z
  addfloat(&p, 1.0f); // 1/W
  addfloat(&p, 0.0f); // Varying 0 (Red)
  addfloat(&p, 1.0f); // Varying 1 (Green)
  addfloat(&p, 0.0f); // Varying 2 (Blue)

  // Vertex: bottom right, Blue
  addshort(&p, x3 << 4); // X in 12.4 fixed point
  addshort(&p, y3 << 4); // Y in 12.4 fixed point
  addfloat(&p, 1.0f); // Z
  addfloat(&p, 1.0f); // 1/W
  addfloat(&p, 0.0f); // Varying 0 (Red)
  addfloat(&p, 0.0f); // Varying 1 (Green)
  addfloat(&p, 1.0f); // Varying 2 (Blue)

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
  addword(&p, fb_get_address()); // framebuffer addresss
  addshort(&p, w); // width
  addshort(&p, h); // height
  if (fb_get_bpp32()) {
     addbyte(&p, 0x04); // framebuffer mode (linear rgba8888)
  } else {
     addbyte(&p, 0x08); // framebuffer mode (linear bgr565)
  }
  addbyte(&p, 0x00);

  // Link all binned lists together
  for(x = 0; x < bw; x++) {
    for(y = 0; y < bh; y++) {

      // Load tile buffer general
      addbyte(&p, 29);
      addbyte(&p, 0x01); // format = raster format; buffer to load = colour
      if (fb_get_bpp32()) {
         addbyte(&p, 0x00); // Pixel colour format = rgba8888
      } else {
         addbyte(&p, 0x02); // Pixel colour format = bgr565
      }
      addword(&p, fb_get_address());

      // Tile Coordinates
      addbyte(&p, 115);
      addbyte(&p, x);
      addbyte(&p, y);
      
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
  v3d[V3D_CT0EA] = bus_addr + length;

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
  v3d[V3D_CT1EA] = bus_addr + 0xe200 + render_length;

  while(v3d[V3D_CT1CS] & 0x20);  

#ifdef DEBUG_V3D
  printf("V3D_CT0CS: 0x%08x, V3D_CT0CA: 0x%08x, V3D_CT0EA: 0x%08x\r\n", v3d[V3D_CT0CS], v3d[V3D_CT0CA], v3d[V3D_CT0EA]);
  printf("V3D_CT1CS: 0x%08x, V3D_CT1CA: 0x%08x, V3D_CT1EA: 0x%08x\r\n", v3d[V3D_CT1CS], v3d[V3D_CT1CA], v3d[V3D_CT1EA]);
#endif

  v3d[V3D_CT1CS] = 0x10;

}



int v3d_initialize() {

   RPI_PropertyInit();
   RPI_PropertyAddTag(TAG_ENABLE_GPU, 1);
   RPI_PropertyProcess();

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

void v3d_test() {
  unsigned int w = fb_get_width();
  unsigned int h = fb_get_height();
  printf("Drawing a %d x %d triangle\r\n", w, h);
  v3d_draw_triangle(w / 2, 0, 0, h-1, w-1, h-1, 0);
}

int v3d_close() {

   free_control_list();

   return 0;
}
