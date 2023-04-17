/**
 * Compile:
 * gcc joystick.c -o joystick
 *
 * Run:
 * ./joystick [/dev/input/jsX]
 *
 * See also:
 * https://www.kernel.org/doc/Documentation/input/joystick-api.txt
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <limits.h>
#include <linux/joystick.h>
#include <string.h>

#define MOVE_X 3
#define MOVE_Y 4
#define SCROLL 1
#define RIGHT_CLICK 5

/**
 * Reads a joystick event from the joystick device.
 *
 * Returns 0 on success. Otherwise -1 is returned.
 */
int read_event(int fd, struct js_event *event) {
  ssize_t bytes;

  bytes = read(fd, event, sizeof(*event));

  if (bytes == sizeof(*event))
    return 0;

  /* Error, could not read full event. */
  return -1;
}

/**
 * Returns the number of axes on the controller or 0 if an error occurs.
 */
size_t get_axis_count(int fd) {
  __u8 axes;

  if (ioctl(fd, JSIOCGAXES, &axes) == -1)
    return 0;

  return axes;
}

/**
 * Returns the number of buttons on the controller or 0 if an error occurs.
 */
size_t get_button_count(int fd) {
  __u8 buttons;
  if (ioctl(fd, JSIOCGBUTTONS, &buttons) == -1)
    return 0;

  return buttons;
}

/**
 * Keeps track of the current axis state.
 *
 * NOTE: This function assumes that axes are numbered starting from 0, and that
 * the X axis is an even number, and the Y axis is an odd number. However, this
 * is usually a safe assumption.
 *
 * Returns the axis that the event indicated.
 */
size_t get_axis_state(struct js_event *event, short axes[8]) {
  size_t axis = event->number;

  if (axis < 8) {
    axes[axis] = event->value;
  }

  return axis;
}

// Mouse click function
void click(Display *display, int button) {
  printf("function invoked");
  // Create and setting up the event
  XEvent event;
  memset(&event, 0, sizeof(event));
  event.xbutton.button = button;
  event.xbutton.same_screen = True;
  event.xbutton.subwindow = DefaultRootWindow(display);
  while (event.xbutton.subwindow) {
    event.xbutton.window = event.xbutton.subwindow;
    XQueryPointer(display, event.xbutton.window, &event.xbutton.root,
                  &event.xbutton.subwindow, &event.xbutton.x_root,
                  &event.xbutton.y_root, &event.xbutton.x, &event.xbutton.y,
                  &event.xbutton.state);
  }
  // Press
  event.type = ButtonPress;
  if (XSendEvent(display, PointerWindow, True, ButtonPressMask, &event) == 0)
    fprintf(stderr, "Error to send the event!\n");
  XFlush(display);
  usleep(1);
  event.type = ButtonRelease;
  if (XSendEvent(display, PointerWindow, True, ButtonReleaseMask, &event) == 0)
    fprintf(stderr, "Error to send the event!\n");
  XFlush(display);
  usleep(1);
}

int main(int argc, char *argv[]) {
  Display *display;
  Window root;

  if ((display = XOpenDisplay(NULL)) == NULL) {
    fprintf(stderr, "Cannot open local X-display.\n");
    exit(1);
  }

  root = DefaultRootWindow(display);

  int x, y, tmp, x2, y2;
  Window fromroot, tmpwin;

  const char *device;
  int js;
  struct js_event event;
  short axes[8] = {0};
  size_t axis;

  char *command, xstr, ystr;

  if (argc > 1)
    device = argv[1];
  else
    device = "/dev/input/js0";

  js = open(device, O_RDONLY);

  if (js == -1)
    perror("Could not open Joystick");

  /* This loop will exit if the controller is unplugged. */
  while (read_event(js, &event) == 0) {
    switch (event.type) {
    case JS_EVENT_BUTTON:
      printf("Button %u %s\n", event.number,
             event.value ? "pressed" : "released");
      switch (event.number) {
      case 5:
        // Left click RB
        if (event.value == 0) {
          // command ="xdotool click 1";
          // system(command);
          click(display, Button1);
        }
        break;
      case 4:
        // Right click LB
        if (event.value == 0) {
          // command ="xdotool click 3";
          // system(command);
          click(display, Button3);
        }
        break;
      }
      break;
    case JS_EVENT_AXIS:
      XQueryPointer(display, root, &fromroot, &tmpwin, &x, &y, &tmp, &tmp,
                    &tmp);
      axis = get_axis_state(&event, axes);
      if (axis < 8) {
        switch (axis) {
        case MOVE_X:
          // Mouse x
          printf("%d\n", axes[axis]);
          x += axes[MOVE_X] / 3200;
          printf("%d\n", x);
          XWarpPointer(display, None, root, 0, 0, 0, 0, x, y);
          XFlush(display);
          break;
        case MOVE_Y:
          // Mouse y
          printf("%d\n", axes[axis]);
          y += axes[MOVE_Y] / 3200;
          printf("%d\n", y);
          XWarpPointer(display, None, root, 0, 0, 0, 0, x, y);
          XFlush(display);
          break;
        case SCROLL:
          // Vertical Scroll Right stick
          if (axes[SCROLL] < 0)
            // command = "xdotool click 4";
            click(display, Button4);
          else
            // command = "xdotool click 5";
            click(display, Button5);
          system(command);
          break;
        }
      }
      XFlush(display);
      printf("Axis %zu at %6d\n", axis, axes[axis]);
      break;
    default:
      /* Ignore init events. */
      break;
    }

    fflush(stdout);
  }

  close(js);
  return 0;
}
