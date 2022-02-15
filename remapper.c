// Include {{{
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
// }}}

// Flags {{{
static int do_terminate = 0;
// }}}

// Functions {{{
#define die(str, args...) do { perror(str); exit(EXIT_FAILURE); } while(0)

void send_event (int fd, int type, int code, int value) {
  struct input_event ev;
  memset(&ev, 0, sizeof(ev));

  gettimeofday(&ev.time, NULL);

  ev.type = type;
  ev.code = code;
  ev.value = value;

  if (write(fd, &ev, sizeof(ev)) < 0)
    die("error: write");
}

void ioctl_set (int fd, int set, int value) {
  if (ioctl(fd, set, value) < 0)
    die("error: ioctl set");
}

void create_uinput_device (int fd) {
  struct uinput_user_dev uidev;
  memset(&uidev, 0, sizeof(uidev));

  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "NiseFootPedal");
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor  = 0xDEAD;
  uidev.id.product = 0xBEEF;
  uidev.id.version = 1;

  if (write(fd, &uidev, sizeof(uidev)) < 0)
    die("error: create_uinput_device: write");

  if (ioctl(fd, UI_DEV_CREATE) < 0)
    die("error: create_uinput_device: ioctl");
}

void destroy_uinput_device (int fd) {
  if (ioctl(fd, UI_DEV_DESTROY) < 0)
    die("error: destroy_uinput_device: ioctl");
}

void on_term (int signal)
{
  printf("TERM\n");
  do_terminate = 1;
}

void set_signal_handler ()
{
  sigset_t mask;
  sigemptyset(&mask);
  signal(SIGTERM, on_term);
  signal(SIGINT, on_term);
  sigaddset(&mask, SIGTERM);
  sigaddset(&mask, SIGINT);
  sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

// }}}

// Remappers {{{

int remap_splitfish_to_vampire_survivors(int code) {
    switch (code) {
      case KEY_Y:
        return KEY_W;
      case KEY_E:
        return KEY_D;
      case KEY_Q:
        return KEY_A;
      case KEY_X:
        return KEY_S;
      case KEY_TAB:
      case KEY_LEFTCTRL:
        return KEY_ENTER;
    }
    return code;
}

int remap_8bitdo_zero2_to_vampire_survivors(int code) {
    switch (code) {
      case KEY_E:
        return KEY_W;
      case KEY_C:
        return KEY_D;
      case KEY_F:
        return KEY_S;
      case KEY_D:
        return KEY_A;
      case KEY_K:
        return KEY_ENTER;
      case KEY_M:
        return KEY_ESC;
    }
    return code;
}

// }}}

// Main {{{

int main (int argc, char** argv) {
  int outkb, inkb, ret;

  if (argc != 2)
    die("Usage: remapper <INPUT_DEVICE_EVENT>");

  inkb = open(argv[1], O_RDONLY);
  if (inkb < 0)
    die("error: open (in)");

  outkb = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (outkb < 0)
    die("error: open (out)");

  ioctl_set(outkb, UI_SET_EVBIT, EV_KEY);
  ioctl_set(outkb, UI_SET_KEYBIT, KEY_ESC);
  ioctl_set(outkb, UI_SET_KEYBIT, KEY_W);
  ioctl_set(outkb, UI_SET_KEYBIT, KEY_A);
  ioctl_set(outkb, UI_SET_KEYBIT, KEY_S);
  ioctl_set(outkb, UI_SET_KEYBIT, KEY_D);
  ioctl_set(outkb, UI_SET_KEYBIT, KEY_ENTER);

  create_uinput_device(outkb);

  struct input_event ev;
  int sz;

  ioctl(inkb, EVIOCGRAB, 1);

  set_signal_handler();

  fd_set fds, in_fds;
  FD_ZERO(&in_fds);
  FD_SET(inkb, &in_fds);

  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 100000;

  while (!do_terminate) {
    memcpy(&fds, &in_fds, sizeof(fd_set));
    int selected = select(inkb + 1, &fds, NULL, NULL, &tv);
    if (selected == 0)
      continue;
    if (selected < 0)
      die("error: select");
    sz = read(inkb, &ev, sizeof(struct input_event));
    if (sz < 0)
      continue;

    if (ev.type == EV_KEY) {
      // ev.code = remap_splitfish_to_vampire_survivors(ev.code);
       ev.code = remap_8bitdo_zero2_to_vampire_survivors(ev.code);
    }

    write(outkb, &ev, sizeof(ev));
  }

  ioctl(inkb, EVIOCGRAB, 0);

  destroy_uinput_device(outkb);

  close(outkb);
  close(inkb);

  exit(EXIT_SUCCESS);
}

// }}}
