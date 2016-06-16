#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "rpi-base.h"
#include "rpi-mailbox-interface.h"
#include "v3d.h"
#include "framebuffer.h"

// I/O access
volatile unsigned *v3d;


unsigned int mem_alloc(unsigned int size, unsigned int align, unsigned int flags)
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

unsigned int mem_free(unsigned int handle)
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

unsigned int mem_lock(unsigned int handle)
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

unsigned int mem_unlock(unsigned int handle)
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

void *mapmem(unsigned int base, unsigned int size)
{
   return (void *) (base & 0x3FFFFFFF);
}

void *unmapmem(void *addr, unsigned int size)
{
   return addr;
}

// Execute a nop control list to prove that we have contol.

void addbyte(uint8_t **list, uint8_t d) {
  *((*list)++) = d;
}

void addshort(uint8_t **list, uint16_t d) {
  *((*list)++) = (d) & 0xff;
  *((*list)++) = (d >> 8)  & 0xff;
}

void addword(uint8_t **list, uint32_t d) {
  *((*list)++) = (d) & 0xff;
  *((*list)++) = (d >> 8)  & 0xff;
  *((*list)++) = (d >> 16) & 0xff;
  *((*list)++) = (d >> 24) & 0xff;
}

void addfloat(uint8_t **list, float f) {
  uint32_t d = *((uint32_t *)&f);
  *((*list)++) = (d) & 0xff;
  *((*list)++) = (d >> 8)  & 0xff;
  *((*list)++) = (d >> 16) & 0xff;
  *((*list)++) = (d >> 24) & 0xff;
}

// Render a single triangle to memory.
void testTriangle() {
  int x, y;

  // 8Mb, 4k alignment
  unsigned int handle = mem_alloc(0x800000, 0x1000, MEM_FLAG_COHERENT | MEM_FLAG_ZERO);
  if (!handle) {
    printf("Error: Unable to allocate memory\r\n");
    return;
  }
  uint32_t bus_addr = mem_lock(handle); 
  printf("bus_addr = %08"PRIx32"\r\n", bus_addr);

  uint8_t *list = (uint8_t*) mapmem(bus_addr, 0x800000);

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
  addshort(&p, (w/2) << 4); // X in 12.4 fixed point
  addshort(&p, (h/4) << 4); // Y in 12.4 fixed point
  addfloat(&p, 1.0f); // Z
  addfloat(&p, 1.0f); // 1/W
  addfloat(&p, 1.0f); // Varying 0 (Red)
  addfloat(&p, 0.0f); // Varying 1 (Green)
  addfloat(&p, 0.0f); // Varying 2 (Blue)

  // Vertex: bottom left, Green
  addshort(&p, (w/4) << 4); // X in 12.4 fixed point
  addshort(&p, (h*4/5) << 4); // Y in 12.4 fixed point
  addfloat(&p, 1.0f); // Z
  addfloat(&p, 1.0f); // 1/W
  addfloat(&p, 0.0f); // Varying 0 (Red)
  addfloat(&p, 1.0f); // Varying 1 (Green)
  addfloat(&p, 0.0f); // Varying 2 (Blue)

  // Vertex: bottom right, Blue
  addshort(&p, (w*3/4) << 4); // X in 12.4 fixed point
  addshort(&p, (h*4/5) << 4); // Y in 12.4 fixed point
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
  //addword(&p, bus_addr + 0x10000); 
  addword(&p, fb_get_address()); // framebuffer addresss
  addshort(&p, w); // width
  addshort(&p, h); // height
  //addbyte(&p, 0x04); // framebuffer mode (linear rgba8888)
  addbyte(&p, fb_get_mode());
  addbyte(&p, 0x00);

  // Do a store of the first tile to force the tile buffer to be cleared
  // Tile Coordinates
  addbyte(&p, 115);
  addbyte(&p, 0);
  addbyte(&p, 0);
  // Store Tile Buffer General
  addbyte(&p, 28);
  addshort(&p, 0); // Store nothing (just clear)
  addword(&p, 0); // no address is needed

  // Link all binned lists together
  for(x = 0; x < bw; x++) {
    for(y = 0; y < bh; y++) {

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
  printf("Binner control list constructed\r\n");
  printf("Start Address: 0x%08"PRIx32", length: 0x%x\r\n", bus_addr, length);

  // Binning

  printf("V3D_CT0CS: 0x%08x, Address: 0x%08x\r\n", v3d[V3D_CT0CS], v3d[V3D_CT0CA]);
  v3d[V3D_CT0CA] = bus_addr;
  v3d[V3D_CT0EA] = bus_addr + length;
  printf("V3D_CT0CS: 0x%08x, Address: 0x%08x\r\n", v3d[V3D_CT0CS], v3d[V3D_CT0CA]);

  // Wait for control list to execute
  while(v3d[V3D_CT0CS] & 0x20);  
  printf("V3D_CT0CS: 0x%08x, Address: 0x%08x\r\n", v3d[V3D_CT0CS], v3d[V3D_CT0CA]);

  // Rendering

  printf("V3D_CT1CS: 0x%08x, Address: 0x%08x\r\n", v3d[V3D_CT1CS], v3d[V3D_CT1CA]);
  v3d[V3D_CT1CA] = bus_addr + 0xe200;
  v3d[V3D_CT1EA] = bus_addr + 0xe200 + render_length;
  printf("V3D_CT1CS: 0x%08x, Address: 0x%08x\r\n", v3d[V3D_CT1CS], v3d[V3D_CT1CA]);

  while(v3d[V3D_CT1CS] & 0x20);  
  printf("V3D_CT1CS: 0x%08x, Address: 0x%08x\r\n", v3d[V3D_CT1CS], v3d[V3D_CT1CA]);
  v3d[V3D_CT1CS] = 0x20;

// Release resources
  unmapmem((void *) list, 0x800000);
  mem_unlock(handle);
  mem_free(handle);
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

  // We now have access to the v3d registers, we should do something.
  testTriangle();

  return 0;
}
