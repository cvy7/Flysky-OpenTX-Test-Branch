/*
 * Copyright (C) OpenTX
 *
 * Based on code named
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _JOYSTICK_TARGET_H_
#define _JOYSTICK_TARGET_H_

#include "window.h"
class JoystickTarget : public Window {
  friend class JoystickKeyboard;
  public:
  JoystickTarget(Window * parent, const rect_t & rect, uint8_t flags=0) : Window(parent, rect, flags) {}
  protected:
    virtual void up() = 0;
    virtual void down() = 0;
    virtual void right() = 0;
    virtual void left() = 0;
    virtual void enter() = 0;
    virtual void escape() = 0;
};

#endif // _JOYSTICK_TARGET_H_
