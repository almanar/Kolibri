#include "vmtype.h"
#include "vmboard.h"
#include "vmsystem.h"
#include "vmlog.h"
#include "vmdcl.h"
#include "vmdcl_gpio.h"
#include "vmthread.h"

#include "spi.h"

// SPI handlers for ease declared global
VM_DCL_HANDLE gpio_handle_cs   = VM_DCL_HANDLE_INVALID;
VM_DCL_HANDLE gpio_handle_mosi = VM_DCL_HANDLE_INVALID;
VM_DCL_HANDLE gpio_handle_miso = VM_DCL_HANDLE_INVALID;
VM_DCL_HANDLE gpio_handle_clk  = VM_DCL_HANDLE_INVALID;
vm_dcl_gpio_control_level_status_t vm_dcl_data;

VMINT32 open_input_gpio_pin(VM_DCL_HANDLE *gpio_handle, VMINT32 gpio_pin_id){
    vm_log_info("Opening input pin");
    *gpio_handle = vm_dcl_open(VM_DCL_GPIO, gpio_pin_id);

    if (*gpio_handle != VM_DCL_HANDLE_INVALID)
    {
        /* Sets the pin mode to MODE_0 */
        vm_dcl_control(*gpio_handle, VM_DCL_GPIO_COMMAND_SET_MODE_0, NULL);

        /* Sets the pin direction to INPUT */
        vm_dcl_control(*gpio_handle, VM_DCL_GPIO_COMMAND_SET_DIRECTION_IN, NULL);

        /* Sets the pin to pull-low status */
        vm_dcl_control(*gpio_handle, VM_DCL_GPIO_COMMAND_ENABLE_PULL, NULL);
        vm_dcl_control(*gpio_handle, VM_DCL_GPIO_COMMAND_SET_PULL_LOW, NULL);
    }
    else
    {
        vm_log_info("Failed to open input pin.");
        return 0;
    }
    return 1;
}

VMINT32 open_output_gpio_pin(VM_DCL_HANDLE *gpio_handle, VMINT32 gpio_pin_id){
    vm_log_info("Opening output pin");
    /* Opens GPIO PIN WRITE_PIN */
    *gpio_handle = vm_dcl_open(VM_DCL_GPIO, gpio_pin_id);
    if (*gpio_handle != VM_DCL_HANDLE_INVALID)
    {
        /* Sets the pin mode to MODE_0 */
        vm_dcl_control(*gpio_handle, VM_DCL_GPIO_COMMAND_SET_MODE_0, NULL);

        /* Sets the pin direction to OUTPUT */
        vm_dcl_control(*gpio_handle, VM_DCL_GPIO_COMMAND_SET_DIRECTION_OUT, NULL);
    }
    else
    {
        vm_log_info("Failed to open output pin.");
        return 0;
    }
    return 1;
}

void    set_cs  (VMUCHAR value) {vm_dcl_control(gpio_handle_cs, value, NULL);};
void    set_clk (VMUCHAR value) {vm_dcl_control(gpio_handle_clk, value, NULL);}; //vm_thread_sleep(SPI_DELAY);}; // SPI delay
void    set_mosi(VMUCHAR value) {vm_dcl_control(gpio_handle_mosi, value, NULL);};
void    set_miso(VMUCHAR value) {vm_dcl_control(gpio_handle_miso, value, NULL);};
VMUCHAR get_miso(void){vm_dcl_control(gpio_handle_miso, VM_DCL_GPIO_COMMAND_READ,
		               (void*)&vm_dcl_data); return vm_dcl_data.level_status;};

VMUCHAR SPI_transfer_byte(VMUCHAR byte)
{
  VMUCHAR bit;
  VMUCHAR tdata=0;
  //while(get_miso()==HIGH){};

  for (bit = 0; bit < 8; bit++) {
      set_clk(LOW);

      /* write MOSI on rising edge of clock */
      if (byte & 0x80)
          set_mosi(HIGH);
      else
          set_mosi(LOW);
      byte <<= 1;

      /* read MISO on rising edge of clock */
      tdata = tdata << 1;
      if(get_miso() == 1)
        tdata = tdata | 0x01;
      else
        tdata = tdata & 0xfe;

      set_clk(HIGH);
  }

  set_clk(LOW);
  return tdata;
}

void spi_open(void){
    /* Opens SPI PINs */
    open_input_gpio_pin(&gpio_handle_miso, MISO_PIN);
    open_output_gpio_pin(&gpio_handle_cs, CS_PIN);
    open_output_gpio_pin(&gpio_handle_mosi, MOSI_PIN);
    open_output_gpio_pin(&gpio_handle_clk, CLK_PIN);
}

void spi_close(void){
	/* Closes SPI PINs */
    vm_dcl_close(gpio_handle_miso);
    vm_dcl_close(gpio_handle_cs);
    vm_dcl_close(gpio_handle_mosi);
    vm_dcl_close(gpio_handle_clk);
}
