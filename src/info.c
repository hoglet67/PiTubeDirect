#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "info.h"
#include "tube-debug.h"

static void print_tag_value(const char *name, const rpi_mailbox_property_t *buf, int hex) {
   LOG_INFO("%20s : ", name);
   if (buf == NULL) {
      LOG_INFO("*** failed ***");
   } else {
      for (uint32_t i = 0;  i < (buf->byte_length + 3) >> 2; i++) {
         if (hex) {
            LOG_INFO("%08"PRIx32, buf->data.buffer_32[i]);
         } else {
            LOG_INFO("%8"PRId32, buf->data.buffer_32[i]);
         }
      }
   }
   LOG_INFO("\r\n");
}

uint32_t get_revision() {
   rpi_mailbox_property_t *buf;
   RPI_PropertyInit();
   RPI_PropertyAddTag(TAG_GET_BOARD_REVISION);
   RPI_PropertyProcess();
   buf = RPI_PropertyGet(TAG_GET_BOARD_REVISION);
   if (buf) {
      return buf->data.buffer_32[0];
   } else {
      return 0;
   }
}

uint32_t get_clock_rate(unsigned int clk_id) {
   rpi_mailbox_property_t *buf;
   RPI_PropertyInit();
   RPI_PropertyAddTag(TAG_GET_CLOCK_RATE_MEASURED, clk_id);
   RPI_PropertyProcess();
   buf = RPI_PropertyGet(TAG_GET_CLOCK_RATE_MEASURED);
   if (buf) {
      return buf->data.buffer_32[1];
   } else {
      return 0;
   }
}

static float get_temp() {
   rpi_mailbox_property_t *buf;
   RPI_PropertyInit();
   RPI_PropertyAddTag(TAG_GET_TEMPERATURE, 0);
   RPI_PropertyProcess();
   buf = RPI_PropertyGet(TAG_GET_TEMPERATURE);
   if (buf) {
      return ((float)buf->data.buffer_32[1]) / 1E3F;
   } else {
      return 0.0F;
   }
}

static float get_voltage(int component_id) {
   rpi_mailbox_property_t *buf;
   RPI_PropertyInit();
   RPI_PropertyAddTag(TAG_GET_VOLTAGE, component_id);
   RPI_PropertyProcess();
   buf = RPI_PropertyGet(TAG_GET_VOLTAGE);
   if (buf) {
      return ((float) buf->data.buffer_32[1]) / 1E6F;
   } else {
      return 0.0F;
   }
}

// Model
// Speed
// Temp

uint32_t get_speed() {
   return get_clock_rate(ARM_CLK_ID) / 1000000;
}

char *get_info_string() {
   __attribute__ ((section (".noinit"))) static char info_string[PROP_SIZE];
   static int read = 0;
   if (!read) {
      sprintf(info_string, "%"PRIx32" %"PRId32"/%"PRId32"MHz %2.1fC", get_revision(), get_clock_rate(ARM_CLK_ID) / 1000000, get_clock_rate(CORE_CLK_ID) / 1000000, (double)get_temp());
      read = 1;
   }
   return info_string;
}

static char *get_cmdline() {
   __attribute__ ((section (".noinit"))) static char cmdline[PROP_SIZE];
   static int read = 0;
   if (!read) {
      rpi_mailbox_property_t *buf;
      RPI_PropertyInit();
      RPI_PropertyAddTag(TAG_GET_COMMAND_LINE, 0);
      RPI_PropertyProcess();
      buf = RPI_PropertyGet(TAG_GET_COMMAND_LINE);
      if (buf) {
         memcpy(cmdline, buf->data.buffer_8, buf->byte_length);
         cmdline[buf->byte_length] = 0;
      } else {
         cmdline[0] = 0;
      }
      read = 1;
   }
   return cmdline;
}

extern char * strcasestr(const char *, const char *);

char *get_cmdline_prop(const char *prop) {
   __attribute__ ((section (".noinit"))) static char ret[PROP_SIZE];
   char *retptr = ret;
   char *cmdptr = get_cmdline();

   cmdptr = strcasestr(cmdptr, prop);

   if (cmdptr != 0) {
      size_t proplen = strlen(prop);
      // check for an equals in the expected place
      if (*(cmdptr + proplen) == '=') {
            // skip the equals
            cmdptr += proplen + 1;
            // copy the property value to the return buffer
            while (*cmdptr != ' ' && *cmdptr != '\0') {
               *retptr++ = *cmdptr++;
            }
            *retptr = '\0';
            return ret;
         }
   }
   return NULL;
}

