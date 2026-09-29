// USB MIDI host — see usb_midi_host.h.
//
// Two FreeRTOS tasks, like ESP-IDF's usb_host_lib example: one pumps the
// host library's daemon events, the other is our single client, which opens
// every device that has a USB-MIDI streaming interface and keeps one bulk IN
// transfer in flight per device. Transfer callbacks run inside
// usb_host_client_handle_events(), i.e. in the client task, so all device
// bookkeeping below is touched by that one task only; the main loop only sees
// the packet queue and two read-only status values.

#include "usb_midi_host.h"

#include <Arduino.h>
#include <string.h>
#include "usb/usb_host.h"

static const int MAX_DEVS = 4;           // plenty for a keyboard or two on a hub
static const int QUEUE_PACKETS = 256;    // ~a second of dense playing, if loop() stalls
static const uint8_t USB_SUBCLASS_MIDISTREAMING = 0x03;

struct MidiDev {
  bool used;
  bool closing;                          // unplugged: waiting for the transfer to come back
  bool inFlight;                         // our IN transfer is submitted
  uint8_t addr;
  uint8_t intfNum;
  usb_device_handle_t dev;
  usb_transfer_t *xfer;
  char name[33];
};

static MidiDev devs[MAX_DEVS];
static usb_host_client_handle_t client;
static QueueHandle_t packetQueue;
static volatile int connectedCount = 0;
static char firstName[33] = "";

// Events arrive in the client callback, but opening/closing devices is done
// afterwards in the task loop (the host library doesn't allow it re-entrantly).
static uint8_t pendingNew[MAX_DEVS];
static int pendingNewCount = 0;
static usb_device_handle_t pendingGone[MAX_DEVS];
static int pendingGoneCount = 0;

static void updateStatus() {
  int n = 0;
  firstName[0] = 0;
  for (auto &d : devs) {
    if (!d.used || d.closing) continue;
    if (n == 0) strlcpy(firstName, d.name, sizeof(firstName));
    n++;
  }
  connectedCount = n;
}

static void transferDone(usb_transfer_t *t) {
  MidiDev *d = (MidiDev *)t->context;
  if (t->status == USB_TRANSFER_STATUS_COMPLETED) {
    for (int i = 0; i + 4 <= t->actual_num_bytes; i += 4) {
      const uint8_t *p = t->data_buffer + i;
      if ((p[0] & 0x0F) == 0) continue;  // CIN 0 = reserved/padding
      xQueueSend(packetQueue, p, 0);     // full queue: drop rather than block USB
    }
  }
  // Keep reading until the device goes away. Anything other than a clean
  // completion (unplug, cancel, stall, bus error) ends this device's reads;
  // replugging it starts fresh.
  if (!d->closing && t->status == USB_TRANSFER_STATUS_COMPLETED &&
      usb_host_transfer_submit(t) == ESP_OK) {
    return;
  }
  d->inFlight = false;
}

static void clientEvent(const usb_host_client_event_msg_t *msg, void *) {
  if (msg->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
    if (pendingNewCount < MAX_DEVS) pendingNew[pendingNewCount++] = msg->new_dev.address;
  } else if (msg->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
    if (pendingGoneCount < MAX_DEVS) pendingGone[pendingGoneCount++] = msg->dev_gone.dev_hdl;
  }
}

static void copyName(const usb_str_desc_t *s, char *out, size_t outLen) {
  out[0] = 0;
  if (!s) return;
  size_t n = 0;
  int chars = (s->bLength - 2) / 2;
  for (int i = 0; i < chars && n + 1 < outLen; i++) {
    uint16_t c = s->wData[i];
    out[n++] = (c >= 32 && c < 127) ? (char)c : '?';
  }
  out[n] = 0;
}

