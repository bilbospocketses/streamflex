/* The one uinput keyboard probe_spawn's Linux commands send through. */
#ifndef PROBE_UINPUT_LINUX_H
#define PROBE_UINPUT_LINUX_H

/* Creates the device with the key codes codes[0..n-1], or every code from 1 to 247 when codes is NULL, and waits 1 s
   for the desktop to pick it up. Returns 0, or prints uinput_failed with the step and errno and returns 1. */
int probe_uinput_open(const unsigned short *codes, unsigned n);

/* One key event and its report: 0, or -1 with errno set. */
int probe_uinput_key(unsigned short code, int down);

/* Waits 0.5 s for the desktop to read the last event, then destroys the device. */
void probe_uinput_close(void);

#endif