clock_info_t * get_clock_rates(unsigned int clk_id) {
   static clock_info_t result;
   uint32_t *rp = (uint32_t *) &result;
   rpi_mailbox_tag_t tags[] = {
      TAG_GET_CLOCK_RATE_MEASURED,
      TAG_GET_MIN_CLOCK_RATE,
      TAG_GET_MAX_CLOCK_RATE
   };
   unsigned int i;
   unsigned int n = sizeof(tags) / sizeof(rpi_mailbox_tag_t);

   rpi_mailbox_property_t *buf;
   RPI_PropertyInit();
   for (i = 0; i < n; i++) {
      RPI_PropertyAddTag(tags[i], clk_id);
   }
   RPI_PropertyProcess();
   for (i = 0; i < n; i++) {
      buf = RPI_PropertyGet(tags[i]);
      *rp++ = buf ? buf->data.buffer_32[1] : 0;
   }
   return &result;
}

void dump_useful_info() {
   unsigned int i;
   rpi_mailbox_tag_t tags[] = {
        TAG_GET_FIRMWARE_VERSION
      , TAG_GET_BOARD_MODEL
      , TAG_GET_BOARD_REVISION
      , TAG_GET_BOARD_MAC_ADDRESS
      , TAG_GET_BOARD_SERIAL
      //, TAG_GET_ARM_MEMORY
      //, TAG_GET_VC_MEMORY
      //, TAG_GET_DMA_CHANNELS
      //, TAG_GET_CLOCKS
      //, TAG_GET_COMMAND_LINE
   };

   const char *tagnames[] = {
      "FIRMWARE_VERSION"
      , "BOARD_MODEL"
      , "BOARD_REVISION"
      , "BOARD_MAC_ADDRESS"
      , "BOARD_SERIAL"
      //, "ARM_MEMORY"
      //, "VC_MEMORY"
      //, "DMA_CHANNEL"
      //, "CLOCKS"
      //, "COMMAND_LINE"
   };

   const char *clock_names[] = {
      "RESERVED",
      "EMMC",
      "UART",
      "ARM",
      "CORE",
      "V3D",
      "H264",
      "ISP",
      "SDRAM",
      "PIXEL",
      "PWM"
   };

   unsigned int n = sizeof(tags) / sizeof(rpi_mailbox_tag_t);
   LOG_INFO("\r\n"); // put some new lines in the serial stream as we don't know what is currently on the terminal
   LOG_INFO("\r\n");
   LOG_INFO("**********     Raspberry Pi BBC Micro Coprocessor     **********\r\n");
   LOG_INFO("\r\n");
   LOG_INFO("\r\n");

   RPI_PropertyInit();
   for (i = 0; i < n ; i++) {
      RPI_PropertyAddTag(tags[i]);
   }

   RPI_PropertyProcess();

   for (i = 0; i < n; i++) {
      rpi_mailbox_property_t *buf = RPI_PropertyGet(tags[i]);
      print_tag_value(tagnames[i], buf, 1);
   }

   for (i = MIN_CLK_ID; i <= MAX_CLK_ID; i++) {
         clock_info_t *clk_info = get_clock_rates(i);
      LOG_INFO("%15s_FREQ : %10.3f MHz %10.3f MHz %10.3f MHz\r\n",
             clock_names[i],
             (double) (clk_info->rate)  / 1.0e6,
             (double) (clk_info->min_rate)  / 1.0e6,
             (double) (clk_info->max_rate)  / 1.0e6
         );
   }

   LOG_INFO("           CORE TEMP : %6.2f °C\r\n", (double)get_temp());
   LOG_INFO("        CORE VOLTAGE : %6.2f V\r\n", (double)get_voltage(COMPONENT_CORE));
   LOG_INFO("     SDRAM_C VOLTAGE : %6.2f V\r\n", (double)get_voltage(COMPONENT_SDRAM_C));
   LOG_INFO("     SDRAM_P VOLTAGE : %6.2f V\r\n", (double)get_voltage(COMPONENT_SDRAM_P));
   LOG_INFO("     SDRAM_I VOLTAGE : %6.2f V\r\n", (double)get_voltage(COMPONENT_SDRAM_I));

   LOG_INFO("            CMD_LINE : %s\r\n", get_cmdline());
   const char *cs ;
   cs = get_cmdline_prop("copro");
   if (!cs)
      cs = "0 (default)";
   LOG_INFO("               COPRO : %s\r\n", cs);
}
