//
// Created by Aaron Gill-Braun on 2021-04-22.
//

#include <usb/scsi.h>
#include <usb/usb.h>
#include <printf.h>
#include <mm.h>
#include <panic.h>
#include <string.h>

#define SCSI_MAX_XFER 64

char sd_suffix = 'a';

void setup_command_block(usb_ms_cbw_t *cbw, void *cb, size_t size, size_t trnsf_len, bool dir) {
  memset(cbw, 0, sizeof(usb_ms_cbw_t));
  cbw->signature = CBW_SIGNATURE;
  cbw->tag = 1;
  cbw->flags = (dir << 7);
  cbw->trnsf_len = trnsf_len;
  cbw->lun = 0;
  cbw->cb_length = size;
  memcpy(cbw->cb, cb, size);
}

//

int scsi_device_init(usb_device_t *device) {
  kprintf("scsi: device init\n");
  scsi_device_t *scsi_dev = kmalloc(sizeof(scsi_device_t));

  scsi_inquiry_cmd_t inquiry_cmd = {
    .op_code = SCSI_OP_INQUIRY,
    .evpd = 0,
    .page_code = 0,
    .alloc_length = sizeof(scsi_device_info_t),
    .control = 0,
  };

  usb_ms_cbw_t *cbw = kmalloc(sizeof(usb_ms_cbw_t));
  usb_ms_csw_t *csw = kmalloc(sizeof(usb_ms_csw_t));
  setup_command_block(cbw, &inquiry_cmd, sizeof(scsi_inquiry_cmd_t), sizeof(scsi_device_info_t), USB_IN);

  scsi_device_info_t *info = kmalloc(sizeof(scsi_device_info_t));
  memset(info, 0, sizeof(scsi_device_info_t));
  scsi_dev->info = info;

  usb_add_transfer(device, USB_OUT, kheap_ptr_to_phys(cbw), sizeof(usb_ms_cbw_t));
  if (usb_start_await_transfer(device, USB_OUT) < 0) {
    kprintf("scsi: failed to send command\n");
    kfree(cbw);
    kfree(csw);
    return -1;
  }

  usb_add_transfer(device, USB_IN, virt_to_phys_addr(info), sizeof(scsi_device_info_t));
  if (usb_start_await_transfer(device, USB_IN) < 0) {
    kprintf("scsi: failed to read device info\n");
    kfree(cbw);
    kfree(csw);
    return -1;
  }

  usb_add_transfer(device, USB_IN, kheap_ptr_to_phys(csw), sizeof(usb_ms_csw_t));
  if (usb_start_await_transfer(device, USB_IN) < 0) {
    kprintf("scsi: failed to read command status\n");
    kfree(cbw);
    kfree(csw);
    return -1;
  }

  kfree(cbw);
  kfree(csw);

  // blkdev_t *blkdev = blkdev_init(device, scsi_read, scsi_write);
  // dev_t d = fs_register_blkdev(0, blkdev, NULL);
  // kassert(d > 0);
  //
  // char suffix = sd_suffix;
  // sd_suffix++;
  //
  // char path[12];
  // ksnprintf(path, 12, "/dev/sd%c", suffix);
  //
  // if (fs_mknod(path, S_IFBLK, d) < 0) {
  //   panic("failed to add device node");
  // }

  device->driver_data = scsi_dev;
  kprintf("scsi: device init finished!\n");
  return 0;
}

int scsi_device_deinit(usb_device_t *device) {
  kfree(device->driver_data);
  return 0;
}

// internal read/write

