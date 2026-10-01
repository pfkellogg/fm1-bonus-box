// USB MIDI host bridge — see usb_midi_bridge.h.
//
// Same structure as fm1_control_box's usb_midi_host.cpp (ESP-IDF USB host
// library, a daemon task + one client task), plus OUT transfers so it can
// send to a USB MIDI device too. Transfer callbacks run inside
// usb_host_client_handle_events(), i.e. in the client task, so all the
// bookkeeping below — device table, per-device send queues — is only ever
// touched by that one task. No locks needed.

#include "usb_midi_bridge.h"

#include <Arduino.h>
#include <string.h>
#include "usb/usb_host.h"

static const int MAX_DEVS = 4;
static const int RING = 128;  // queued 4-byte packets per destination
static const uint8_t USB_SUBCLASS_MIDISTREAMING = 0x03;
// Everything is sent on MIDI channel 1 (0 on the wire), the FM-1's default
// Note Channel — so the keyboard's own channel setting doesn't matter.
static const uint8_t OUT_CHANNEL = 0;

struct Dev {
  bool used;
  bool closing;       // unplugged: waiting for its transfers to come back
  bool inBusy;        // IN transfer submitted
  bool outBusy;       // OUT transfer submitted
  uint8_t intfNum;
  usb_device_handle_t dev;
  usb_transfer_t *in;   // null if the device has no MIDI IN endpoint
  usb_transfer_t *out;  // null if the device has no MIDI OUT endpoint
  int outMps;
  uint8_t ring[RING][4];
  int head, tail;
};

static Dev devs[MAX_DEVS];
static usb_host_client_handle_t client;
static volatile int connectedCount = 0;
static volatile uint32_t noteCount = 0;

static uint8_t pendingNew[MAX_DEVS];
static int pendingNewCount = 0;
static usb_device_handle_t pendingGone[MAX_DEVS];
static int pendingGoneCount = 0;

static void updateCount() {
  int n = 0;
  for (auto &d : devs) {
    if (d.used && !d.closing) n++;
  }
  connectedCount = n;
}

// Sends as many queued packets as fit in one OUT transfer, if it's idle.
static void kick(Dev &d) {
  if (!d.out || d.outBusy || d.closing || d.head == d.tail) return;
  int n = 0;
  int maxPackets = d.outMps / 4;
  while (d.tail != d.head && n < maxPackets) {
    memcpy(d.out->data_buffer + n * 4, d.ring[d.tail], 4);
    d.tail = (d.tail + 1) % RING;
    n++;
  }
  d.out->num_bytes = n * 4;
  d.outBusy = usb_host_transfer_submit(d.out) == ESP_OK;
}

static void queuePacket(Dev &d, const uint8_t p[4]) {
  int next = (d.head + 1) % RING;
  if (next == d.tail) return;  // full: drop rather than stall
  memcpy(d.ring[d.head], p, 4);
  d.head = next;
}

static void outDone(usb_transfer_t *t) {
  Dev &d = *(Dev *)t->context;
  d.outBusy = false;
  kick(d);
}

static void inDone(usb_transfer_t *t) {
  Dev &src = *(Dev *)t->context;
  if (t->status == USB_TRANSFER_STATUS_COMPLETED) {
    for (int i = 0; i + 4 <= t->actual_num_bytes; i += 4) {
      uint8_t p[4];
      memcpy(p, t->data_buffer + i, 4);
      if (p[0] >> 4) continue;  // cable 0 only (a Keystation's 2nd port is its transport buttons)
      uint8_t cin = p[0] & 0x0F;
      if (cin < 0x08 || cin > 0x0E) continue;  // channel messages only: no SysEx/clock/active sensing
      p[1] = (p[1] & 0xF0) | OUT_CHANNEL;
      if (cin == 0x09 && p[3] > 0) noteCount = noteCount + 1;  // only this task writes it
      for (auto &dst : devs) {
        if (&dst == &src || !dst.used || dst.closing || !dst.out) continue;
        queuePacket(dst, p);
        kick(dst);
      }
    }
  }
  if (!src.closing && t->status == USB_TRANSFER_STATUS_COMPLETED &&
      usb_host_transfer_submit(t) == ESP_OK) {
    return;
  }
  src.inBusy = false;
}

static void clientEvent(const usb_host_client_event_msg_t *msg, void *) {
  if (msg->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
    if (pendingNewCount < MAX_DEVS) pendingNew[pendingNewCount++] = msg->new_dev.address;
  } else if (msg->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
    if (pendingGoneCount < MAX_DEVS) pendingGone[pendingGoneCount++] = msg->dev_gone.dev_hdl;
  }
}

