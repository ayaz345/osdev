//
// Created by Aaron Gill-Braun on 2020-09-30.
//

#include <drivers/serial.h>
#include <cpu/io.h>

#include <device.h>
#include <fs.h>
#include <mm.h>

#include <printf.h>


#define ASSERT(x) kassert(x)
#define DPRINTF(fmt, ...) kprintf("serial: %s: " fmt, __func__, ##__VA_ARGS__)

#define SERIAL_DATA 0
#define SERIAL_INTR_EN 1
#define SERIAL_FIFO_CTRL 2
#define SERIAL_LINE_CTRL 3
#define SERIAL_MODEM_CTRL 4
#define SERIAL_LINE_STATUS 5
#define SERIAL_MODEM_STATUS 6

static int init_test_port(int port) {
  outb(port + SERIAL_INTR_EN, 0x00);    // disable interrupts
  outb(port + SERIAL_LINE_CTRL, 0x80);  // set baud rate divisor
  outw(port + SERIAL_DATA, 0x01);       // 115200 baud
  outb(port + SERIAL_LINE_CTRL, 0x03);  // 8 bits, one stop bit, no parity
  outb(port + SERIAL_FIFO_CTRL, 0xC7);  // enable FIFO, clear, 14-byte threshold
  outb(port + SERIAL_MODEM_CTRL, 0x0B); // enable IRQs, RTS/DSR set
  outb(port + SERIAL_MODEM_CTRL, 0x1E); // set in loopback mode, test the serial chip
  outb(port + SERIAL_DATA, 0xAE);       // send the test character

  // check that the port sends back the test character
  if (inb(port + SERIAL_DATA) != 0xAE) {
    return -1;
  }

  outb(port + SERIAL_MODEM_CTRL, 0x0F); // reset the serial chip
  return 0;
}

static char serial_read_char(int port) {
  while (!(inb(port + SERIAL_LINE_STATUS) & 0x01)); // wait for rx buffer to be full
  return (char) inb(port);
}

static void serial_write_char(int port, char a) {
  while (!(inb(port + SERIAL_LINE_STATUS) & 0x20)); // wait for tx buffer to be empty
  outb(port, a);
}

// Device API

struct serial_device {
  int port;
};

static int serial_d_open(device_t *device) {
  struct serial_device *dev = device->data;
  return 0;
}

static int serial_d_close(device_t *device) {
  return 0;
}

static ssize_t serial_d_read(device_t *device, size_t off, kio_t *kio) {
  struct serial_device *dev = device->data;
  if (off != 0) {
    return -EINVAL;
  }

  while (kio_writeb(kio, serial_read_char(dev->port)) > 0) {
    // do nothing
  }
  return (ssize_t) kio_transfered(kio);
}

static ssize_t serial_d_write(device_t *device, size_t off, kio_t *kio) {
  struct serial_device *dev = device->data;
  if (off != 0) {
    return -EINVAL;
  }

  uint8_t byte;
  while (kio_readb(kio, &byte) > 0) {
    serial_write_char(dev->port, (char) byte);
  }
  return (ssize_t) kio_transfered(kio);
}

static struct device_ops serial_ops = {
  .d_open = serial_d_open,
  .d_close = serial_d_close,
  .d_read = serial_d_read,
  .d_write = serial_d_write,
};

static void serial_module_init() {
  static const int ports[] = { COM1, COM2, COM3, COM4 };
  for (int i = 0; i < ARRAY_SIZE(ports); i++) {
    if (init_test_port(ports[i]) < 0) {
      continue;
    }

    kprintf("serial: found serial device on port COM%d\n", i+1);

    struct serial_device *serial_dev = kmalloc(sizeof(struct serial_device));
    serial_dev->port = ports[i];

    device_t *dev = alloc_device(serial_dev, &serial_ops);
    if (register_dev("serial", dev) < 0) {
      DPRINTF("failed to register device");
      dev->data = NULL;
      free_device(dev);
      kfree(serial_dev);
    }
  }
}
MODULE_INIT(serial_module_init);
