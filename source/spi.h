#define CS_PIN    13
#define MOSI_PIN  44
#define MISO_PIN  18
#define CLK_PIN   43

#define SPI_DELAY 0

#define LOW VM_DCL_GPIO_COMMAND_WRITE_LOW
#define HIGH VM_DCL_GPIO_COMMAND_WRITE_HIGH

VMINT32 open_input_gpio_pin(VM_DCL_HANDLE *gpio_handle, VMINT32 gpio_pin_id);
VMINT32 open_output_gpio_pin(VM_DCL_HANDLE *gpio_handle, VMINT32 gpio_pin_id);
VMUCHAR SPI_transfer_byte(VMUCHAR byte);
void spi_open(void);
void spi_close(void);
