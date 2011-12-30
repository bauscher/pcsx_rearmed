/*
 * (C) Gražvydas "notaz" Ignotas, 2011
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include "gpu.h"
#include "../../frontend/plugin_lib.h"
#include "../../frontend/cspace.h"

static const struct rearmed_cbs *cbs;
static void *screen_buf;

int vout_init(void)
{
  return 0;
}

int vout_finish(void)
{
  return 0;
}

static void check_mode_change(void)
{
  static uint32_t old_status;
  static int old_h;

  // width|rgb24 change?
  if ((gpu.status.reg ^ old_status) & ((7<<16)|(1<<21)) || gpu.screen.h != old_h)
  {
    old_status = gpu.status.reg;
    old_h = gpu.screen.h;
    screen_buf = cbs->pl_vout_set_mode(gpu.screen.hres,
                                       gpu.screen.h, gpu.status.rgb24 ? 24 : 16);
  }
}

static void blit(void)
{
  int x = gpu.screen.x & ~1; // alignment needed by blitter
  int y = gpu.screen.y;
  int w = gpu.screen.w;
  int h = gpu.screen.h;
  uint16_t *vram = gpu.vram;
  int stride = gpu.screen.hres;
  int fb_offs, doffs;
  uint8_t *dest;

  fb_offs = y * 1024 + x;
  dest = (uint8_t *)screen_buf;

  // only do centering, at least for now
  doffs = (stride - w) / 2 & ~1;

  if (gpu.status.rgb24)
  {
#ifndef MAEMO
    dest += (doffs / 8) * 24;
    for (; h-- > 0; dest += stride * 3, fb_offs += 1024)
    {
      fb_offs &= 1024*512-1;
      bgr888_to_rgb888(dest, vram + fb_offs, w * 3);
    }
#else
    dest += doffs * 2;
    for (; h-- > 0; dest += stride * 2, fb_offs += 1024)
    {
      fb_offs &= 1024*512-1;
      bgr888_to_rgb565(dest, vram + fb_offs, w * 3);
    }
#endif
  }
  else
  {
    dest += doffs * 2;
    for (; h-- > 0; dest += stride * 2, fb_offs += 1024)
    {
      fb_offs &= 1024*512-1;
      bgr555_to_rgb565(dest, vram + fb_offs, w * 2);
    }
  }

  screen_buf = cbs->pl_vout_flip();
}

void GPUupdateLace(void)
{
  if (gpu.status.blanking || !gpu.state.fb_dirty)
    return;

  if (gpu.frameskip.set) {
    if (!gpu.frameskip.frame_ready) {
      if (*gpu.state.frame_count - gpu.frameskip.last_flip_frame < 9)
        return;
    }
    gpu.frameskip.frame_ready = 0;
  }

  if (gpu.cmd_len > 0)
    flush_cmd_buffer();
  renderer_flush_queues();
  check_mode_change();
  if (cbs->pl_vout_raw_flip)
    cbs->pl_vout_raw_flip(gpu.screen.x, gpu.screen.y);
  else
    blit();
  gpu.state.fb_dirty = 0;
}

long GPUopen(void **unused)
{
  gpu.frameskip.active = 0;
  gpu.frameskip.frame_ready = 1;

  cbs->pl_vout_open();
  screen_buf = cbs->pl_vout_flip();
  return 0;
}

long GPUclose(void)
{
  cbs->pl_vout_close();
  return 0;
}

void GPUrearmedCallbacks(const struct rearmed_cbs *cbs_)
{
  cbs = cbs_;
  gpu.frameskip.set = cbs->frameskip;
  gpu.frameskip.advice = &cbs->fskip_advice;
  gpu.frameskip.active = 0;
  gpu.frameskip.frame_ready = 1;
  gpu.state.hcnt = cbs->gpu_hcnt;
  gpu.state.frame_count = cbs->gpu_frame_count;

  if (cbs->pl_vout_set_raw_vram)
    cbs->pl_vout_set_raw_vram(gpu.vram);
  renderer_set_config(cbs_);
}

// vim:shiftwidth=2:expandtab