ssize_t scsi_read_internal(usb_device_t *device, uint64_t lba, uint32_t count, void *buf) {
  kassert(count)
  kassert(count > 0 && count <= SCSI_MAX_XFER);
  // kprintf("scsi: read [lba = %llu, count = %u]\n", lba, count);

  uint64_t size = count * 512;
  scsi_read16_cmd_t read_cmd = {
    .op_code = SCSI_OP_READ_16,
    .dld2 = 0,
    .rarc = 0,
    .fua = 0,
    .dpo = 0,
    .rdprotect = 0,
    .lba = big_endian(lba),
    .xfer_length = big_endian(count),
    .group_num = 0,
    .dld0 = 0,
    .dld1 = 0,
    .control = 0
  };

  usb_ms_cbw_t *cbw = kmalloc(sizeof(usb_ms_cbw_t));
  usb_ms_csw_t *csw = kmalloc(sizeof(usb_ms_csw_t));
  setup_command_block(cbw, &read_cmd, sizeof(scsi_read16_cmd_t), size, USB_IN);

  usb_add_transfer(device, USB_OUT, kheap_ptr_to_phys(cbw), sizeof(usb_ms_cbw_t));
  if (usb_start_await_transfer(device, USB_OUT) < 0) {
    goto FAIL;
  }

  usb_add_transfer(device, USB_IN, virt_to_phys_addr(buf), size);
  if (usb_start_await_transfer(device, USB_IN) < 0) {
    goto FAIL;
  }

  usb_add_transfer(device, USB_IN, kheap_ptr_to_phys(csw), sizeof(usb_ms_csw_t));
  if (usb_start_await_transfer(device, USB_IN) < 0) {
    goto FAIL;
  }

  kfree(cbw);
  kfree(csw);
  // kprintf("scsi: read successful\n");
  return size;

LABEL(FAIL);
  kfree(cbw);
  kfree(csw);
  kprintf("scsi: read failed\n");
  return -EFAILED;
}

ssize_t scsi_write_internal(usb_device_t *device, uint64_t lba, uint32_t count, void *buf) {
  kassert(count)
  kassert(count > 0 && count <= SCSI_MAX_XFER);

  uint64_t size = count * 512;
  scsi_write16_cmd_t write_cmd = {
    .op_code = SCSI_OP_WRITE_16,
    .dld2 = 0,
    .fua = 0,
    .dpo = 0,
    .wrprotect = 0,
    .lba = big_endian(lba),
    .xfer_length = big_endian(count),
    .group_num = 0,
    .dld0 = 0,
    .dld1 = 0,
    .control = 0
  };

  usb_ms_cbw_t *cbw = kmalloc(sizeof(usb_ms_cbw_t));
  usb_ms_csw_t *csw = kmalloc(sizeof(usb_ms_csw_t));
  setup_command_block(cbw, &write_cmd, sizeof(scsi_read16_cmd_t), size, USB_OUT);

  usb_add_transfer(device, USB_OUT, kheap_ptr_to_phys(cbw), sizeof(usb_ms_cbw_t));
  if (usb_start_await_transfer(device, USB_OUT) < 0) {
    goto FAIL;
  }

  usb_add_transfer(device, USB_OUT, _vm_virt_to_phys((uintptr_t) buf), size);
  if (usb_start_await_transfer(device, USB_OUT) < 0) {
    goto FAIL;
  }

  usb_add_transfer(device, USB_IN, kheap_ptr_to_phys(csw), sizeof(usb_ms_csw_t));
  if (usb_start_await_transfer(device, USB_IN) < 0) {
    goto FAIL;
  }

  kfree(cbw);
  kfree(csw);
  // kprintf("[scsi] write successful\n");
  return size;

LABEL(FAIL);
  kfree(cbw);
  kfree(csw);
  // kprintf("[scsi] write failed\n");
  return -EFAILED;
}

// disk api

ssize_t scsi_read(usb_device_t *device, uint64_t lba, uint32_t count, void *buf) {
  if (count == 0 || buf == NULL) {
    return 0;
  }

  // kprintf("scsi: read (%u blocks)\n", count);
  size_t buf_offset = 0;
  size_t lba_offset = 0;
  while (count > 0) {
    size_t ccount = min(count, SCSI_MAX_XFER);
    // kprintf("scsi: read -> %u\n", count);
    ssize_t result = scsi_read_internal(device, lba + lba_offset, ccount, buf + buf_offset);
    if (result < 0) {
      return -1;
    }

    buf_offset += result;
    lba_offset += ccount;
    count -= ccount;
  }
  return buf_offset;
}


ssize_t scsi_write(usb_device_t *device, uint64_t lba, uint32_t count, void *buf) {
  if (count == 0 || buf == NULL) {
    return 0;
  }

  size_t buf_offset = 0;
  size_t lba_offset = 0;
  while (count > 0) {
    size_t ccount = min(count, SCSI_MAX_XFER);
    ssize_t result = scsi_write_internal(device, lba + lba_offset, ccount, buf + buf_offset);
    if (result < 0) {
      return -1;
    }

    buf_offset += result;
    lba_offset += ccount;
    count -= ccount;
  }
  return buf_offset;
}