static usb_transfer_t *makeTransfer(Dev *d, usb_device_handle_t dev, const usb_ep_desc_t *ep, usb_transfer_cb_t cb) {
  usb_transfer_t *t;
  if (usb_host_transfer_alloc(USB_EP_DESC_GET_MPS(ep), 0, &t) != ESP_OK) return nullptr;
  t->device_handle = dev;
  t->bEndpointAddress = ep->bEndpointAddress;
  t->callback = cb;
  t->context = d;
  t->num_bytes = USB_EP_DESC_GET_MPS(ep);
  return t;
}

static void openDevice(uint8_t addr) {
  Dev *slot = nullptr;
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

  // First MIDI-streaming interface (audio class, subclass 3) and its bulk endpoints.
  int off = 0;
  const usb_standard_desc_t *desc = (const usb_standard_desc_t *)cfg;
  const usb_intf_desc_t *intf = nullptr;
  const usb_ep_desc_t *epIn = nullptr, *epOut = nullptr;
  while (!intf && (desc = usb_parse_next_descriptor_of_type(desc, cfg->wTotalLength, USB_B_DESCRIPTOR_TYPE_INTERFACE, &off))) {
    const usb_intf_desc_t *candidate = (const usb_intf_desc_t *)desc;
    if (candidate->bInterfaceClass != USB_CLASS_AUDIO || candidate->bInterfaceSubClass != USB_SUBCLASS_MIDISTREAMING) continue;
    for (int e = 0; e < candidate->bNumEndpoints; e++) {
      int epOff = off;
      const usb_ep_desc_t *ep = usb_parse_endpoint_descriptor_by_index(candidate, e, cfg->wTotalLength, &epOff);
      if (!ep || USB_EP_DESC_GET_XFERTYPE(ep) != USB_TRANSFER_TYPE_BULK) continue;
      if (USB_EP_DESC_GET_EP_DIR(ep)) { if (!epIn) epIn = ep; }
      else if (!epOut) epOut = ep;
    }
    if (epIn || epOut) intf = candidate;
  }
  if (!intf || usb_host_interface_claim(client, dev, intf->bInterfaceNumber, intf->bAlternateSetting) != ESP_OK) {
    usb_host_device_close(client, dev);  // not a MIDI device (or a hub/mouse/etc.)
    return;
  }

  memset(slot, 0, sizeof(*slot));
  slot->used = true;
  slot->dev = dev;
  slot->intfNum = intf->bInterfaceNumber;
  if (epOut) {
    slot->out = makeTransfer(slot, dev, epOut, outDone);
    slot->outMps = USB_EP_DESC_GET_MPS(epOut);
  }
  if (epIn) {
    slot->in = makeTransfer(slot, dev, epIn, inDone);
    if (slot->in) slot->inBusy = usb_host_transfer_submit(slot->in) == ESP_OK;
  }
  updateCount();
}

static void startClosing(usb_device_handle_t dev) {
  for (auto &d : devs) {
    if (!d.used || d.dev != dev || d.closing) continue;
    d.closing = true;
    // Cancel pending transfers; their callbacks then clear the busy flags.
    for (usb_transfer_t *t : {d.in, d.out}) {
      if (!t) continue;
      usb_host_endpoint_halt(dev, t->bEndpointAddress);
      usb_host_endpoint_flush(dev, t->bEndpointAddress);
    }
  }
  updateCount();
}

static void reapClosed() {
  for (auto &d : devs) {
    if (!d.used || !d.closing || d.inBusy || d.outBusy) continue;
    for (usb_transfer_t *t : {d.in, d.out}) {
      if (!t) continue;
      usb_host_endpoint_clear(d.dev, t->bEndpointAddress);
      usb_host_transfer_free(t);
    }
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
    // Short timeout so an unplugged device gets reaped even if nothing else happens.
    usb_host_client_handle_events(client, pdMS_TO_TICKS(50));
    for (int i = 0; i < pendingGoneCount; i++) startClosing(pendingGone[i]);
    pendingGoneCount = 0;
    for (int i = 0; i < pendingNewCount; i++) openDevice(pendingNew[i]);
    pendingNewCount = 0;
    reapClosed();
  }
}

bool bridgeBegin() {
  usb_host_config_t hostCfg = {};
  hostCfg.intr_flags = ESP_INTR_FLAG_LEVEL1;
  if (usb_host_install(&hostCfg) != ESP_OK) return false;
  if (xTaskCreatePinnedToCore(hostDaemonTask, "usbhost", 4096, nullptr, 2, nullptr, 1) != pdPASS) return false;

  usb_host_client_config_t clientCfg = {};
  clientCfg.is_synchronous = false;
  clientCfg.max_num_event_msg = 5;
  clientCfg.async.client_event_callback = clientEvent;
  clientCfg.async.callback_arg = nullptr;
  if (usb_host_client_register(&clientCfg, &client) != ESP_OK) return false;

  return xTaskCreatePinnedToCore(clientTask, "usbmidi", 4096, nullptr, 3, nullptr, 1) == pdPASS;
}

int bridgeDeviceCount() {
  return connectedCount;
}

uint32_t bridgeNoteCount() {
  return noteCount;
}