static void openDevice(uint8_t addr) {
  MidiDev *slot = nullptr;
  for (auto &d : devs) {
    if (!d.used) { slot = &d; break; }
  }
  if (!slot) return;

  usb_device_handle_t dev;
  if (usb_host_device_open(client, addr, &dev) != ESP_OK) return;

  const usb_config_desc_t *cfg;
  if (usb_host_get_active_config_descriptor(dev, &cfg) != ESP_OK) {
    usb_host_device_close(client, dev);
    return;
  }

  // First MIDI-streaming interface (audio class, subclass 3) with a bulk IN endpoint.
  int off = 0;
  const usb_standard_desc_t *desc = (const usb_standard_desc_t *)cfg;
  const usb_intf_desc_t *intf = nullptr;
  const usb_ep_desc_t *epIn = nullptr;
  while (!epIn && (desc = usb_parse_next_descriptor_of_type(desc, cfg->wTotalLength, USB_B_DESCRIPTOR_TYPE_INTERFACE, &off))) {
    const usb_intf_desc_t *candidate = (const usb_intf_desc_t *)desc;
    if (candidate->bInterfaceClass != USB_CLASS_AUDIO || candidate->bInterfaceSubClass != USB_SUBCLASS_MIDISTREAMING) continue;
    for (int e = 0; e < candidate->bNumEndpoints && !epIn; e++) {
      int epOff = off;
      const usb_ep_desc_t *ep = usb_parse_endpoint_descriptor_by_index(candidate, e, cfg->wTotalLength, &epOff);
      if (ep && USB_EP_DESC_GET_EP_DIR(ep) && USB_EP_DESC_GET_XFERTYPE(ep) == USB_TRANSFER_TYPE_BULK) {
        intf = candidate;
        epIn = ep;
      }
    }
  }
  if (!epIn) {  // not a MIDI device (or a hub/mouse/etc.) — leave it alone
    usb_host_device_close(client, dev);
    return;
  }

  if (usb_host_interface_claim(client, dev, intf->bInterfaceNumber, intf->bAlternateSetting) != ESP_OK) {
    usb_host_device_close(client, dev);
    return;
  }

  int mps = USB_EP_DESC_GET_MPS(epIn);
  usb_transfer_t *xfer;
  if (usb_host_transfer_alloc(mps, 0, &xfer) != ESP_OK) {
    usb_host_interface_release(client, dev, intf->bInterfaceNumber);
    usb_host_device_close(client, dev);
    return;
  }

  memset(slot, 0, sizeof(*slot));
  slot->used = true;
  slot->addr = addr;
  slot->intfNum = intf->bInterfaceNumber;
  slot->dev = dev;
  slot->xfer = xfer;
  usb_device_info_t info;
  if (usb_host_device_info(dev, &info) == ESP_OK) copyName(info.str_desc_product, slot->name, sizeof(slot->name));

  xfer->device_handle = dev;
  xfer->bEndpointAddress = epIn->bEndpointAddress;
  xfer->callback = transferDone;
  xfer->context = slot;
  xfer->num_bytes = mps;
  slot->inFlight = usb_host_transfer_submit(xfer) == ESP_OK;
  updateStatus();
}

static void startClosing(usb_device_handle_t dev) {
  for (auto &d : devs) {
    if (!d.used || d.dev != dev || d.closing) continue;
    d.closing = true;
    // Cancel the pending IN transfer; its callback then clears inFlight.
    usb_host_endpoint_halt(dev, d.xfer->bEndpointAddress);
    usb_host_endpoint_flush(dev, d.xfer->bEndpointAddress);
  }
  updateStatus();
}

// Finishes closing any device whose transfer has come back.
static void reapClosed() {
  for (auto &d : devs) {
    if (!d.used || !d.closing || d.inFlight) continue;
    usb_host_endpoint_clear(d.dev, d.xfer->bEndpointAddress);
    usb_host_transfer_free(d.xfer);
    usb_host_interface_release(client, d.dev, d.intfNum);
    usb_host_device_close(client, d.dev);
    d.used = false;
  }
}

static void hostDaemonTask(void *) {
  for (;;) {
    uint32_t flags;
    usb_host_lib_handle_events(portMAX_DELAY, &flags);
    if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) usb_host_device_free_all();
  }
}

static void clientTask(void *) {
  for (;;) {
    // Short timeout so a closing device gets reaped even if no other event comes.
    usb_host_client_handle_events(client, pdMS_TO_TICKS(50));
    for (int i = 0; i < pendingGoneCount; i++) startClosing(pendingGone[i]);
    pendingGoneCount = 0;
    for (int i = 0; i < pendingNewCount; i++) openDevice(pendingNew[i]);
    pendingNewCount = 0;
    reapClosed();
  }
}

bool usbMidiHostBegin() {
  packetQueue = xQueueCreate(QUEUE_PACKETS, 4);
  if (!packetQueue) return false;

  usb_host_config_t hostCfg = {};
  hostCfg.intr_flags = ESP_INTR_FLAG_LEVEL1;
  if (usb_host_install(&hostCfg) != ESP_OK) return false;

  // Core 1 (with loop()); core 0 belongs to the pitch detector and WiFi.
  if (xTaskCreatePinnedToCore(hostDaemonTask, "usbhost", 4096, nullptr, 2, nullptr, 1) != pdPASS) return false;

  usb_host_client_config_t clientCfg = {};
  clientCfg.is_synchronous = false;
  clientCfg.max_num_event_msg = 5;
  clientCfg.async.client_event_callback = clientEvent;
  clientCfg.async.callback_arg = nullptr;
  if (usb_host_client_register(&clientCfg, &client) != ESP_OK) return false;

  return xTaskCreatePinnedToCore(clientTask, "usbmidi", 4096, nullptr, 2, nullptr, 1) == pdPASS;
}

bool usbMidiHostRead(uint8_t packet[4]) {
  return packetQueue && xQueueReceive(packetQueue, packet, 0) == pdTRUE;
}

int usbMidiDeviceCount() {
  return connectedCount;
}

const char *usbMidiDeviceName() {
  return firstName;
}
