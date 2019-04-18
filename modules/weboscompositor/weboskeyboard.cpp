// Copyright (c) 2019-2020 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
#include "weboskeyboard.h"
#include <QWaylandCompositor>

WebOSKeyboard::WebOSKeyboard(QWaylandSeat *seat)
    : QWaylandKeyboard(seat)
{
    connect(&m_pendingFocusDestroyListener, &QWaylandDestroyListener::fired, this, &WebOSKeyboard::pendingFocusDestroyed);
}

void WebOSKeyboard::setFocus(QWaylandSurface *surface)
{
    if (m_pendingFocus != surface) {
        m_pendingFocus = surface;
        m_pendingFocusDestroyListener.reset();

        if (surface)
            m_pendingFocusDestroyListener.listenForDestruction(surface->resource());
    }

    if (m_grab) {
        m_grab->focused(surface);
    }
    else
        QWaylandKeyboard::setFocus(surface);
}

void WebOSKeyboard::sendKeyModifiers(QWaylandClient *client, uint32_t serial)
{
    Q_D(QWaylandKeyboard);

    if (m_grab) {
#if QT_CONFIG(xkbcommon)
        auto xkb_state = d->xkbState();
        modsDepressed = xkb_state_serialize_mods(xkb_state, (xkb_state_component)XKB_STATE_MODS_DEPRESSED);
        modsLatched   = xkb_state_serialize_mods(xkb_state, (xkb_state_component)XKB_STATE_MODS_LATCHED);
        modsLocked    = xkb_state_serialize_mods(xkb_state, (xkb_state_component)XKB_STATE_MODS_LOCKED);
        group         = xkb_state_serialize_group(xkb_state, (xkb_state_component)XKB_STATE_EFFECTIVE);
        xkb_state_update_mask(xkb_state, modsDepressed, modsLatched, modsLocked, 0, 0, group);
#endif
        m_grab->modifiers(serial, modsDepressed, modsLatched, modsLocked, group);
    }
    else
        QWaylandKeyboard::sendKeyModifiers(client, serial);
}

void WebOSKeyboard::sendKeyPressEvent(uint code, bool repeat)
{
    if (m_grab)
        sendKeyEvent(code, WL_KEYBOARD_KEY_STATE_PRESSED, repeat);
    else
        QWaylandKeyboard::sendKeyPressEvent(code, repeat);
}

void WebOSKeyboard::sendKeyReleaseEvent(uint code, bool repeat)
{
    if (m_grab)
        sendKeyEvent(code, WL_KEYBOARD_KEY_STATE_RELEASED, repeat);
    else
        QWaylandKeyboard::sendKeyReleaseEvent(code, repeat);
}

void WebOSKeyboard::addClient(QWaylandClient *client, uint32_t id, uint32_t version)
{
    QWaylandKeyboard::addClient(client, id, version);
}

void WebOSKeyboard::startGrab(KeyboardGrabber *grab)
{
    Q_D(QWaylandKeyboard);
    m_grab = grab;
    m_grab->m_keyboard = d;
    m_grab->m_keyboardPublic = this;
    m_grab->focused(focus());
}

void WebOSKeyboard::endGrab()
{
    Q_D(QWaylandKeyboard);
    m_grab = nullptr;
    setFocus(m_pendingFocus);

    //Modifier state can be changed during grab status.
    //So send it again.
    d->updateModifierState(this);
}

KeyboardGrabber *WebOSKeyboard::currentGrab() const
{
    return m_grab;
}


void WebOSKeyboard::pendingFocusDestroyed(void *data)
{
    Q_UNUSED(data)
    m_pendingFocusDestroyListener.reset();
    m_pendingFocus = nullptr;
}

void WebOSKeyboard::sendKeyEvent(uint code, uint32_t state, bool repeat)
{
    Q_D(QWaylandKeyboard);
    uint32_t time = compositor()->currentTimeMsecs();
    uint32_t serial = compositor()->nextSerial();
    uint key = code - 8;
    m_grab->key(serial, time, key, state);

    d->updateModifierState(code, state, repeat);
}